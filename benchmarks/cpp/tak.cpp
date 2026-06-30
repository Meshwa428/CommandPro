#include <cstdio>
long long tak(long long x, long long y, long long z) {
    if (y >= x) return z;
    return tak(tak(x-1,y,z), tak(y-1,z,x), tak(z-1,x,y));
}
int main() { printf("%lld\n", tak(18, 12, 6)); }
