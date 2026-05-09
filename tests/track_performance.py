import time
import subprocess
import os
import csv
import sys
from datetime import datetime

sys.setrecursionlimit(2000)

CSV_FILE = "tests/performance_history.csv"
SYNAPSE_SCRIPT = "tests/stress_test.syn"
ITERATIONS = 5 # Run multiple times to average and reduce margin of error

def run_python_benchmark():
    # 1. Arithmetic Loop (100k)
    start = time.perf_counter()
    i = 0
    s_val = 0
    while i < 100000:
        s_val = s_val + i
        i = i + 1
    loop_t = (time.perf_counter() - start) * 1000

    # 2. Fib 20
    def fib(n):
        if n < 2: return n
        return fib(n-1) + fib(n-2)
    start = time.perf_counter()
    fib(20)
    fib_t = (time.perf_counter() - start) * 1000

    # 3. Coercion (50k)
    start = time.perf_counter()
    k = 0
    while k < 50000:
        x = 10.5
        k = k + 1
    coerce_t = (time.perf_counter() - start) * 1000

    # 4. Tuple Creation (5k)
    start = time.perf_counter()
    l = 0
    while l < 5000:
        t = (l, l + 1, l + 2)
        l = l + 1
    tuple_t = (time.perf_counter() - start) * 1000

    # 6. Deeply Nested Scopes (1,000 calls)
    start = time.perf_counter()
    def nested_scopes(depth):
        if depth > 0:
            x = depth
            return nested_scopes(depth - 1)
        return 0
    nested_scopes(1000)
    nested_t = (time.perf_counter() - start) * 1000

    # 7. High-Frequency Function Calls (100,000 calls)
    def add_one(n):
        return n + 1
    start = time.perf_counter()
    total = 0
    counter = 0
    while counter < 100000:
        total = add_one(total)
        counter = counter + 1
    fn_call_t = (time.perf_counter() - start) * 1000

    # 8. Large List Literal Creation (500 elements)
    start = time.perf_counter()
    large_list = [
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
    ]
    list_t = (time.perf_counter() - start) * 1000

    # 9. Complex Expression (Nested arithmetic, 100,000 iterations)
    start = time.perf_counter()
    m = 0
    while m < 100000:
        res = (1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10) * (11 - 1) / 2 + (m % 100) - (m // 10)
        m = m + 1
    expr_t = (time.perf_counter() - start) * 1000

    # 10. Deep Recursion (Fibonacci 25)
    start = time.perf_counter()
    fib(25)
    fib25_t = (time.perf_counter() - start) * 1000

    # 11. Large List Iteration (10,000 accesses)
    lst = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    start = time.perf_counter()
    idx = 0
    total_list = 0
    while idx < 10000:
        total_list = total_list + lst[idx % 10]
        idx = idx + 1
    list_iter_t = (time.perf_counter() - start) * 1000

    return loop_t, fib_t, coerce_t, tuple_t, nested_t, fn_call_t, list_t, expr_t, fib25_t, list_iter_t

def run_synapse_benchmark():
    result = subprocess.run(["./build/synapse", "run", SYNAPSE_SCRIPT, "--mock"], capture_output=True, text=True)
    lines = result.stdout.split('\n')
    times = []
    for line in lines:
        if "Time:" in line:
            t_str = line.split("Time: ")[1].replace("ms", "")
            try:
                times.append(float(t_str))
            except ValueError:
                continue
    # times order: 
    # 1. Loop(100k), 2. Fib(20), 3. Concat(5k), 4. Coerce(50k), 5. Tuple(5k),
    # 6. Nested(1k), 7. FnCalls(100k), 8. List(500), 9. ComplexExpr(100k),
    # 10. Fib(25), 11. ListIter(10k)
    if len(times) < 11: return [0]*10
    # Returning in order matching Python benchmark
    return [times[0], times[1], times[3], times[4], times[5], times[6], times[7], times[8], times[9], times[10]]

def track():
    print(f"Starting benchmark ({ITERATIONS} iterations)...")
    
    syn_results = []
    py_results = []
    
    for i in range(ITERATIONS):
        syn_results.append(run_synapse_benchmark())
        py_results.append(run_python_benchmark())
    
    # Average results
    avg_syn = [sum(x)/ITERATIONS for x in zip(*syn_results)]
    avg_py = [sum(x)/ITERATIONS for x in zip(*py_results)]
    
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    file_exists = os.path.isfile(CSV_FILE)
    with open(CSV_FILE, 'a', newline='') as f:
        writer = csv.writer(f)
        if not file_exists:
            writer.writerow([
                "Timestamp", 
                "Syn_Loop_100k", "Syn_Fib_20", "Syn_Coerce_50k", "Syn_Tuple_5k", "Syn_Nested_1k", "Syn_FnCall_100k", "Syn_List_500", "Syn_Expr_100k", "Syn_Fib_25", "Syn_ListIter_10k",
                "Py_Loop_100k", "Py_Fib_20", "Py_Coerce_50k", "Py_Tuple_5k", "Py_Nested_1k", "Py_FnCall_100k", "Py_List_500", "Py_Expr_100k", "Py_Fib_25", "Py_ListIter_10k"
            ])
        writer.writerow([timestamp] + avg_syn + avg_py)
    
    print(f"Results saved to {CSV_FILE}")
    print(f"--- COMPARISON (Synapse vs Python) ---")
    print(f"Arithmetic (100k): {avg_syn[0]:.2f}ms vs {avg_py[0]:.2f}ms")
    print(f"Fibonacci (20):    {avg_syn[1]:.2f}ms vs {avg_py[1]:.2f}ms")
    print(f"Fibonacci (25):    {avg_syn[8]:.2f}ms vs {avg_py[8]:.2f}ms")
    print(f"Tuple (5k):        {avg_syn[3]:.2f}ms vs {avg_py[3]:.2f}ms")
    print(f"Nested Scopes (1k):{avg_syn[4]:.2f}ms vs {avg_py[4]:.2f}ms")
    print(f"Fn Calls (100k):   {avg_syn[5]:.2f}ms vs {avg_py[5]:.2f}ms")
    print(f"Large List (500):  {avg_syn[6]:.2f}ms vs {avg_py[6]:.2f}ms")
    print(f"List Iter (10k):   {avg_syn[9]:.2f}ms vs {avg_py[9]:.2f}ms")
    print(f"Complex Expr (100k):{avg_syn[7]:.2f}ms vs {avg_py[7]:.2f}ms")

if __name__ == "__main__":
    track()
