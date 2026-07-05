n = 30000
int_total = 0
float_total = 0.0
str_len_total = 0
list_total = 0
cache = {}
i = 0
while i < n:
    int_total = int_total + i
    float_total = float_total + i * 1.5
    s = f"n{i}"
    str_len_total = str_len_total + len(s)
    xs = [i, i + 1]
    list_total = list_total + xs[0] + xs[1]
    k = i % 100
    if k in cache:
        cache[k] = cache[k] + 1
    else:
        cache[k] = 1
    i = i + 1
cache_total = 0
j = 0
while j < 100:
    cache_total = cache_total + cache[j]
    j = j + 1
print(int_total)
print(float_total)
print(str_len_total)
print(list_total)
print(cache_total)
