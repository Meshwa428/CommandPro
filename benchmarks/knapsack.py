n, cap = 200, 2000
weights, values, seed = [], [], 42
for _ in range(n):
    seed = (seed * 1664525 + 1013904223) % 4294967296
    weights.append(seed % 50 + 1)
    seed = (seed * 1664525 + 1013904223) % 4294967296
    values.append(seed % 100 + 1)
w = cap + 1
dp = [0] * ((n + 1) * w)
for i in range(1, n + 1):
    wi, vi = weights[i-1], values[i-1]
    base, cur = (i-1)*w, i*w
    for ww in range(cap + 1):
        prev = dp[base + ww]
        if wi <= ww:
            c = dp[base + ww - wi] + vi
            if c > prev: prev = c
        dp[cur + ww] = prev
print(dp[n * w + cap])
