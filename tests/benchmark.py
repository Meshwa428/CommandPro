import subprocess
import time
import sys
import os
import re

# Configurations
SYNAPSE_BIN = "./build/synapse"
STRESS_TEST = "tests/stress_test.syn"
PYTHON_EXE = sys.executable

def run_synapse(script, use_vm=False):
    cmd = [SYNAPSE_BIN, "run", script, "--mock"]
    if use_vm:
        cmd.append("--vm")
    
    start = time.perf_counter()
    try:
        # We use a timeout to catch the VM hangs
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        end = time.perf_counter()
        
        return {
            "output": result.stdout,
            "error": result.stderr,
            "exit_code": result.returncode,
            "total_time_ms": (end - start) * 1000
        }
    except subprocess.TimeoutExpired:
        return {"error": "TIMEOUT", "total_time_ms": 15000, "exit_code": -1, "output": ""}
    except Exception as e:
        return {"error": str(e), "total_time_ms": 0, "exit_code": -1, "output": ""}

def parse_stress_results(output):
    sections = {}
    current_section = None
    
    for line in output.splitlines():
        if line.startswith("---"):
            # Extract section name: "--- 1. Arithmetic & Loop Stress (100,000 iterations) ---"
            # We want to match exactly what's between "--- " and " ("
            match = re.search(r"--- (.*?) (\(|---)", line)
            if match:
                current_section = match.group(1).strip()
            else:
                current_section = line.strip("- ").strip()
            sections[current_section] = {"time": None, "result": None}
        elif "Time: " in line and current_section:
            time_match = re.search(r"Time: ([\d\.\+e\-]+)ms", line)
            if time_match:
                sections[current_section]["time"] = float(time_match.group(1))
        elif "Result: " in line and current_section:
            sections[current_section]["result"] = line.split("Result: ")[1].strip()
            
    return sections

def run_python_baseline():
    code = """
import time
import sys

def fib(n):
    if n < 2: return n
    return fib(n - 1) + fib(n - 2)

def run():
    results = {}
    
    # 1. Arithmetic & Loop Stress
    start = time.perf_counter()
    s, i = 0, 0
    while i < 100000:
        s += i
        i += 1
    results["1. Arithmetic & Loop Stress"] = (time.perf_counter() - start) * 1000
    
    # 2. Function Call & Recursion Stress
    start = time.perf_counter()
    fib(20)
    results["2. Function Call & Recursion Stress"] = (time.perf_counter() - start) * 1000
    
    # 3. String Concatenation Stress
    start = time.perf_counter()
    s = ""
    for _ in range(5000): s += "a"
    results["3. String Concatenation Stress"] = (time.perf_counter() - start) * 1000

    # 4. Typed Variable Coercion Stress
    start = time.perf_counter()
    x = 0
    for _ in range(50000):
        x = int(10.5)
    results["4. Typed Variable Coercion Stress"] = (time.perf_counter() - start) * 1000

    # 5. Tuple Creation Stress
    start = time.perf_counter()
    for l in range(5000):
        t = (l, l + 1, l + 2)
    results["5. Tuple Creation Stress"] = (time.perf_counter() - start) * 1000

    # 6. Deeply Nested Scopes
    def nested_scopes(depth):
        if depth > 0:
            x = depth
            return nested_scopes(depth - 1)
        return 0
    sys.setrecursionlimit(2000)
    start = time.perf_counter()
    nested_scopes(1000)
    results["6. Deeply Nested Scopes"] = (time.perf_counter() - start) * 1000
    
    # 7. High-Frequency Function Calls
    def add_one(n): return n + 1
    start = time.perf_counter()
    t, c = 0, 0
    while c < 100000:
        t = add_one(t)
        c += 1
    results["7. High-Frequency Function Calls"] = (time.perf_counter() - start) * 1000
    
    # 8. Large List Literal Creation
    start = time.perf_counter()
    large_list = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9] * 50
    results["8. Large List Literal Creation"] = (time.perf_counter() - start) * 1000

    # 9. Complex Expression
    start = time.perf_counter()
    m = 0
    while m < 100000:
        res = (1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10) * (11 - 1) / 2 + (m % 100) - (m // 10)
        m += 1
    results["9. Complex Expression"] = (time.perf_counter() - start) * 1000

    # 10. Deep Recursion
    start = time.perf_counter()
    fib(25)
    results["10. Deep Recursion"] = (time.perf_counter() - start) * 1000
    
    # 11. Large List Iteration
    lst = list(range(1, 11))
    start = time.perf_counter()
    idx, tot = 0, 0
    while idx < 10000:
        tot += lst[idx % 10]
        idx += 1
    results["11. Large List Iteration"] = (time.perf_counter() - start) * 1000
    
    for k, v in results.items():
        print(f"{k}: {v:.8f}ms")

run()
    """
    result = subprocess.run([PYTHON_EXE, "-c", code], capture_output=True, text=True)
    
    benchmarks = {}
    for line in result.stdout.splitlines():
        if ":" in line:
            name, t = line.split(": ")
            benchmarks[name.strip()] = float(t.replace("ms", ""))
    return benchmarks

def main():
    print("🚀 Starting High-Precision Unified Benchmark Suite...")
    
    print("\n[1/3] Running Python Baseline...")
    py_results = run_python_baseline()
    
    print("[2/3] Running Tree-Walker...")
    tree_raw = run_synapse(STRESS_TEST, use_vm=False)
    tree_results = parse_stress_results(tree_raw.get("output", ""))
    
    print("[3/3] Running Bytecode VM...")
    vm_raw = run_synapse(STRESS_TEST, use_vm=True)
    vm_results = parse_stress_results(vm_raw.get("output", ""))
    
    print("\n" + "="*110)
    print(f"{'Benchmark Case':<45} | {'Tree':>15} | {'VM':>15} | {'Python':>15}")
    print("-" * 110)
    
    all_cases = sorted(set(list(tree_results.keys()) + list(vm_results.keys()) + list(py_results.keys())))
    
    def format_val(val):
        if val is None or val == "N/A": return "N/A"
        if val < 0.001: return f"{val*1000000:>9.2f} ns"
        if val < 1.0:   return f"{val*1000:>9.2f} µs"
        return f"{val:>9.4f} ms"

    for case in all_cases:
        tree_t = tree_results.get(case, {}).get("time", "N/A")
        vm_t = vm_results.get(case, {}).get("time", "N/A")
        py_t = py_results.get(case, "N/A")
        
        print(f"{case:<45} | {format_val(tree_t):>15} | {format_val(vm_t):>15} | {format_val(py_t):>15}")

    print("="*110)

    if vm_raw["exit_code"] != 0:
        print("\n⚠️  VM Warning: The VM run failed or timed out.")
        if vm_raw["error"]:
            print(f"VM Error Log: {vm_raw['error'].strip()[:200]}...")
    
    if tree_raw["exit_code"] != 0:
        print("\n⚠️  Tree-Walker Warning: The run failed.")
        if tree_raw["error"]:
            print(f"Tree-Walker Error Log: {tree_raw['error'].strip()[:200]}...")

if __name__ == "__main__":
    main()
