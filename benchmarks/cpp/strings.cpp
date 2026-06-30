#include <iostream>
#include <string>
int main() {
    std::string s;
    s.reserve(50000);
    for (int i = 0; i < 50000; i++) s += 'x';
    std::cout << s.size() << "\n";
}
