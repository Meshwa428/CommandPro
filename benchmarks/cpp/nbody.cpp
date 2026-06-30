#include <cstdio>
#include <cmath>
#include <vector>
static const double pi = 3.141592653589793;
static const double solar_mass = 4.0*pi*pi;
static const double dpy = 365.24;
// 5 bodies * 7 doubles (x y z vx vy vz m)
static double B[] = {
    0,0,0,0,0,0,solar_mass,
    4.84143144246472090,-1.16032004402742839,-0.103622044471123109,
    0.00166007664274403694*dpy,0.00769901118419740425*dpy,-0.0000690460016972063023*dpy,0.000954791938424326609*solar_mass,
    8.34336671824457987,4.12479856412430479,-0.403523417114321381,
    -0.00276742510726862411*dpy,0.00499852801234917238*dpy,0.0000230417297573763929*dpy,0.000285885980666130812*solar_mass,
    12.8943695621391310,-15.1111514016986312,-0.223307578892655734,
    0.00296460137564761618*dpy,0.00237847173959480950*dpy,-0.0000296589568540237556*dpy,0.0000436624404335156298*solar_mass,
    15.3796971148509165,-25.9193146099879641,0.179258772950371181,
    0.00268067772490389322*dpy,0.00162824170038242295*dpy,-0.0000951592254519715870*dpy,0.0000515138902046611451*solar_mass
};
static const int nb=5;
void advance(double dt){
    for(int b=0;b<nb;b++) for(int c=b+1;c<nb;c++){
        int bi=b*7,ci=c*7;
        double dx=B[bi]-B[ci],dy=B[bi+1]-B[ci+1],dz=B[bi+2]-B[ci+2];
        double d2=dx*dx+dy*dy+dz*dz,dist=sqrt(d2),mag=dt/(d2*dist);
        double bm=B[bi+6],cm=B[ci+6];
        B[bi+3]-=dx*cm*mag;B[bi+4]-=dy*cm*mag;B[bi+5]-=dz*cm*mag;
        B[ci+3]+=dx*bm*mag;B[ci+4]+=dy*bm*mag;B[ci+5]+=dz*bm*mag;
    }
    for(int b=0;b<nb;b++){int bi=b*7;B[bi]+=dt*B[bi+3];B[bi+1]+=dt*B[bi+4];B[bi+2]+=dt*B[bi+5];}
}
double energy(){
    double e=0;
    for(int b=0;b<nb;b++){int bi=b*7;double vx=B[bi+3],vy=B[bi+4],vz=B[bi+5];
        e+=0.5*B[bi+6]*(vx*vx+vy*vy+vz*vz);
        for(int c=b+1;c<nb;c++){int ci=c*7;double dx=B[bi]-B[ci],dy=B[bi+1]-B[ci+1],dz=B[bi+2]-B[ci+2];
            e-=B[bi+6]*B[ci+6]/sqrt(dx*dx+dy*dy+dz*dz);}
    }return e;
}
int main(){
    double px=0,py=0,pz=0;
    for(int b=1;b<nb;b++){int bi=b*7;px+=B[bi+3]*B[bi+6];py+=B[bi+4]*B[bi+6];pz+=B[bi+5]*B[bi+6];}
    B[3]=-px/solar_mass;B[4]=-py/solar_mass;B[5]=-pz/solar_mass;
    for(int s=0;s<50000;s++) advance(0.01);
    char buf[64]; double v=energy();
    for(int p=1;p<=17;p++){snprintf(buf,sizeof(buf),"%.*g",p,v);double c;sscanf(buf,"%lf",&c);if(c==v)break;}
    puts(buf);
}
