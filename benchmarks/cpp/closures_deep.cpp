#include <cstdio>
#include <functional>
#include <vector>

std::function<long long(long long)> make_adder(long long n) {
    return [n](long long x) { return x + n; };
}

int main() {
    std::vector<std::function<long long(long long)>> fns;
    for (long long i = 0; i < 200; i++) fns.push_back(make_adder(i));

    long long total = 0;
    for (long long round = 0; round < 500; round++) {
        for (auto& f : fns) total += f(round);
    }
    printf("%lld\n", total);
}
