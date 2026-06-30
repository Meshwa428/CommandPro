#include <iostream>
#include <functional>
std::function<long long()> make_counter() {
    long long n = 0;
    return [n]() mutable -> long long { return ++n; };
}
int main() {
    long long total = 0;
    for (int i = 0; i < 1000; i++) {
        auto c = make_counter();
        for (int j = 0; j < 100; j++) total += c();
    }
    std::cout << total << "\n";
}
