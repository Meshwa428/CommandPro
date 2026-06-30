def sift_down(a, i, n):
    while True:
        largest = i; l = 2*i+1; r = 2*i+2
        if l < n and a[l] > a[largest]: largest = l
        if r < n and a[r] > a[largest]: largest = r
        if largest == i: break
        a[i], a[largest] = a[largest], a[i]; i = largest
def heapsort(a):
    n = len(a)
    for i in range(n//2-1, -1, -1): sift_down(a, i, n)
    for end in range(n-1, 0, -1):
        a[0], a[end] = a[end], a[0]; sift_down(a, 0, end)
seed = 55555; lst = []
for _ in range(100000):
    seed = (seed * 1664525 + 1013904223) % 4294967296
    lst.append(seed % 100000)
heapsort(lst)
print(lst[50000])
