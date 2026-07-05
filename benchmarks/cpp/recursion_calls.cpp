#include <cstdio>

long long sum_rec(long long n) {
    if (n <= 0) return 0;
    return n + sum_rec(n - 1);
}

int main() {
    long long total = 0;
    for (int i = 0; i < 30000; i++) {
        total += sum_rec(50);
    }
    printf("%lld\n", total);
}
