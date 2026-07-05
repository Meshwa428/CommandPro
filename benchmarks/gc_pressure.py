total = 0
i = 0
while i < 300000:
    xs = [i, i + 1, i + 2]
    total = total + xs[0] + xs[1] + xs[2]
    i = i + 1
print(total)
