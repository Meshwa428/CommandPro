def make_mat(n):
    return [[i * n + j for j in range(n)] for i in range(n)]

n = 40
a = make_mat(n)
b = make_mat(n)
c = [[0] * n for _ in range(n)]
for i in range(n):
    for j in range(n):
        s = 0
        for k in range(n):
            s += a[i][k] * b[k][j]
        c[i][j] = s
print(c[n-1][n-1])
