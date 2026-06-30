#include <cstdio>
#include <vector>
int main() {
    const int n = 500000;
    std::vector<bool> s(n+1, true);
    s[0] = s[1] = false;
    for (int i = 2; (long long)i*i <= n; i++)
        if (s[i]) for (int j = i*i; j <= n; j += i) s[j] = false;
    int count = 0;
    for (int i = 0; i <= n; i++) if (s[i]) count++;
    printf("%d\n", count);
}
