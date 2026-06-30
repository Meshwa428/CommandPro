#include <cstdio>
int main() {
    int count = 0;
    for (int py = 0; py < 300; py++) {
        for (int px = 0; px < 300; px++) {
            double x0 = px / 150.0 - 2.5, y0 = py / 150.0 - 1.25;
            double x = 0, y = 0;
            bool escaped = false;
            for (int i = 0; i < 50; i++) {
                double x2 = x*x, y2 = y*y;
                if (x2 + y2 > 4.0) { escaped = true; break; }
                double xt = x2 - y2 + x0;
                y = 2.0*x*y + y0; x = xt;
            }
            if (escaped) count++;
        }
    }
    printf("%d\n", count);
}
