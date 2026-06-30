#include <cstdio>
#include <vector>
void sift_down(std::vector<long long>& a, int i, int n) {
    while (true) {
        int lg=i,l=2*i+1,r=2*i+2;
        if (l<n&&a[l]>a[lg]) lg=l;
        if (r<n&&a[r]>a[lg]) lg=r;
        if (lg==i) break;
        std::swap(a[i],a[lg]); i=lg;
    }
}
int main() {
    long long seed=55555; std::vector<long long> lst(100000);
    for (auto& v:lst){seed=(seed*1664525+1013904223)%4294967296LL;v=seed%100000;}
    int n=lst.size();
    for(int i=n/2-1;i>=0;i--) sift_down(lst,i,n);
    for(int end=n-1;end>0;end--){std::swap(lst[0],lst[end]);sift_down(lst,0,end);}
    printf("%lld\n", lst[50000]);
}
