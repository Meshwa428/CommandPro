n = 50000
head = {"val": 0, "nxt": None}
cur = head
for i in range(1, n):
    node = {"val": i, "nxt": None}
    cur["nxt"] = node
    cur = node
total = 0
cur = head
while cur is not None:
    total += cur["val"]
    cur = cur["nxt"]
print(total)
