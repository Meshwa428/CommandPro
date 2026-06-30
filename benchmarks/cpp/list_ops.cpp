#include <iostream>
#include <vector>
int main() {
    std::vector<long long> lst;
    lst.reserve(100000);
    for (long long i = 0; i < 100000; i++) lst.push_back(i);
    long long total = 0;
    for (auto x : lst) total += x;
    std::cout << total << "\n";
}
