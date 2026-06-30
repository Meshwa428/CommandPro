#include <cstdio>
int main() {
    long long seed = 1234567, inside = 0, n = 5000000;
    for (long long i = 0; i < n; i++) {
        seed = (seed * 1664525 + 1013904223) % 4294967296LL;
        double x = seed / 4294967296.0;
        seed = (seed * 1664525 + 1013904223) % 4294967296LL;
        double y = seed / 4294967296.0;
        if (x*x + y*y <= 1.0) inside++;
    }
    // shortest round-trip
    char buf[32]; double v = inside * 4.0 / n;
    for (int p = 1; p <= 17; p++) {
        snprintf(buf, sizeof(buf), "%.*g", p, v);
        double c; sscanf(buf, "%lf", &c);
        if (c == v) break;
    }
    puts(buf);
}
