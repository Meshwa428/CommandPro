#include <cstdio>
#include <string>
#include <vector>

int main() {
    const int n = 20000;
    std::vector<std::string> parts;
    parts.reserve(n);
    for (int i = 0; i < n; i++) {
        parts.push_back("item-" + std::to_string(i) + "-end");
    }
    std::string joined;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) joined += "|";
        joined += parts[i];
    }
    printf("%zu\n", joined.size());
}
