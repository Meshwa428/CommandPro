from math import sqrt
pi = 3.141592653589793
solar_mass = 4.0 * pi * pi
days_per_year = 365.24
bodies = [
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, solar_mass,
    4.84143144246472090, -1.16032004402742839, -0.103622044471123109,
    0.00166007664274403694*days_per_year, 0.00769901118419740425*days_per_year,
    -0.0000690460016972063023*days_per_year, 0.000954791938424326609*solar_mass,
    8.34336671824457987, 4.12479856412430479, -0.403523417114321381,
    -0.00276742510726862411*days_per_year, 0.00499852801234917238*days_per_year,
    0.0000230417297573763929*days_per_year, 0.000285885980666130812*solar_mass,
    12.8943695621391310, -15.1111514016986312, -0.223307578892655734,
    0.00296460137564761618*days_per_year, 0.00237847173959480950*days_per_year,
    -0.0000296589568540237556*days_per_year, 0.0000436624404335156298*solar_mass,
    15.3796971148509165, -25.9193146099879641, 0.179258772950371181,
    0.00268067772490389322*days_per_year, 0.00162824170038242295*days_per_year,
    -0.0000951592254519715870*days_per_year, 0.0000515138902046611451*solar_mass
]
nb = 5
def advance(dt):
    for b in range(nb):
        for c in range(b+1, nb):
            bi, ci = b*7, c*7
            dx = bodies[bi]-bodies[ci]; dy = bodies[bi+1]-bodies[ci+1]; dz = bodies[bi+2]-bodies[ci+2]
            dist2 = dx*dx+dy*dy+dz*dz; dist = sqrt(dist2); mag = dt/(dist2*dist)
            bm, cm = bodies[bi+6], bodies[ci+6]
            bodies[bi+3] -= dx*cm*mag; bodies[bi+4] -= dy*cm*mag; bodies[bi+5] -= dz*cm*mag
            bodies[ci+3] += dx*bm*mag; bodies[ci+4] += dy*bm*mag; bodies[ci+5] += dz*bm*mag
    for b in range(nb):
        bi = b*7
        bodies[bi] += dt*bodies[bi+3]; bodies[bi+1] += dt*bodies[bi+4]; bodies[bi+2] += dt*bodies[bi+5]
def energy():
    e = 0.0
    for b in range(nb):
        bi = b*7; vx,vy,vz = bodies[bi+3],bodies[bi+4],bodies[bi+5]
        e += 0.5*bodies[bi+6]*(vx*vx+vy*vy+vz*vz)
        for c in range(b+1, nb):
            ci = c*7; dx=bodies[bi]-bodies[ci]; dy=bodies[bi+1]-bodies[ci+1]; dz=bodies[bi+2]-bodies[ci+2]
            e -= bodies[bi+6]*bodies[ci+6]/sqrt(dx*dx+dy*dy+dz*dz)
    return e
px=py=pz=0.0
for b in range(1, nb):
    bi=b*7; px+=bodies[bi+3]*bodies[bi+6]; py+=bodies[bi+4]*bodies[bi+6]; pz+=bodies[bi+5]*bodies[bi+6]
bodies[3]=-px/solar_mass; bodies[4]=-py/solar_mass; bodies[5]=-pz/solar_mass
for _ in range(50000): advance(0.01)
print(energy())
