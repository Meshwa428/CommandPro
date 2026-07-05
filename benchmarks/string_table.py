words = "the quick brown fox jumps over the lazy dog the fox runs quick brown dog jumps over lazy fox".split(" ")
counts = {}
n = 20000
total = 0
i = 0
while i < n:
    idx = i % len(words)
    w = words[idx]
    if w in counts:
        counts[w] = counts[w] + 1
    else:
        counts[w] = 1
    i = i + 1
for w in words:
    total = total + counts[w]
print(total)
