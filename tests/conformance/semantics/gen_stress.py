print("# Large Stress Test\n")
for i in range(500):
    print(f"let x_{i} = {i};")
    print(f"let y_{i} = x_{i} * 2;")

print("let sum = 0;")
for i in range(500):
    print(f"sum = sum + y_{i};")

print("if (sum == 249500) { println \"Stress OK\"; } else { println \"Stress FAIL: \" + sum; }")
