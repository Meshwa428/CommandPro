#include <iostream>
#include <unordered_map>
std::unordered_map<long long, long long> cache;
long long fib_memo(long long n) {
    if (n <= 1) return n;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    long long r = fib_memo(n-1) + fib_memo(n-2);
    return cache[n] = r;
}
int main() { std::cout << fib_memo(35) << "\n"; }
