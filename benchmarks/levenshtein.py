m, n = 500, 480
a = [i % 26 for i in range(m)]
b = [(i * 3 + 7) % 26 for i in range(n)]
w = n + 1
dp = list(range(w)) + [0] * (m * w)
for i in range(1, m + 1):
    dp[i * w] = i
    for j in range(1, n + 1):
        if a[i-1] == b[j-1]:
            dp[i*w+j] = dp[(i-1)*w+(j-1)]
        else:
            dp[i*w+j] = 1 + min(dp[(i-1)*w+j], dp[i*w+(j-1)], dp[(i-1)*w+(j-1)])
print(dp[m*w+n])
