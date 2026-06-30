#include <cstdio>
#include <vector>
int main() {
    const int n = 200, cap = 2000, w = cap + 1;
    std::vector<int> wts(n), vals(n);
    long long seed = 42;
    for (int i = 0; i < n; i++) {
        seed = (seed*1664525+1013904223)%4294967296LL; wts[i] = seed%50+1;
        seed = (seed*1664525+1013904223)%4294967296LL; vals[i] = seed%100+1;
    }
    std::vector<int> dp((n+1)*w, 0);
    for (int i = 1; i <= n; i++) {
        int wi = wts[i-1], vi = vals[i-1], base = (i-1)*w, cur = i*w;
        for (int ww = 0; ww <= cap; ww++) {
            int prev = dp[base+ww];
            if (wi <= ww) { int c = dp[base+ww-wi]+vi; if (c > prev) prev = c; }
            dp[cur+ww] = prev;
        }
    }
    printf("%d\n", dp[n*w+cap]);
}
