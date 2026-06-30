def is_prime(n):
    if n < 2:
        return False
    i = 2
    while i * i <= n:
        if n % i == 0:
            return False
        i += 1
    return True

count = 0
for n in range(2, 50000):
    if is_prime(n):
        count += 1
print(count)
