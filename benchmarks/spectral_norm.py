from math import sqrt
def A(i, j): return 1.0 / ((i+j)*(i+j+1)//2 + i + 1)
def av(u, n):
    return [sum(A(i,j)*u[j] for j in range(n)) for i in range(n)]
def atv(u, n):
    return [sum(A(j,i)*u[j] for j in range(n)) for i in range(n)]
def atav(v, n): return atv(av(v, n), n)
n = 100
u = [1.0]*n
for _ in range(10):
    v = atav(u, n); u = atav(v, n)
vbv = sum(u[i]*v[i] for i in range(n))
vv  = sum(v[i]*v[i] for i in range(n))
print(sqrt(vbv/vv))
