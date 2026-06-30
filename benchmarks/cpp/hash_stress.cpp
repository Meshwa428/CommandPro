#include <cstdio>
#include <unordered_map>
int main() {
    const int n = 5000;
    std::unordered_map<long long,long long> m;
    long long seed = 99991, total = 0;
    for (int i = 0; i < n; i++) {
        seed = (seed*1664525+1013904223)%4294967296LL;
        long long k = seed % n;
        m[k] = k * 2;
    }
    for (int i = 0; i < n; i++) {
        seed = (seed*1664525+1013904223)%4294967296LL;
        long long k = seed % n;
        auto it = m.find(k);
        if (it != m.end()) total += it->second;
    }
    printf("%lld\n", total);
}
