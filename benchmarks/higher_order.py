def apply_sum(f, n):
    total = 0
    for i in range(n):
        total += f(i)
    return total

def square(x):
    return x * x

print(apply_sum(square, 1000000))
