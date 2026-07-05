#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>

int main() {
    std::string src = "the quick brown fox jumps over the lazy dog the fox runs quick brown dog jumps over lazy fox";
    std::vector<std::string> words;
    std::istringstream iss(src);
    std::string w;
    while (iss >> w) words.push_back(w);

    std::unordered_map<std::string, long long> counts;
    const int n = 20000;
    long long total = 0;
    for (int i = 0; i < n; i++) {
        const std::string& word = words[i % words.size()];
        auto it = counts.find(word);
        if (it != counts.end()) it->second += 1;
        else counts[word] = 1;
    }
    for (auto& word : words) total += counts[word];
    printf("%lld\n", total);
}
