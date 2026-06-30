def make_counter():
    n = 0
    def counter():
        nonlocal n
        n += 1
        return n
    return counter

total = 0
for i in range(1000):
    c = make_counter()
    for j in range(100):
        total += c()
print(total)
