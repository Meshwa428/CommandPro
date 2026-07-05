#include <cstdio>
#include <string>
#include <unordered_map>

int main() {
    const int n = 30000;
    long long int_total = 0;
    double float_total = 0.0;
    long long str_len_total = 0;
    long long list_total = 0;
    std::unordered_map<int, long long> cache;
    for (int i = 0; i < n; i++) {
        int_total += i;
        float_total += i * 1.5;
        std::string s = "n" + std::to_string(i);
        str_len_total += (long long)s.size();
        long long xs[2] = {i, i + 1};
        list_total += xs[0] + xs[1];
        int k = i % 100;
        auto it = cache.find(k);
        if (it != cache.end()) it->second += 1;
        else cache[k] = 1;
    }
    long long cache_total = 0;
    for (int j = 0; j < 100; j++) cache_total += cache[j];
    printf("%lld\n", int_total);
    printf("%f\n", float_total);
    printf("%lld\n", str_len_total);
    printf("%lld\n", list_total);
    printf("%lld\n", cache_total);
}
