#include <iostream>
#include <vector>
int main() {
    int n = 40;
    auto make_mat = [&]() {
        std::vector<std::vector<long long>> m(n, std::vector<long long>(n));
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                m[i][j] = (long long)i * n + j;
        return m;
    };
    auto a = make_mat(), b = make_mat();
    std::vector<std::vector<long long>> c(n, std::vector<long long>(n, 0));
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            long long s = 0;
            for (int k = 0; k < n; k++) s += a[i][k] * b[k][j];
            c[i][j] = s;
        }
    std::cout << c[n-1][n-1] << "\n";
}
