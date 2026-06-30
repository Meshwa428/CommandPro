line = "field1,field2,field3,field4,field5,field6,field7,field8,field9,field10"
total = 0
for i in range(100000):
    fields = line.split(",")
    total += len(fields)
print(total)
