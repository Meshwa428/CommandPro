words = "the quick brown fox jumps over the lazy dog".split(" ")
n = 100000
count = 0
for i in range(n):
    s = "-".join(words)
    if s.startswith("the"): count += 1
    if s.endswith("dog"):   count += 1
    u = s.upper()
    if u.find("FOX") >= 0:  count += 1
print(count)
