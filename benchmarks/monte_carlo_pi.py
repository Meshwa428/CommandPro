seed = 1234567
inside = 0
n = 5000000
for _ in range(n):
    seed = (seed * 1664525 + 1013904223) % 4294967296
    x = seed / 4294967296.0
    seed = (seed * 1664525 + 1013904223) % 4294967296
    y = seed / 4294967296.0
    if x*x + y*y <= 1.0:
        inside += 1
print(inside * 4.0 / n)
