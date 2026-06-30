n = 500000
sieve = [True] * (n + 1)
sieve[0] = sieve[1] = False
i = 2
while i * i <= n:
    if sieve[i]:
        j = i * i
        while j <= n:
            sieve[j] = False
            j += i
    i += 1
print(sum(sieve))
