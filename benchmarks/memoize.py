cache = {}
def fib_memo(n):
    if n <= 1:
        return n
    if n in cache:
        return cache[n]
    r = fib_memo(n - 1) + fib_memo(n - 2)
    cache[n] = r
    return r
print(fib_memo(35))
