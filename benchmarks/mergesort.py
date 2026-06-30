import sys
sys.setrecursionlimit(100000)
def merge(a, lo, mid, hi):
    left = a[lo:mid]; right = a[mid:hi]
    li = ri = 0; k = lo
    while li < len(left):
        if ri >= len(right) or left[li] <= right[ri]:
            a[k] = left[li]; li += 1
        else:
            a[k] = right[ri]; ri += 1
        k += 1
    while ri < len(right):
        a[k] = right[ri]; ri += 1; k += 1
def msort(a, lo, hi):
    if hi - lo <= 1: return
    mid = (lo + hi) // 2
    msort(a, lo, mid); msort(a, mid, hi)
    merge(a, lo, mid, hi)
seed = 98765; lst = []
for _ in range(10000):
    seed = (seed * 1664525 + 1013904223) % 4294967296
    lst.append(seed % 10000)
msort(lst, 0, len(lst))
print(lst[5000])
