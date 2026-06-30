import sys
sys.setrecursionlimit(20000)

def qsort(lst, lo, hi):
    if lo >= hi:
        return
    pivot = lst[hi]
    i = lo
    for j in range(lo, hi):
        if lst[j] <= pivot:
            lst[i], lst[j] = lst[j], lst[i]
            i += 1
    lst[i], lst[hi] = lst[hi], lst[i]
    qsort(lst, lo, i - 1)
    qsort(lst, i + 1, hi)

lst = []
x = 12345
for i in range(10000):
    x = (x * 1103515245 + 12345) % 2147483647
    lst.append(x % 10000)
qsort(lst, 0, len(lst) - 1)
print(lst[5000])
