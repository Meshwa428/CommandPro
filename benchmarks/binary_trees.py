def make_tree(depth):
    if depth == 0: return (1, None, None)
    return (1, make_tree(depth-1), make_tree(depth-1))
def check_tree(node):
    if node[1] is None: return 1
    return 1 + check_tree(node[1]) + check_tree(node[2])
max_depth = 12
total = 0
d = 4
while d <= max_depth:
    iters = 2 ** (max_depth - d + 4)
    for _ in range(iters):
        total += check_tree(make_tree(d))
    d += 2
print(total)
