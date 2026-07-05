#include <cstdio>
#include <vector>
#include <unordered_map>

struct Rec {
    long long id, score;
    std::vector<long long> tags;
};

int main() {
    const int n = 3000;
    std::vector<Rec> records;
    records.reserve(n);
    for (int i = 0; i < n; i++) {
        Rec r;
        r.id = i;
        r.score = i * 2;
        r.tags = {i % 3, i % 5, i % 7};
        records.push_back(std::move(r));
    }
    long long total = 0;
    for (auto& rec : records) {
        total += rec.id + rec.score;
        for (auto t : rec.tags) total += t;
    }
    printf("%lld\n", total);
}
