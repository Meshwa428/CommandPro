#include <cstdio>
#include <string>
#include <vector>
std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> r; size_t p = 0;
    for (size_t i = 0; i <= s.size(); i++)
        if (i == s.size() || s[i] == d) { r.push_back(s.substr(p, i-p)); p = i+1; }
    return r;
}
int main() {
    std::string line = "field1,field2,field3,field4,field5,field6,field7,field8,field9,field10";
    long long total = 0;
    for (int i = 0; i < 100000; i++) total += split(line, ',').size();
    printf("%lld\n", total);
}
