#include "ChaomaiPlaneOptimizer.h"
#include "ChaomaiPlaneData.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <thread>

extern "C" void predict_row(const double*, int, const double*, const double*, int,
    const double*, int, const int*, int, const double*, double*, double*, float*, int);
namespace chaomai_plane {
namespace {
constexpr double pi = 3.14159265358979323846;
Vec add(Vec a, Vec b) { return {a[0]+b[0], a[1]+b[1], a[2]+b[2]}; }
Vec sub(Vec a, Vec b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
Vec mul(Vec a, double f) { return {a[0]*f, a[1]*f, a[2]*f}; }
double dot(Vec a, Vec b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
Vec cross(Vec a, Vec b) { return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]}; }
double length(Vec a) { return std::sqrt(dot(a,a)); }
Vec axis(double yaw) { return {-std::sin(yaw),std::cos(yaw),0}; }
bool isCopper(Vec p, Vec n) { return std::abs(p[1]-.14)<1e-7 && n[1]<-.9999; }
double interpolate(double z, const std::vector<double>& x, const std::vector<double>& y) {
    auto it=std::upper_bound(x.begin(),x.end(),z);
    if(it==x.begin()) return y.front();
    if(it==x.end()) return y.back();
    const size_t b=size_t(it-x.begin()), a=b-1;
    return y[a]+(y[b]-y[a])*(z-x[a])/(x[b]-x[a]);
}
std::vector<double> uniqueSorted(std::vector<double> v) {
    std::sort(v.begin(),v.end()); v.erase(std::unique(v.begin(),v.end()),v.end()); return v;
}
double firstHit(const Surface& surface, Vec o, Vec d) {
    double nearest=std::numeric_limits<double>::infinity();
    for(const auto& tri: surface.triangles) {
        const Vec e1=sub(tri[1],tri[0]), e2=sub(tri[2],tri[0]), h=cross(d,e2);
        const double det=dot(e1,h); if(std::abs(det)<=1e-12) continue;
        const Vec s=sub(o,tri[0]), q=cross(s,e1);
        const double u=dot(s,h)/det, v=dot(d,q)/det, t=dot(e2,q)/det;
        if(u>=-1e-9 && v>=-1e-9 && u+v<=1+1e-9 && t>1e-7) nearest=std::min(nearest,t);
    }
    return nearest;
}
using Sample=std::array<double,14>;
std::vector<Sample> spraySamples(const Plan& p, const Json& stroke, double step, const Context& ctx) {
    std::vector<Sample> samples;
    const int first=stroke.at("first"), last=stroke.at("last");
    for(int i=first+1;i<=last;++i) {
        if(!p.on[i]) continue;
        ctx.check();
        const double dt=p.time[i]-p.time[i-1], yaw=p.q[i-1][3];
        const int count=std::max(1,int(std::ceil(dt/step)));
        const Vec d=axis(yaw), major{std::cos(yaw),std::sin(yaw),0};
        for(int j=0;j<count;++j) {
            const Vec pos=mul(add(p.p[i-1],mul(sub(p.p[i],p.p[i-1]),double(j)/count)),1000);
            samples.push_back({pos[0],pos[1],pos[2],d[0],d[1],0,major[0],major[1],0,0,0,-1,
                p.time[i-1]+dt*j/count,dt/count});
        }
    }
    return samples;
}
std::vector<Triangle> millimeterMesh(const Surface& surface) {
    auto tri=surface.triangles; for(auto& t:tri) for(auto& v:t) v=mul(v,1000); return tri;
}
int threadCount() {
    int n=std::min(8,std::max(1,int(std::thread::hardware_concurrency()/2)));
    if(const char* value=std::getenv("RS2026_THICKNESS_THREADS")) n=std::atoi(value);
    return std::clamp(n,1,16);
}
std::vector<double> predictSamples(const std::vector<Triangle>& tri,const Grid& grid,
    const std::vector<Sample>& samples,const Parameters& cfg,std::vector<double>& tau,std::vector<double>& last) {
    const int nv=int(grid.positions.size()), nc=int(samples.size());
    std::vector<float> field(nv,0);
    if(nc) predict_row(reinterpret_cast<const double*>(tri.data()),int(tri.size()),
        reinterpret_cast<const double*>(grid.positions.data()),reinterpret_cast<const double*>(grid.normals.data()),nv,
        reinterpret_cast<const double*>(samples.data()),nc,&nc,1,cfg.data(),tau.data(),last.data(),field.data(),threadCount());
    std::vector<double> result(nv); for(int i=0;i<nv;++i) result[i]=double(field[i])*1000;
    return result;
}
// Active-set bounded least squares on normal equations. Each subproblem solves
// the free variables exactly; accepted solutions must satisfy projected KKT.
// The axial difference regularizer is included in H, not applied afterwards.
bool boundedSolve(const Eigen::MatrixXd& H,const Eigen::VectorXd& g,const Eigen::VectorXd& upper,
    Eigen::VectorXd& x,const Context& ctx) {
    const int n=int(g.size()); x=Eigen::VectorXd::Constant(n,.4);
    std::vector<int> state(n,-1);
    const double tolerance=1e-10*std::max(1.,g.cwiseAbs().maxCoeff());
    for(int iteration=0;iteration<6000;++iteration) {
        ctx.check();
        std::vector<int> free;
        for(int i=0;i<n;++i) if(state[i]==0) free.push_back(i);
        if(!free.empty()) {
            Eigen::MatrixXd M(free.size(),free.size()); Eigen::VectorXd rhs(free.size());
            for(size_t i=0;i<free.size();++i) {
                const int a=free[i]; rhs[i]=g[a];
                for(int b=0;b<n;++b) if(state[b]) rhs[i]-=H(a,b)*x[b];
                for(size_t j=0;j<free.size();++j) M(i,j)=H(a,free[j]);
            }
            Eigen::LDLT<Eigen::MatrixXd> decomposition(M);
            if(decomposition.info()!=Eigen::Success) return false;
            const Eigen::VectorXd solution=decomposition.solve(rhs);
            if(!solution.allFinite() || (M*solution-rhs).lpNorm<Eigen::Infinity>()>1e-6*std::max(1.,rhs.lpNorm<Eigen::Infinity>())) return false;
            double fraction=1.; int hit=-1, hitState=0;
            for(size_t i=0;i<free.size();++i) {
                const int a=free[i]; const double delta=solution[i]-x[a];
                const double f=solution[i]<.4 ? (.4-x[a])/delta : solution[i]>upper[a] ? (upper[a]-x[a])/delta : 1.;
                if(f<fraction) { fraction=std::max(0.,f); hit=a; hitState=solution[i]<.4?-1:1; }
            }
            for(size_t i=0;i<free.size();++i) {
                const int a=free[i]; x[a]=std::clamp(x[a]+fraction*(solution[i]-x[a]),.4,upper[a]);
            }
            if(hit>=0) { state[hit]=hitState; x[hit]=hitState<0?.4:upper[hit]; continue; }
        }
        const Eigen::VectorXd grad=H*x-g;
        double violation=tolerance; int release=-1;
        for(int i=0;i<n;++i) {
            const double v=state[i]<0?-grad[i]:state[i]>0?grad[i]:0.;
            if(v>violation) { violation=v; release=i; }
        }
        if(release<0) {
            for(int i=0;i<n;++i) if(!state[i] && std::abs(grad[i])>tolerance*10) return false;
            return true;
        }
        state[release]=0;
    }
    return false;
}
}
void Context::check() const { if(cancelled.load(std::memory_order_relaxed)) throw std::runtime_error("Optimization cancelled."); }
void Context::report(double v,const std::string& message) const { check(); if(progress) progress(v,message); }
const char* initialSolution() { return savedSolution; }
const char* implementationSignature() { return nativeImplementationSignature; }
std::array<double,4> inverseKinematics(Vec p,double yaw) {
    return {p[0]+.08*std::sin(yaw),p[1]+.08-.08*std::cos(yaw),p[2]-.47,yaw};
}
void identifyStrokes(Plan& p) {
    p.strokes=Json::array();
    for(size_t i=1;i<p.on.size();++i) {
        if(!p.on[i]) continue;
        const size_t a=i-1; while(i+1<p.on.size() && p.on[i+1]) ++i;
        const size_t b=i; double yaw=0;
        for(size_t j=a;j<=b;++j) yaw+=p.q[j][3]; yaw/=double(b-a+1);
        p.strokes.push_back({{"source",p.strokes.size()},{"xy",{p.p[a][0],p.p[a][1]}},{"yaw",yaw},
            {"z_start",p.p[a][2]},{"z_end",p.p[b][2]},{"time_start",p.time[a]},{"time_end",p.time[b]},
            {"first",a},{"last",b},{"speed",std::abs(p.p[b][2]-p.p[a][2])/(p.time[b]-p.time[a])},
            {"region",std::abs(yaw)<1e-5?"plate":"tube"}});
    }
}
void validatePlan(Plan& p,bool sourceSlots) {
    const size_t n=p.time.size();
    if(n<3||p.p.size()!=n||p.q.size()!=n||p.on.size()!=n) throw std::runtime_error("Trajectory array dimensions are inconsistent.");
    for(size_t i=0;i<n;++i) {
        for(double x:p.p[i]) if(!std::isfinite(x)) throw std::runtime_error("Non-finite TCP position.");
        for(double x:p.q[i]) if(!std::isfinite(x)) throw std::runtime_error("Non-finite robot joint.");
        if(!std::isfinite(p.time[i]) || (i && p.time[i]<=p.time[i-1]) || (p.on[i]!=0&&p.on[i]!=1))
            throw std::runtime_error("Time must be strictly increasing and spray flags must be 0/1.");
        const auto expected=inverseKinematics(p.p[i],p.q[i][3]);
        for(int k=0;k<3;++k) if(std::abs(expected[k]-p.q[i][k])>2e-5) throw std::runtime_error("XYZR joint/TCP frame is inconsistent.");
    }
    const double t0=p.time.front(); for(auto& t:p.time) t-=t0;
    if(sourceSlots) identifyStrokes(p);
    if(p.strokes.size()!=36) throw std::runtime_error("The specialized method requires 36 axial source scans.");
    for(size_t j=0;j<p.strokes.size();++j) {
        auto& st=p.strokes[j]; const int a=st.at("first"), b=st.at("last");
        if(a<0||b<=a||b>=int(n)) throw std::runtime_error("Invalid scan interval.");
        if(sourceSlots) st["region"]=(j<10||j>=26)?"plate":"tube";
        double minYaw=p.q[a][3], maxYaw=minYaw;
        for(int i=a;i<=b;++i) {
            if(std::hypot(p.p[i][0]-p.p[a][0],p.p[i][1]-p.p[a][1])>2e-5) throw std::runtime_error("Paint passes must be straight axial scans.");
            minYaw=std::min(minYaw,p.q[i][3]); maxYaw=std::max(maxYaw,p.q[i][3]);
        }
        if(maxYaw-minYaw>1e-4) throw std::runtime_error("Paint pass yaw must be fixed.");
        if(sourceSlots && st.at("region")=="plate") {
            const double expectedX=j<10?.0575-.005*j:-.0125-.005*(j-26);
            if(std::abs(double(st["yaw"]))>1e-4 || std::abs(p.p[a][1]-.04)>.015 || std::abs(p.p[a][0]-expectedX)>.008)
                throw std::runtime_error("Plate source order or workpiece coordinate frame differs from the registered method.");
        }
        if(std::min(p.p[a][2],p.p[b][2])>.38 || std::max(p.p[a][2],p.p[b][2])<.57)
            throw std::runtime_error("Source scans do not span Z=375..575mm.");
    }
}
Plan planFromJson(const Json& j,bool sourceSlots) {
    if(j.contains("plans")) throw std::runtime_error("Import a single plan, not complete demo data.");
    for(const auto& flag:j.at("on"))
        if(!flag.is_number() || (flag!=0 && flag!=1)) throw std::runtime_error("Spray flags must be exactly 0 or 1.");
    for(const auto& point:j.at("p")) if(!point.is_array() || point.size()!=3) throw std::runtime_error("TCP positions need exactly 3 components.");
    for(const auto& joint:j.at("q")) if(!joint.is_array() || joint.size()!=4) throw std::runtime_error("XYZR joints need exactly 4 components.");
    Plan p; p.p=j.at("p").get<decltype(p.p)>(); p.q=j.at("q").get<decltype(p.q)>();
    p.time=j.at("time").get<decltype(p.time)>(); p.on=j.at("on").get<decltype(p.on)>();
    if(j.contains("strokes")) p.strokes=j["strokes"];
    if(j.contains("command")) p.command=j["command"].get<decltype(p.command)>();
    if(j.contains("row")) p.row=j["row"].get<decltype(p.row)>();
    validatePlan(p,sourceSlots); return p;
}
Json planToJson(const Plan& p) {
    return {{"p",p.p},{"q",p.q},{"time",p.time},{"on",p.on},{"command",p.command},{"row",p.row},{"strokes",p.strokes}};
}
Profile sampleProfile(const Surface& s,const Plan& original,double pitch,const Context& ctx) {
    ctx.check(); Profile prof; prof.low={1e100,1e100,1e100}; prof.high={-1e100,-1e100,-1e100};
    for(const auto& t:s.triangles) for(auto p:t) for(int k=0;k<3;++k) {
        prof.low[k]=std::min(prof.low[k],p[k]); prof.high[k]=std::max(prof.high[k],p[k]);
    }
    const double station=(prof.low[2]+prof.high[2])*.5;
    std::vector<Vec> anchors;
    double plateY=.14;
    bool gotPlate=false;
    for(const auto& st:original.strokes) {
        const double yaw=st.at("yaw"); const auto xy=st.at("xy").get<std::array<double,2>>();
        if(st.at("region")=="tube") anchors.push_back(add({xy[0],xy[1],station},mul(axis(yaw),.1)));
        else if(!gotPlate) { plateY=xy[1]+.1; gotPlate=true; }
    }
    Eigen::MatrixXd A(anchors.size(),3); Eigen::VectorXd b(anchors.size());
    for(size_t i=0;i<anchors.size();++i) { auto p=anchors[i]; A.row(i)<<2*p[0],2*p[1],1.; b[i]=p[0]*p[0]+p[1]*p[1]; }
    const Eigen::Vector3d fit=A.colPivHouseholderQr().solve(b);
    struct Segment {Vec a,b,n;}; std::vector<Segment> segments;
    for(size_t j=0;j<s.triangles.size();++j) {
        const auto& tri=s.triangles[j]; std::vector<Vec> hits;
        for(int k=0;k<3;++k) {
            const Vec a=tri[k], b=tri[(k+1)%3]; const double da=a[2]-station, db=b[2]-station;
            if(da*db<0) hits.push_back(add(a,mul(sub(b,a),da/(da-db))));
        }
        if(hits.size()==2) segments.push_back({hits[0],hits[1],s.normals[j]});
    }
    for(bool changed=true;changed;) {
        ctx.check(); changed=false;
        for(size_t i=0;i<segments.size()&&!changed;++i) for(size_t j=i+1;j<segments.size();++j) {
            auto a=segments[i]; auto b=segments[j];
            if(dot(a.n,b.n)<1-1e-7) continue;
            if(length(sub(a.b,b.b))<1e-8||length(sub(a.a,b.a))<1e-8) std::swap(b.a,b.b);
            if(length(sub(a.b,b.a))<1e-8) segments[i]={a.a,b.b,a.n};
            else if(length(sub(b.b,a.a))<1e-8) segments[i]={b.a,a.b,a.n};
            else continue;
            segments.erase(segments.begin()+j); changed=true; break;
        }
    }
    std::vector<double> angles{-88,-85,85,88}; for(int i=-75;i<=75;i+=15) angles.push_back(i);
    angles=uniqueSorted(angles);
    for(const auto& seg:segments) {
        ctx.check(); const Vec delta=sub(seg.b,seg.a), mid=mul(add(seg.a,seg.b),.5);
        double spacing=pitch;
        if(length(delta)<.02&&std::abs(mid[0]-fit[0])<.006&&mid[1]>fit[1]+.007) spacing=std::min(spacing,.000025);
        const int count=std::max(1,int(std::ceil(length(delta)/spacing)));
        for(int k=0;k<count;++k) {
            const Vec a=add(seg.a,mul(delta,double(k)/count)), b=add(seg.a,mul(delta,double(k+1)/count)), p=mul(add(a,b),.5);
            if(p[1]>plateY+1e-8) continue;
            bool visible=false;
            for(double deg:angles) {
                const Vec d=axis(deg*pi/180);
                if(-dot(seg.n,d)>.02 && std::abs(firstHit(s,sub(p,mul(d,.1)),d)-.1)<2e-6) { visible=true; break; }
            }
            if(visible) { prof.points.push_back(p); prof.normals.push_back(seg.n); prof.cells.push_back({a,b}); }
        }
    }
    if(prof.points.empty()) throw std::runtime_error("No visible exterior cross-section samples.");
    return prof;
}
std::vector<double> heightLevels(bool dense) {
    std::vector<double> h; for(int i=0;i<51;++i) h.push_back(.375+((.575-.375)/50)*i);
    if(dense) { for(int i=1;i<16;++i) h.push_back(.375+.001*i); for(int i=185;i<200;++i) h.push_back(.375+.001*i); }
    return uniqueSorted(h);
}
Grid makeGrid(const Profile& profile,const std::vector<double>& levels,bool copperOnly) {
    Grid grid; grid.heights=levels; std::vector<size_t> ids;
    for(size_t i=0;i<profile.points.size();++i) if(!copperOnly||isCopper(profile.points[i],profile.normals[i])) {
        ids.push_back(i); grid.cells.push_back(profile.cells[i]);
    }
    for(size_t k=0;k<levels.size();++k) {
        const double lo=k?(levels[k-1]+levels[k])*.5:levels.front();
        const double hi=k+1<levels.size()?(levels[k]+levels[k+1])*.5:levels.back();
        for(size_t i:ids) {
            Vec p=profile.points[i]; p[2]=levels[k]; grid.positions.push_back(mul(p,1000)); grid.normals.push_back(profile.normals[i]);
            grid.areas.push_back((hi-lo)*length(sub(profile.cells[i].b,profile.cells[i].a)));
            grid.copper.push_back(isCopper(p,profile.normals[i]));
        }
    }
    if(grid.positions.empty()) throw std::runtime_error("No copper samples in the selected grid.");
    return grid;
}
Plan buildPlan(const Plan& original,const Profile& profile,const Json& config,const Context& ctx) {
    const auto& geo=config.at("geometry");
    const auto knots=config.at("knots_z_m").get<std::vector<double>>();
    const auto W=config.at("weights").get<std::vector<std::vector<double>>>();
    if(geo.size()!=18||W.size()!=36||knots.size()<2) throw std::runtime_error("Invalid native solution dimensions.");
    for(size_t k=0;k<knots.size();++k) if(!std::isfinite(knots[k])||(k&&knots[k]<=knots[k-1])) throw std::runtime_error("Invalid axial knot sequence.");
    for(const auto& w:W) { if(w.size()!=knots.size()) throw std::runtime_error("Invalid dwell profile dimensions."); for(double v:w) if(!std::isfinite(v)||v<.4-1e-8) throw std::runtime_error("Invalid dwell bounds."); }
    Json ordered=geo;
    for(int j=17;j>=0;--j) { Json st=geo[j]; st["xy"][0]=-double(st["xy"][0]); st["yaw"]=-double(st["yaw"]); st["target"][0]=-double(st["target"][0]); ordered.push_back(st); }
    Plan p; p.p.push_back(original.p.front()); p.q.push_back(original.q.front()); p.time.push_back(0); p.on.push_back(0); p.command.push_back(0); p.phase.push_back("park"); p.row.push_back(-1);
    auto append=[&](Vec point,double yaw,double dt,int on,const char* phase,int row,int cmd) {
        if(!(dt>0)) throw std::runtime_error("Nonpositive generated trajectory interval.");
        p.p.push_back(point); p.q.push_back(inverseKinematics(point,yaw)); p.time.push_back(p.time.back()+dt);
        p.on.push_back(on); p.command.push_back(cmd); p.phase.push_back(phase); p.row.push_back(row);
    };
    auto transfer=[&](Vec target,double yaw) {
        const Vec a=p.p.back(), delta=sub(target,a); const double ya=p.q.back()[3], dy=yaw-ya, len=length(delta);
        std::vector<double> angles{ya,yaw};
        for(int k=int(std::ceil(std::min(ya,yaw)/(pi/2)));k<=int(std::floor(std::max(ya,yaw)/(pi/2)));++k) angles.push_back(k*pi/2);
        double bx=0,by=0;
        for(double v:angles) { bx=std::max(bx,std::abs(delta[0]+.08*std::cos(v)*dy)); by=std::max(by,std::abs(delta[1]+.08*std::sin(v)*dy)); }
        double duration=std::max({1.5*len/.2,1.5*bx/.1,1.5*by/.1,1.5*std::abs(delta[2])/.05,1.5*std::abs(dy)/(pi/3),.04,std::sqrt(6*std::max(len,.001)/.6)});
        const int steps=std::max(2,int(std::ceil(duration/.025)));
        for(int k=1;k<=steps;++k) { double f=double(k)/steps,u=f*f*(3-2*f); append(add(a,mul(delta,u)),ya+dy*u,duration/steps,0,"transfer",-1,0); }
    };
    constexpr double v=.05, acc=.15, delay=.1;
    const double ramp=v/acc, rampDistance=v*v/(2*acc);
    const double extension=config.value("footprint_radius_m",.03)+rampDistance+v*delay+.002;
    const double low=profile.low[2]-extension, high=profile.high[2]+extension;
    for(int j=0;j<36;++j) {
        ctx.check(); auto st=ordered[j];
        const double yaw=st["yaw"], x=st["xy"][0],y=st["xy"][1];
        const bool plate=j<10||j>=26;
        if((st.at("region")=="plate")!=plate || (plate&&std::abs(yaw)>1e-10)) throw std::runtime_error("Solution does not preserve source slots and 20 normal scans.");
        const double z0=j%2?high:low,z1=j%2?low:high,sign=z1>z0?1.:-1.; const Vec start{x,y,z0};
        transfer(start,yaw); append(start,yaw,delay,0,"valve_settle",-1,1);
        const double time0=p.time.back(),len=std::abs(z1-z0),duration=2*ramp+(len-2*rampDistance)/v,gate=duration-ramp-delay;
        const size_t first=p.time.size()-1;
        const int steps=std::max(int(std::ceil(len/.001)),int(std::ceil(duration/.04)));
        std::vector<double> grid; for(int k=1;k<=steps;++k) grid.push_back(k*duration/steps);
        grid.push_back(ramp); grid.push_back(gate); grid.push_back(duration-ramp); grid=uniqueSorted(grid);
        double previous=0;
        for(double tau:grid) {
            const double dist=tau<=ramp?.5*acc*tau*tau:tau<=duration-ramp?rampDistance+(tau-ramp)*v:len-.5*acc*(duration-tau)*(duration-tau);
            append({x,y,z0+sign*dist},yaw,tau-previous,tau<=duration-ramp+1e-12,"scan",j,tau<=duration-ramp-delay+1e-12);
            previous=tau;
        }
        st["source"]=j; st["first"]=first; st["last"]=p.time.size()-1; st["z_start"]=z0; st["z_end"]=z1;
        st["time_start"]=time0; st["time_end"]=p.time.back(); st["speed"]=v; st["extension"]=extension;
        st["paint_time_start"]=time0; st["paint_time_end"]=time0+duration-ramp; st["paint_z_start"]=z0; st["paint_z_end"]=z1-sign*rampDistance;
        st["command_close_time"]=time0+gate; st["added"]=false; st["angle_adjusted"]=std::abs(yaw-double(original.strokes[j]["yaw"]))>1e-8;
        p.strokes.push_back(st);
    }
    transfer(original.p.back(),original.q.back()[3]);
    const auto oldTime=p.time; std::vector<double> dt(p.time.size()-1),ds(dt.size());
    for(size_t i=0;i<dt.size();++i) {
        dt[i]=oldTime[i+1]-oldTime[i]; ds[i]=length(sub(p.p[i+1],p.p[i]));
        if(p.row[i+1]>=0) dt[i]*=interpolate((p.p[i][2]+p.p[i+1][2])*.5,knots,W[p.row[i+1]])/.4;
    }
    for(int row=0;row<36;++row) {
        std::vector<size_t> ids; std::vector<double> speed;
        for(size_t i=0;i<dt.size();++i) if(p.row[i+1]==row) {ids.push_back(i); speed.push_back(ds[i]/dt[i]);}
        for(int repeat=0;repeat<3;++repeat) {
            for(size_t k=1;k<ids.size();++k) if(speed[k]>speed[k-1]) {
                const double b=speed[k-1]+.5*acc*ds[ids[k-1]]/std::max(speed[k-1],1e-12);
                speed[k]=std::min(speed[k],.5*(b+std::sqrt(b*b+2*acc*ds[ids[k]])));
            }
            for(int k=int(ids.size())-2;k>=0;--k) if(speed[k]>speed[k+1]) {
                const double b=speed[k+1]+.5*acc*ds[ids[k+1]]/std::max(speed[k+1],1e-12);
                speed[k]=std::min(speed[k],.5*(b+std::sqrt(b*b+2*acc*ds[ids[k]])));
            }
        }
        for(size_t k=0;k<ids.size();++k) dt[ids[k]]=ds[ids[k]]/std::max(speed[k],1e-12);
    }
    p.time[0]=0; for(size_t i=0;i<dt.size();++i) p.time[i+1]=p.time[i]+dt[i];
    for(int j=0;j<36;++j) {
        auto& st=p.strokes[j];
        for(const char* key:{"time_start","time_end","paint_time_start","paint_time_end","command_close_time"}) st[key]=interpolate(st[key],oldTime,p.time);
        st["speed"]=.02/interpolate(.475,knots,W[j]); st["speed_profile_z_m"]=knots;
        std::vector<double> speeds; for(double w:W[j]) speeds.push_back(20/w); st["speed_profile_mm_s"]=speeds;
    }
    validatePlan(p,false); return p;
}
std::vector<double> predict(const Surface& surface,const Grid& grid,const Plan& plan,const Parameters& cfg,
    double step,const Context& ctx,double base,double span) {
    ctx.check(); const auto tri=millimeterMesh(surface);
    std::vector<double> tau(grid.positions.size(),0), last=tau, values=tau;
    for(size_t j=0;j<plan.strokes.size();++j) {
        ctx.check(); const auto samples=spraySamples(plan,plan.strokes[j],step,ctx);
        const auto row=predictSamples(tri,grid,samples,cfg,tau,last);
        for(size_t k=0;k<row.size();++k) values[k]+=row[k];
        if(span && (j+1)%6==0) ctx.report(base+span*(j+1)/plan.strokes.size(),"Thickness prediction "+std::to_string(j+1)+"/36");
    }
    ctx.check(); for(double v:values) if(!std::isfinite(v)||v<0) throw std::runtime_error("Invalid native thickness prediction.");
    return values;
}
Json metrics(const std::vector<double>& values,const Grid& grid,double target,double tolerance) {
    if(values.size()!=grid.positions.size()) throw std::runtime_error("Thickness/grid dimensions differ.");
    double total=0,sum=0,min=1e100,max=-1e100,err=0,within=0,var=0;
    for(size_t i=0;i<values.size();++i) if(grid.copper[i]) {
        const double v=values[i],w=grid.areas[i],e=std::abs(v/target-1)*100;
        total+=w; sum+=v*w; min=std::min(min,v); max=std::max(max,v); err=std::max(err,e); if(e<=tolerance+1e-9) within+=w;
    }
    if(!(total>0)) throw std::runtime_error("No positive copper area.");
    const double mean=sum/total;
    for(size_t i=0;i<values.size();++i) if(grid.copper[i]) var+=(values[i]-mean)*(values[i]-mean)*grid.areas[i];
    return {{"min_um",min},{"max_um",max},{"mean_um",mean},{"cv_percent",std::sqrt(var/total)/std::max(mean,1e-12)*100},
        {"max_relative_error_percent",err},{"within_tolerance_area_percent",within/total*100}};
}
Json refit(const Surface& surface,const Plan& original,const Profile& profile,Json config,const Parameters& cfg,
    double step,double target,double tolerance,const Context& ctx,const Evaluator& evaluate) {
    const auto prof=sampleProfile(surface,original,.0001,ctx); Profile fit=prof;
    fit.points.clear();fit.normals.clear();fit.cells.clear();
    size_t copperIndex=0;
    for(size_t i=0;i<prof.points.size();++i) if(isCopper(prof.points[i],prof.normals[i])) {
        const double x=std::abs(prof.points[i][0]); const size_t k=copperIndex++;
        if((x<.016 && k%2==0)||(x>.055 && k%3==0)||(x>=.016&&x<=.055&&k%8==0)) {
            fit.points.push_back(prof.points[i]);fit.normals.push_back(prof.normals[i]);fit.cells.push_back(prof.cells[i]);
        }
    }
    const Grid grid=makeGrid(fit,heightLevels(true),true); const auto tri=millimeterMesh(surface);
    const auto knots=config.at("knots_z_m").get<std::vector<double>>(); const int K=int(knots.size()),N=int(grid.positions.size());
    const std::vector<int> variable{0,1,2,3,4,5,6,7,8,9,14,15};
    Eigen::MatrixXd A(N,12*K); Eigen::VectorXd constant=Eigen::VectorXd::Zero(N);
    Parameters noHistory=cfg; noHistory[17]=0;
    for(int j=0;j<18;++j) {
        ctx.check(); const auto& st=config.at("geometry")[j];
        const auto it=std::find(variable.begin(),variable.end(),j);
        const int variableIndex=int(it-variable.begin());
        auto basisColumn=[&](int k) {
            std::vector<double> total(N,0);
            for(int sign:{1,-1}) {
                const double yaw=double(st["yaw"])*sign; const Vec d=axis(yaw);
                std::vector<Sample> sam;
                for(int i=0;i<=1400;++i) {
                    const double z=.335+.0002*i;
                    double value=.4;
                    if(k>=0) { std::vector<double> hat(K,0);hat[k]=1;value=interpolate(z,knots,hat); if(value<=1e-12) continue; }
                    sam.push_back({double(st["xy"][0])*sign*1000,double(st["xy"][1])*1000,z*1000,
                        d[0],d[1],0,std::cos(yaw),std::sin(yaw),0,0,0,-1,i*.01,.01*value});
                }
                std::vector<double> tau(N,0),last=tau;
                const auto field=predictSamples(tri,grid,sam,noHistory,tau,last);
                for(int i=0;i<N;++i) total[i]+=field[i];
                ctx.check();
            }
            return total;
        };
        if(it==variable.end()) { const auto c=basisColumn(-1);for(int i=0;i<N;++i) constant[i]+=c[i]; }
        else for(int k=0;k<K;++k) {const auto c=basisColumn(k);for(int i=0;i<N;++i) A(i,variableIndex*K+k)=c[i];}
        ctx.report(.37+.18*(j+1)/18,"Fitting axial dose basis "+std::to_string(j+1)+"/18");
    }
    Eigen::MatrixXd H=A.transpose()*A;
    for(int row=0;row<12;++row) for(int k=0;k<K-1;++k) {
        const int a=row*K+k,b=a+1;H(a,a)+=.25;H(b,b)+=.25;H(a,b)-=.25;H(b,a)-=.25;
    }
    Eigen::VectorXd upper(12*K); for(int row=0;row<12;++row) upper.segment(row*K,K).setConstant(row<10?3.:row==10?6.:4.);
    auto actual=[&](const Json& c) { return evaluate(grid,buildPlan(original,profile,c,ctx),cfg,step,0,0); };
    Json best=config; auto values=actual(best); double bestError=0;
    for(double v:values) bestError=std::max(bestError,std::abs(v/target-1));
    Eigen::VectorXd desired=Eigen::VectorXd::Constant(N,target/(cfg[17]?1.25:1.));
    for(int iteration=0;iteration<5;++iteration) {
        Eigen::VectorXd x;
        if(!boundedSolve(H,A.transpose()*(desired-constant),upper,x,ctx)) {ctx.report(.55,"Bounded solver did not meet KKT tolerance; retaining best verified candidate.");break;}
        std::vector<std::vector<double>> W(18,std::vector<double>(K,.4));
        for(int row=0;row<12;++row) for(int k=0;k<K;++k) W[variable[row]][k]=x[row*K+k];
        auto mirrored=W; for(int j=17;j>=0;--j) mirrored.push_back(W[j]);
        Json candidate=config;candidate["weights"]=mirrored;
        values=actual(candidate); double error=0;for(double v:values) error=std::max(error,std::abs(v/target-1));
        if(error<bestError) {best=candidate;bestError=error;}
        ctx.report(.55+.10*(iteration+1)/5,"Actual history feedback "+std::to_string(iteration+1)+"/5; maximum deviation "+std::to_string(error*100)+"%");
        if(error*100<=tolerance*.9) break;
        for(int i=0;i<N;++i) desired[i]=std::clamp(desired[i]*target/std::max(values[i],1e-8),target*.25,target*2);
    }
    return best;
}
bool boundedSolverRegression() {
    // Correlated variables with one lower bound, one upper bound and one free.
    // Known KKT optimum; also checks release of an initially bound variable.
    Eigen::Matrix3d H;H<<4,1,.5,1,3,.25,.5,.25,2;
    Eigen::Vector3d expected;expected<<.4,2.,1.2;
    Eigen::Vector3d grad;grad<<.7,-.9,0;
    const Eigen::Vector3d g=H*expected-grad;
    Eigen::Vector3d upper;upper<<3,2,4;
    std::atomic_bool stop{false};Context ctx{stop,{}};Eigen::VectorXd x;
    return boundedSolve(H,g,upper,x,ctx) && (x-expected).norm()<1e-8;
}
}
