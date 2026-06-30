count = 0
for py in range(300):
    for px in range(300):
        x0 = px / 150.0 - 2.5
        y0 = py / 150.0 - 1.25
        x = y = 0.0
        escaped = False
        for _ in range(50):
            x2, y2 = x*x, y*y
            if x2 + y2 > 4.0:
                escaped = True
                break
            x, y = x2 - y2 + x0, 2.0*x*y + y0
        if escaped:
            count += 1
print(count)
