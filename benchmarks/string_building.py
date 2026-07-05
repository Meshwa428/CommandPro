n = 20000
parts = []
i = 0
while i < n:
    parts.append(f"item-{i}-end")
    i = i + 1
joined = "|".join(parts)
print(len(joined))
