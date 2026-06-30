#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> res;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == delim) {
            res.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return res;
}

std::string join(const std::vector<std::string>& v, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < v.size(); i++) { if (i) r += sep; r += v[i]; }
    return r;
}

std::string to_upper(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

int main() {
    auto words = split("the quick brown fox jumps over the lazy dog", ' ');
    int count = 0;
    for (int i = 0; i < 100000; i++) {
        std::string s = join(words, "-");
        if (s.size() >= 3 && s.substr(0, 3) == "the") count++;
        if (s.size() >= 3 && s.substr(s.size() - 3) == "dog") count++;
        std::string u = to_upper(s);
        if (u.find("FOX") != std::string::npos) count++;
    }
    std::cout << count << "\n";
}
