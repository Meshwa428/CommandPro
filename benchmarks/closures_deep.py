def make_adder(n):
    def adder(x):
        return x + n
    return adder

fns = []
i = 0
while i < 200:
    fns.append(make_adder(i))
    i = i + 1

total = 0
round_ = 0
while round_ < 500:
    for f in fns:
        total = total + f(round_)
    round_ = round_ + 1
print(total)
