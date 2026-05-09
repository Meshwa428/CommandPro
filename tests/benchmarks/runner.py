import subprocess
import time
import sys
import os
import argparse
import statistics

SYNAPSE_BIN = "./build/synapse"
PYTHON_EXE = sys.executable
RUNTIME_SCRIPTS_DIR = "tests/benchmarks/scripts"

# --- Python Baselines ---

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
        if i % 2 == 0:
            x += 1
        else:
            x -= 1
        i += 1

def bench_closure_capture():
    def outer():
        x = 10
        def inner():
            return x
        return inner
    f = outer()
    i = 0
    while i < 1000000:
        f()
        i += 1

def bench_dispatch_stress():
    i = 0
    while i < 1000000:
        i += 1

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

def get_mouse_pos(): return (0, 0)
def app_list(): return ["Browser", "Terminal", "Code"]

def bench_native_call_bench():
    i = 0
    while i < 100000:
        get_mouse_pos()
        i += 1

def bench_real_world_auto():
    def findAndClick(targetName):
        apps = app_list()
        i = 0
        while i < len(apps):
            if apps[i] == targetName: return True
            i += 1
        return False
    i = 0
    while i < 1000:
        findAndClick("Browser")
        pos = get_mouse_pos()
        x = pos[0] + 1
        y = pos[1] + 1
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
        if s1 == s2:
            pass
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

# --- Runner ---

def run_synapse_once(script, use_vm=True):
    cmd = [SYNAPSE_BIN, "run", script, "--mock"]
    if use_vm:
        cmd.append("--vm")
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        return -1.0
        
    for line in result.stdout.splitlines():
        if line.startswith("RESULT:"):
            try:
                return float(line.split(":")[1])
            except (IndexError, ValueError):
                continue
                
    return -2.0

def run_python_once(script_name):
    func = PYTHON_BENCHMARKS.get(script_name)
    if not func: return 0.0
    
    start = time.perf_counter()
    func()
    end = time.perf_counter()
    return (end - start) * 1000

def run_benchmark(script_name, path, iterations):
    vm_times = []
    py_times = []
    
    for _ in range(iterations):
        vt = run_synapse_once(path)
        pt = run_python_once(script_name)
        
        if vt < 0: return vt, pt # Error
        vm_times.append(vt)
        py_times.append(pt)
        
    avg_vm = statistics.mean(vm_times)
    avg_py = statistics.mean(py_times)
    return avg_vm, avg_py

def main():
    parser = argparse.ArgumentParser(description="Synapse Runtime Benchmark Runner")
    parser.add_argument("-n", "--iterations", type=int, default=5, help="Number of iterations for each benchmark (default: 5)")
    args = parser.parse_args()
    
    scripts = [f for f in os.listdir(RUNTIME_SCRIPTS_DIR) if f.endswith(".syn")]
    scripts.sort()
    
    print(f"🚀 Running Runtime Systems Benchmarks ({args.iterations} iterations per test)")
    print(f"Comparing Synapse VM vs. CPython Baseline (Averages)")
    print("-" * 85)
    print(f"{'Benchmark':<30} | {'VM Avg (ms)':>15} | {'Py Avg (ms)':>15} | {'Ratio':>10}")
    print("-" * 85)
    
    for script_name in scripts:
        path = os.path.join(RUNTIME_SCRIPTS_DIR, script_name)
        vm_time, py_time = run_benchmark(script_name, path, args.iterations)
        
        if vm_time == -1.0:
            print(f"{script_name:<30} | {'FAILED':>15} | {py_time:>15.4f} | {'N/A':>10}")
        elif vm_time == -2.0:
            print(f"{script_name:<30} | {'NO RESULT':>15} | {py_time:>15.4f} | {'N/A':>10}")
        else:
            ratio = vm_time / py_time if py_time > 0 else 0
            print(f"{script_name:<30} | {vm_time:>15.4f} | {py_time:>15.4f} | {ratio:>9.2f}x")
    
    print("-" * 85)

if __name__ == "__main__":
    main()
