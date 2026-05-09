import subprocess
import time
import sys
import os
import json

SYNAPSE_BIN = "./build/synapse"
PYTHON_EXE = sys.executable
RUNTIME_SCRIPTS_DIR = "tests/scripts/runtime"

def run_bench(script, use_vm=True):
    cmd = [SYNAPSE_BIN, "run", script, "--mock"]
    if use_vm:
        cmd.append("--vm")
    
    start = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end = time.perf_counter()
    
    return (end - start) * 1000

def main():
    scripts = [f for f in os.listdir(RUNTIME_SCRIPTS_DIR) if f.endswith(".syn")]
    scripts.sort()
    
    print(f"🚀 Running Runtime Systems Benchmarks...")
    print("-" * 60)
    print(f"{'Benchmark':<30} | {'VM Time (ms)':>15}")
    print("-" * 60)
    
    for script_name in scripts:
        path = os.path.join(RUNTIME_SCRIPTS_DIR, script_name)
        vm_time = run_bench(path, use_vm=True)
        print(f"{script_name:<30} | {vm_time:>15.4f}")
    
    print("-" * 60)

if __name__ == "__main__":
    main()
