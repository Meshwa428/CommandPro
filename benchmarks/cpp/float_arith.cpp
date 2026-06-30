#include <cstdio>
int main() {
    double x = 1.0;
    for (int i = 0; i < 10000000; i++) x = x * 1.0000001 + 0.5;
    // shortest round-trip (matches Python repr / Synapse output)
    char buf[32];
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, x);
        double check; sscanf(buf, "%lf", &check);
        if (check == x) break;
    }
    puts(buf);
}
