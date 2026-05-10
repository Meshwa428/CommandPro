#
# String Equality Benchmark - long strings (Python baseline)
#

def run():
    base = "abcdefghijklmnopqrstuvwxyz"
    suffix = "1234567890"
    i = 0
    while i < 1000000:
        s1 = base + suffix + base + suffix
        s2 = base + suffix + base + suffix
        if s1 == s2: pass
        i += 1

if __name__ == '__main__':
    import time
    start = time.time()
    run()
    print("RESULT:" + str((time.time() - start) * 1000))