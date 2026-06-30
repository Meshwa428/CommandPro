#include <cstdio>
#include <cmath>
#include <vector>
static double A(int i, int j){ return 1.0/((double)((i+j)*(i+j+1)/2+i+1)); }
static void av(const std::vector<double>& u, std::vector<double>& out, int n){
    for(int i=0;i<n;i++){double s=0;for(int j=0;j<n;j++)s+=A(i,j)*u[j];out[i]=s;}
}
static void atv(const std::vector<double>& u, std::vector<double>& out, int n){
    for(int i=0;i<n;i++){double s=0;for(int j=0;j<n;j++)s+=A(j,i)*u[j];out[i]=s;}
}
static void atav(const std::vector<double>& v, std::vector<double>& tmp, std::vector<double>& out, int n){
    av(v,tmp,n); atv(tmp,out,n);
}
int main(){
    const int n=100;
    std::vector<double> u(n,1),v(n),tmp(n);
    for(int i=0;i<10;i++){atav(u,tmp,v,n);atav(v,tmp,u,n);}
    double vbv=0,vv=0;
    for(int i=0;i<n;i++){vbv+=u[i]*v[i];vv+=v[i]*v[i];}
    char buf[64]; double val=sqrt(vbv/vv);
    for(int p=1;p<=17;p++){snprintf(buf,sizeof(buf),"%.*g",p,val);double c;sscanf(buf,"%lf",&c);if(c==val)break;}
    puts(buf);
}
