n = 3000
records = []
i = 0
while i < n:
    rec = {"id": i, "score": i * 2, "tags": [i % 3, i % 5, i % 7]}
    records.append(rec)
    i = i + 1
total = 0
for rec in records:
    total = total + rec["id"] + rec["score"]
    for t in rec["tags"]:
        total = total + t
print(total)
