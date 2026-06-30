n = 5000
m = {}
seed = 99991
for _ in range(n):
    seed = (seed * 1664525 + 1013904223) % 4294967296
    k = seed % n
    m[k] = k * 2
total = 0
for _ in range(n):
    seed = (seed * 1664525 + 1013904223) % 4294967296
    k = seed % n
    v = m.get(k)
    if v is not None: total += v
print(total)
