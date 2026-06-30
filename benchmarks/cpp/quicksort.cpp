#include <iostream>
#include <vector>
void qsort_impl(std::vector<long long>& lst, int lo, int hi) {
    if (lo >= hi) return;
    long long pivot = lst[hi];
    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (lst[j] <= pivot) { std::swap(lst[i], lst[j]); i++; }
    }
    std::swap(lst[i], lst[hi]);
    qsort_impl(lst, lo, i - 1);
    qsort_impl(lst, i + 1, hi);
}
int main() {
    std::vector<long long> lst;
    long long x = 12345;
    for (int i = 0; i < 10000; i++) {
        x = (x * 1103515245LL + 12345) % 2147483647LL;
        lst.push_back(x % 10000);
    }
    qsort_impl(lst, 0, (int)lst.size() - 1);
    std::cout << lst[5000] << "\n";
}
