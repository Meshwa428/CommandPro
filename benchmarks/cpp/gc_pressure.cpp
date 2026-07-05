#include <cstdio>
#include <vector>

int main() {
    long long total = 0;
    for (long long i = 0; i < 300000; i++) {
        std::vector<long long> xs = {i, i + 1, i + 2};
        total += xs[0] + xs[1] + xs[2];
    }
    printf("%lld\n", total);
}
