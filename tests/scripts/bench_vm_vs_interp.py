import time
import subprocess
import os

SYNAPSE_BIN = "./build/synapse"
FIB_SCRIPT = "tests/test_vm_fib.syn"

def run_bench(use_vm):
    args = [SYNAPSE_BIN, "run", FIB_SCRIPT]
    if use_vm: args.append("--vm")
    
    start = time.perf_counter()
    subprocess.run(args, capture_output=True)
    return (time.perf_counter() - start) * 1000

print(f"Tree-Walker: {run_bench(False):.2f}ms")
print(f"Bytecode VM: {run_bench(True):.2f}ms")
