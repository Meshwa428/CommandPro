#include <iostream>
bool is_prime(long long n) {
    if (n < 2) return false;
    for (long long i = 2; i * i <= n; i++)
        if (n % i == 0) return false;
    return true;
}
int main() {
    int count = 0;
    for (long long n = 2; n < 50000; n++)
        if (is_prime(n)) count++;
    std::cout << count << "\n";
}
