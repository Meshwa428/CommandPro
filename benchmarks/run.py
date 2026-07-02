#!/usr/bin/env python3
"""Synapse vs Python vs C++ benchmark harness.

Usage:
    python3 benchmarks/run.py [--release] [benchmark_name ...]

    --release  use the release build of syn (default: debug)

    If no benchmark names given, runs all benchmarks.
"""
import os
import sys
import subprocess
import statistics
import time

BENCHMARKS = [
    # original 12
    "fib", "loop", "primes", "strings", "list_ops",
    "float_arith", "closures", "matrix_mul", "quicksort", "higher_order", "memoize",
    "string_ops",
    # recursive / call overhead
    "tak", "ackermann",
    # numeric / scientific
    "nbody", "mandelbrot", "spectral_norm", "monte_carlo_pi",
    # memory / GC
    "binary_trees", "linked_list",
    # arrays / memory access
    "sieve",
    # string processing
    "csv_parse",
    # 2D DP
    "levenshtein", "knapsack", "lcs",
    # sorting
    "mergesort", "heapsort",
    # hash map
    "hash_stress",
]
ROUNDS = 5

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR   = os.path.dirname(SCRIPT_DIR)
CPP_SRC    = os.path.join(SCRIPT_DIR, "cpp")
CPP_BIN    = os.path.join(REPO_DIR, "build", "cpp_bench")


def time_cmd(cmd: list[str], rounds: int) -> list[float]:
    times = []
    for _ in range(rounds):
        start = time.perf_counter()
        r = subprocess.run(cmd, capture_output=True)
        end = time.perf_counter()
        if r.returncode != 0:
            print(f"  ERROR: {r.stderr.decode().strip()[:200]}", file=sys.stderr)
            return []
        times.append(end - start)
    return times


def compile_cpp(name: str) -> str | None:
    src = os.path.join(CPP_SRC, f"{name}.cpp")
    if not os.path.exists(src):
        return None
    os.makedirs(CPP_BIN, exist_ok=True)
    out = os.path.join(CPP_BIN, name)
    r = subprocess.run(
        ["g++", "-O3", "-std=c++17", "-o", out, src],
        capture_output=True
    )
    if r.returncode != 0:
        print(f"  C++ compile error ({name}): {r.stderr.decode().strip()[:200]}", file=sys.stderr)
        return None
    return out


def fmt_ratio(ratio: float) -> str:
    if ratio >= 1.0:
        return f"x{ratio:.2f} faster"
    else:
        return f"x{1/ratio:.2f} slower"


def main():
    use_release = "--release" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    benchmarks = args if args else BENCHMARKS

    build_preset = "release" if use_release else "debug"
    syn_bin = os.path.join(REPO_DIR, "build", build_preset, "syn")

    print(f"Building syn ({build_preset})...")
    build = subprocess.run(
        ["cmake", "--build", "--preset", build_preset],
        cwd=REPO_DIR, capture_output=True
    )
    if build.returncode != 0:
        print(build.stderr.decode().strip(), file=sys.stderr)
        sys.exit(1)
    print("Build OK\n")

    # Pre-compile all C++ benchmarks
    cpp_bins: dict[str, str | None] = {}
    for name in benchmarks:
        cpp_bins[name] = compile_cpp(name)

    # Table layout
    N  = 14   # benchmark name
    T  = 9    # time columns
    R  = 14   # ratio columns
    header = (f"{'Benchmark':<{N}} {'C++(ms)':>{T}} {'Syn(ms)':>{T}} {'Py(ms)':>{T}}"
              f"  {'Syn vs C++':>{R}}  {'Py vs C++':>{R}}  {'Syn vs Py':>{R}}")
    sep = "-" * len(header)

    print(sep)
    print(header)
    print(sep)

    for name in benchmarks:
        syn_file = os.path.join(SCRIPT_DIR, f"{name}.syn")
        py_file  = os.path.join(SCRIPT_DIR, f"{name}.py")
        cpp_bin  = cpp_bins.get(name)

        syn_times = time_cmd([syn_bin, syn_file], ROUNDS) if os.path.exists(syn_file) else []
        py_times  = time_cmd([sys.executable, py_file], ROUNDS) if os.path.exists(py_file) else []
        cpp_times = time_cmd([cpp_bin], ROUNDS) if cpp_bin else []

        syn_ms = statistics.median(syn_times) * 1000 if syn_times else None
        py_ms  = statistics.median(py_times)  * 1000 if py_times  else None
        cpp_ms = statistics.median(cpp_times) * 1000 if cpp_times else None

        def ms_str(v): return f"{v:.2f}" if v is not None else "N/A"
        def ratio_str(a, b):  # a relative to b (positive = a is faster than b)
            if a is None or b is None: return "N/A"
            r = b / a
            return f"x{r:.2f} faster" if r >= 1 else f"x{1/r:.2f} slower"

        print(f"{name:<{N}} {ms_str(cpp_ms):>{T}} {ms_str(syn_ms):>{T}} {ms_str(py_ms):>{T}}"
              f"  {ratio_str(cpp_ms, None) if cpp_ms is None else ratio_str(syn_ms, cpp_ms):>{R}}"
              f"  {ratio_str(py_ms, cpp_ms):>{R}}"
              f"  {ratio_str(syn_ms, py_ms):>{R}}")

    print(sep)
    print(f"Median of {ROUNDS} runs. Build: {build_preset}. Python: {sys.version.split()[0]}")


if __name__ == "__main__":
    main()
