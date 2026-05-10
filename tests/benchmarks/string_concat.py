#
# String Concat Benchmark - Python baseline
# Tests runtime string concatenation
#

def run():
    base = "hello"
    suffix = "_world"
    i = 0
    while i < 1000000:
        s1 = base + suffix
        s2 = base + suffix
        if s1 == s2: pass
        i += 1

if __name__ == '__main__':
    import time
    start = time.time()
    run()
    print("RESULT:" + str((time.time() - start) * 1000))