import os
import sys

# Move to the project root if needed
os.chdir("/home/meshwa/Documents/Projects/CommandPro")

# The functions we want to extract
baselines = {
    "alloc_pressure": """
def run():
    i = 0
    while i < 10000:
        x = [1, 2, 3, 4, 5]
        y = (1, 2, 3)
        z = {"a": 1, "b": 2}
        i += 1
""",
    "branch_stress": """
def run():
    i = 0
    x = 0
    while i < 100000:
        if i % 2 == 0: x += 1
        else: x -= 1
        i += 1
""",
    "closure_capture": """
def run():
    def outer():
        x = 10
        def inner(): return x
        return inner
    f = outer()
    i = 0
    while i < 1000000:
        f()
        i += 1
""",
    "dispatch_stress": """
def run():
    i = 0
    while i < 1000000: i += 1
""",
    "global_bench": """
global_counter = 0
def run():
    global global_counter
    def test():
        global global_counter
        global_counter += 1
    i = 0
    while i < 1000000:
        test()
        i += 1
""",
    "map_stress": """
def run():
    m = {}
    i = 0
    while i < 10000:
        m["key_" + str(i)] = i
        i += 1
    i = 0
    while i < 10000:
        x = m.get("key_" + str(i))
        i += 1
""",
    "native_call_bench": """
def run():
    i = 0
    while i < 100000: i += 1
""",
    "real_world_auto": """
def run():
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
""",
    "stack_churn": """
def run():
    i = 0
    while i < 100000:
        x = (((((i + 1) * 2) - 3) / 4) % 5)
        i += 1
""",
    "string_equality": """
def run():
    i = 0
    while i < 1000000:
        s1 = "hello_world"
        s2 = "hello_world"
        if s1 == s2: pass
        i += 1
""",
    "stress_1_arithmetic": """
def run():
    i, sum_val = 0, 0
    while i < 100000:
        sum_val += i
        i += 1
""",
    "stress_2_fib20": """
def run():
    def fib(n):
        if n < 2: return n
        return fib(n-1) + fib(n-2)
    fib(20)
""",
    "stress_3_string_concat": """
def run():
    s, j = "", 0
    while j < 5000:
        s += "a"
        j += 1
""",
    "stress_4_coercion": """
def run():
    k = 0
    while k < 50000:
        x = 10
        k += 1
""",
    "stress_5_tuples": """
def run():
    l = 0
    while l < 5000:
        t = (l, l + 1, l + 2)
        l += 1
""",
    "stress_6_nested_scopes": """
import sys
sys.setrecursionlimit(2000)
def run():
    def nested(depth):
        if depth > 0: return nested(depth - 1)
        return 0
    nested(1000)
""",
    "stress_7_func_calls": """
def run():
    def add_one(n): return n + 1
    total, counter = 0, 0
    while counter < 100000:
        total = add_one(total)
        counter += 1
""",
    "stress_8_list_literal": """
def run():
    lst = [0] * 500
""",
    "stress_9_complex_expr": """
def run():
    m = 0
    while m < 100000:
        res = (1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10) * (11 - 1) / 2 + (m % 100) - (m // 10)
        m += 1
""",
    "stress_10_fib25": """
def run():
    def fib(n):
        if n < 2: return n
        return fib(n-1) + fib(n-2)
    fib(25)
""",
    "stress_11_list_access": """
def run():
    lst = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    idx, total = 0, 0
    while idx < 10000:
        total += lst[idx % 10]
        idx += 1
"""
}

os.makedirs("tests/benchmarks", exist_ok=True)

for name, code in baselines.items():
    with open(f"tests/benchmarks/{name}.py", "w") as f:
        f.write(code.strip() + "\n\nif __name__ == '__main__':\n    run()\n")
    print(f"Created tests/benchmarks/{name}.py")
