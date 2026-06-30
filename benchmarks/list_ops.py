lst = []
for i in range(100000):
    lst.append(i)

total = 0
for x in lst:
    total += x
print(total)
