#include <iostream>
#include <functional>
long long apply_sum(std::function<long long(long long)> f, long long n) {
    long long total = 0;
    for (long long i = 0; i < n; i++) total += f(i);
    return total;
}
int main() {
    std::cout << apply_sum([](long long x) { return x * x; }, 1000000) << "\n";
}
