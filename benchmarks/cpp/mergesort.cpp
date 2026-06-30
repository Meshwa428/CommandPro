#include <cstdio>
#include <vector>
void merge(std::vector<long long>& a, int lo, int mid, int hi) {
    std::vector<long long> L(a.begin()+lo, a.begin()+mid);
    std::vector<long long> R(a.begin()+mid, a.begin()+hi);
    int li=0,ri=0,k=lo;
    while (li<(int)L.size()) {
        if (ri>=(int)R.size()||L[li]<=R[ri]) a[k++]=L[li++];
        else a[k++]=R[ri++];
    }
    while (ri<(int)R.size()) a[k++]=R[ri++];
}
void msort(std::vector<long long>& a, int lo, int hi) {
    if (hi-lo<=1) return;
    int mid=(lo+hi)/2;
    msort(a,lo,mid); msort(a,mid,hi); merge(a,lo,mid,hi);
}
int main() {
    long long seed=98765; std::vector<long long> lst(10000);
    for (auto& v:lst) { seed=(seed*1664525+1013904223)%4294967296LL; v=seed%10000; }
    msort(lst,0,lst.size());
    printf("%lld\n", lst[5000]);
}
