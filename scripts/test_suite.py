import subprocess
import os
import sys
import time
import statistics
import argparse
import hashlib
import random

# Configuration
SYNAPSE_BIN = "./build/synapse"
PYTHON_EXE = sys.executable
CONFORMANCE_DIR = "tests/conformance"
BENCHMARK_DIR = "tests/benchmarks/scripts"

# --- Colors ---
GREEN = '\033[0;32m'
BLUE = '\033[0;34m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
BOLD = '\033[1m'
NC = '\033[0m'

# --- Python Baselines for Benchmarking ---
def bench_alloc_pressure():
    i = 0
    while i < 10000:
        x = [1, 2, 3, 4, 5]
        y = (1, 2, 3)
        z = {"a": 1, "b": 2}
        i += 1

def bench_branch_stress():
    i = 0
    x = 0
    while i < 100000:
        if i % 2 == 0: x += 1
        else: x -= 1
        i += 1

def bench_closure_capture():
    def outer():
        x = 10
        def inner(): return x
        return inner
    f = outer()
    i = 0
    while i < 1000000:
        f()
        i += 1

def bench_dispatch_stress():
    i = 0
    while i < 1000000: i += 1

global_counter = 0
def bench_global_bench():
    global global_counter
    def test():
        global global_counter
        global_counter += 1
    i = 0
    while i < 1000000:
        test()
        i += 1

def bench_map_stress():
    m = {}
    i = 0
    while i < 10000:
        m["key_" + str(i)] = i
        i += 1
    i = 0
    while i < 10000:
        x = m.get("key_" + str(i))
        i += 1

def bench_native_call_bench():
    i = 0
    while i < 100000: i += 1

def bench_real_world_auto():
    def findAndClick(targetName):
        apps = ["Browser", "Terminal", "Code"]
        i = 0
        while i < len(apps):
            if apps[i] == targetName: return True
            i += 1
        return False
    i = 0
    while i < 1000:
        findAndClick("Browser")
        i += 1

def bench_stack_churn():
    i = 0
    while i < 100000:
        x = (((((i + 1) * 2) - 3) / 4) % 5)
        i += 1

def bench_string_equality():
    i = 0
    while i < 1000000:
        s1 = "hello_world"
        s2 = "hello_world"
        if s1 == s2: pass
        i += 1

PYTHON_BENCHMARKS = {
    "alloc_pressure.syn": bench_alloc_pressure,
    "branch_stress.syn": bench_branch_stress,
    "closure_capture.syn": bench_closure_capture,
    "dispatch_stress.syn": bench_dispatch_stress,
    "global_bench.syn": bench_global_bench,
    "map_stress.syn": bench_map_stress,
    "native_call_bench.syn": bench_native_call_bench,
    "real_world_auto.syn": bench_real_world_auto,
    "stack_churn.syn": bench_stack_churn,
    "string_equality.syn": bench_string_equality,
}

# --- Core Utility Functions ---

def run_synapse(script, use_vm=True, extra_args=None):
    cmd = [SYNAPSE_BIN, "run", script, "--mock"]
    if use_vm: cmd.append("--vm")
    if extra_args: cmd.extend(extra_args)
    
    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=False)
    end = time.perf_counter()
    
    stdout = result.stdout.decode('utf-8', errors='replace')
    stderr = result.stderr.decode('utf-8', errors='replace')
    
    exec_time = (end - start) * 1000 # Default fallback
    
    # Try to get internal RESULT: time if available
    for line in stdout.splitlines():
        if line.startswith("RESULT:"):
            try: exec_time = float(line.split(":")[1])
            except: pass
            
    # Create a mock object for compatibility
    class MockResult:
        def __init__(self, returncode, stdout, stderr):
            self.returncode = returncode
            self.stdout = stdout
            self.stderr = stderr
            
    return MockResult(result.returncode, stdout, stderr), exec_time

def run_python_bench(script_name):
    func = PYTHON_BENCHMARKS.get(script_name)
    if not func: return 0.0
    start = time.perf_counter()
    func()
    end = time.perf_counter()
    return (end - start) * 1000

# --- Test Discovery and Metadata ---

def parse_test_metadata(file_path):
    """
    Parses a .syn file for @EXPECT tags.
    Format:
    # @EXPECT EXIT <int>
    # @EXPECT STDOUT "<string>"
    # @EXPECT STDERR "<string>"
    """
    expectations = {
        'exit': 0,
        'stdout': [],
        'stderr': [],
        'skip_vm': False
    }
    
    if not os.path.exists(file_path):
        return expectations
        
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line.startswith('# @EXPECT'):
                    continue
                    
                parts = line.split(None) # ['#', '@EXPECT', 'TAG', ...]
                if len(parts) < 3:
                    continue
                    
                tag_type = parts[2].upper()
                tag_value = " ".join(parts[3:]).strip() if len(parts) > 3 else ""
                
                # Handle quote stripping for values
                if (tag_value.startswith('"') and tag_value.endswith('"')) or \
                   (tag_value.startswith("'") and tag_value.endswith("'")):
                    tag_value = tag_value[1:-1]
                
                if tag_type == 'EXIT':
                    try: expectations['exit'] = int(tag_value)
                    except: pass
                elif tag_type == 'STDOUT':
                    expectations['stdout'].append(tag_value)
                elif tag_type == 'STDERR':
                    expectations['stderr'].append(tag_value)
                elif tag_type == 'SKIP_VM':
                    expectations['skip_vm'] = True
    except Exception as e:
        print(f"Error parsing metadata for {file_path}: {e}")
                
    return expectations

def discover_tests(directories):
    """Recursively find all .syn files in the given directories."""
    test_files = []
    for d in directories:
        if not os.path.exists(d): continue
        for root, _, files in os.walk(d):
            for f in files:
                if f.endswith('.syn'):
                    full_path = os.path.join(root, f)
                    # Store as (suite_name, rel_path, full_path)
                    suite_name = os.path.basename(d)
                    rel_path = os.path.relpath(full_path, d)
                    test_files.append((suite_name, rel_path, full_path))
    return sorted(test_files)

# --- Subcommand Handlers ---

def mode_conformance(args, target_dirs=None):
    if target_dirs is None:
        target_dirs = [CONFORMANCE_DIR, "tests/functional"]
        
    print(f"\n{BLUE}{BOLD}🚀 Running Synapse Test Suite (Automated Discovery){NC}")
    print("-" * 75)
    
    test_info = discover_tests(target_dirs)
    
    passed = 0
    failed = 0
    
    for suite, rel_path, full_path in test_info:
        expectations = parse_test_metadata(full_path)
        
        code = expectations['exit']
        out = expectations['stdout']
        err = expectations['stderr']
        
        if args.vm and expectations['skip_vm']:
            print(f"{YELLOW}⚠️ SKIP: [{suite}] {rel_path:<35}{NC} | VM UNSUPPORTED")
            continue
            
        res, _ = run_synapse(full_path, use_vm=args.vm)
        
        test_failed = False
        errors = []
        
        if res.returncode != code:
            test_failed = True
            errors.append(f"Exit code mismatch: expected {code}, got {res.returncode}")
            
        if out:
            for s in out:
                if s not in res.stdout:
                    test_failed = True
                    errors.append(f"Stdout missing: '{s}'")
                    
        if err:
            for s in err:
                if s not in res.stderr:
                    test_failed = True
                    errors.append(f"Stderr missing: '{s}'")
                    
        if test_failed:
            print(f"{RED}❌ FAIL: [{suite}] {rel_path:<35}{NC} | ERROR")
            for e in errors: print(f"   - {e}")
            if args.verbose:
                print(f"--- STDOUT ---\n{res.stdout}\n--- STDERR ---\n{res.stderr}")
            failed += 1
        else:
            print(f"{GREEN}✅ PASS: [{suite}] {rel_path:<35}{NC} | OK")
            passed += 1
            
    print("-" * 75)
    print(f"TOTAL: {passed + failed} | {GREEN}PASSED: {passed}{NC} | {RED}FAILED: {failed}{NC}")
    return failed == 0

def mode_benchmark(args):
    print(f"\n{BLUE}{BOLD}🚀 Running Performance Benchmarks (n={args.iterations}){NC}")
    print(f"Comparing Synapse {'VM' if args.vm else 'Interp'} vs. CPython Baseline")
    print("-" * 85)
    print(f"{'Benchmark':<30} | {'Syn Avg (ms)':>15} | {'Py Avg (ms)':>15} | {'Ratio':>10}")
    print("-" * 85)
    
    test_info = discover_tests([BENCHMARK_DIR])
    
    for suite, rel_path, full_path in test_info:
        expectations = parse_test_metadata(full_path)
        if args.vm and expectations['skip_vm']:
            print(f"{rel_path:<30} | {YELLOW}{'SKIPPED':>15}{NC} | {'N/A':>15} | {'N/A':>10}")
            continue
            
        syn_times = []
        py_times = []
        
        failed = False
        for _ in range(args.iterations):
            res, t = run_synapse(full_path, use_vm=args.vm)
            if res.returncode != 0:
                failed = True
                break
            syn_times.append(t)
            py_times.append(run_python_bench(rel_path))
            
        if failed:
            print(f"{rel_path:<30} | {RED}{'FAILED':>15}{NC} | {'N/A':>15} | {'N/A':>10}")
            continue
            
        avg_syn = statistics.mean(syn_times)
        avg_py = statistics.mean(py_times)
        ratio = avg_syn / avg_py if avg_py > 0 else 0
        
        color = GREEN if ratio < 1.0 else YELLOW if ratio < 5.0 else RED
        print(f"{rel_path:<30} | {avg_syn:>15.4f} | {avg_py:>15.4f} | {color}{ratio:>9.2f}x{NC}")
        
    print("-" * 85)

def mode_stability(args):
    print(f"\n{BLUE}{BOLD}🧐 Checking Stability & Determinism (n={args.iterations}){NC}")
    script = "tests/functional/scenarios/stress_test.syn"
    if not os.path.exists(script):
        print(f"{RED}Error: stress_test.syn not found.{NC}")
        return
        
    print(f"Target: {script}")
    
    hashes = set()
    for i in range(args.iterations):
        res, _ = run_synapse(script, use_vm=args.vm)
        # Filter out non-deterministic lines (like those with execution times)
        filtered_lines = []
        for line in res.stdout.splitlines():
            if "Time:" in line or "ms" in line or line.startswith("RESULT:"):
                continue
            filtered_lines.append(line)
        
        filtered_stdout = "\n".join(filtered_lines)
        h = hashlib.sha256(filtered_stdout.encode()).hexdigest()
        hashes.add(h)
        if len(hashes) > 1:
            print(f"{RED}❌ STABILITY FAILURE at run {i+1}!{NC}")
            if args.verbose:
                print(f"Diff detected. Current Output:\n{filtered_stdout}")
            return False
            
    print(f"{GREEN}✅ STABILITY OK: {args.iterations}/{args.iterations} runs matched.{NC}")
    return True

def mode_fuzz(args):
    print(f"\n{BLUE}{BOLD}👾 Running Syntax Fuzzer (n={args.iterations}){NC}")
    
    chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \n\t+-*/=(){}[],;\"'#!"
    def generate_fuzz(length):
        return ''.join(random.choice(chars) for _ in range(length))
        
    fuzz_file = "fuzz_temp.syn"
    crashes = 0
    
    for i in range(args.iterations):
        content = generate_fuzz(random.randint(10, 200))
        with open(fuzz_file, "w") as f:
            f.write(content)
            
        # Run with a short timeout to prevent infinite loops from hanging the test suite
        try:
            cmd = [SYNAPSE_BIN, "run", fuzz_file, "--mock", "--vm"]
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=1.0)
            # We expect ParseError or LexerError, NOT Segmentation Fault (-11)
            if res.returncode == -11 or res.returncode == 139:
                print(f"{RED}💥 CRASH (Segfault) detected! Iteration {i+1}{NC}")
                print(f"Content:\n{content}")
                crashes += 1
        except subprocess.TimeoutExpired:
            pass # Timeout is fine
        except Exception as e:
            print(f"Error: {e}")
            
    if os.path.exists(fuzz_file): os.remove(fuzz_file)
    
    if crashes == 0:
        print(f"{GREEN}✅ FUZZING OK: {args.iterations} iterations, 0 crashes.{NC}")
    else:
        print(f"{RED}❌ FUZZING FAILED: {crashes} crashes detected.{NC}")
    return crashes == 0

# --- Main Entry Point ---

def main():
    parser = argparse.ArgumentParser(description="Synapse Unified Verification Tool")
    parser.add_argument("mode", nargs="?", default="conformance", choices=["conformance", "benchmark", "fuzz", "stability", "all", "functional"],
                        help="Execution mode (default: conformance)")
    parser.add_argument("--vm", action="store_true", default=True, help="Use Bytecode VM (default: True)")
    parser.add_argument("--interp", action="store_false", dest="vm", help="Use Tree-walking Interpreter")
    parser.add_argument("-n", "--iterations", type=int, default=5, help="Number of iterations for bench/fuzz/stability")
    parser.add_argument("-v", "--verbose", action="store_true", help="Detailed output on failures")
    parser.add_argument("--suite", type=str, help="Specific suite directory to run")
    
    args = parser.parse_args()
    
    if not os.path.exists(SYNAPSE_BIN):
        print(f"{RED}Error: {SYNAPSE_BIN} not found. Please compile first.{NC}")
        sys.exit(1)
        
    success = True
    
    if args.mode in ["conformance", "all", "functional"]:
        target_dirs = None
        if args.suite:
            target_dirs = [args.suite]
        elif args.mode == "functional":
            target_dirs = ["tests/functional"]
        elif args.mode == "conformance":
            target_dirs = [CONFORMANCE_DIR]
            
        if not mode_conformance(args, target_dirs): success = False
        
    if args.mode in ["benchmark", "all"]:
        mode_benchmark(args)
        
    if args.mode in ["stability", "all"]:
        if not mode_stability(args): success = False
        
    if args.mode in ["fuzz", "all"]:
        if not mode_fuzz(args): success = False
        
    if not success:
        sys.exit(1)

if __name__ == "__main__":
    main()
