#include <cstdio>
#include <vector>
#include <algorithm>
int main() {
    const int m = 500, n = 480, w = n + 1;
    std::vector<int> a(m), b(n), dp((m+1)*w, 0);
    for (int i = 0; i < m; i++) a[i] = i % 26;
    for (int i = 0; i < n; i++) b[i] = (i*5+3) % 26;
    for (int i = 1; i <= m; i++)
        for (int j = 1; j <= n; j++)
            dp[i*w+j] = (a[i-1]==b[j-1]) ? dp[(i-1)*w+(j-1)]+1
                                           : std::max(dp[(i-1)*w+j], dp[i*w+(j-1)]);
    printf("%d\n", dp[m*w+n]);
}
