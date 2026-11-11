// CPU execution of RS2026 SpatialFilteredGaussianHistoryShader equations.
// Geometry and thickness are in millimeters; time is in seconds.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <numeric>
#include <thread>
#include <vector>

struct V {
    double x,y,z;
    V operator+(V b)const{return {x+b.x,y+b.y,z+b.z};}
    V operator-(V b)const{return {x-b.x,y-b.y,z-b.z};}
    V operator*(double s)const{return {x*s,y*s,z*s};}
    double operator[](int i)const{return i==0?x:i==1?y:z;}
};
static V load(const double* p){return {p[0],p[1],p[2]};}
static double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static V cross(V a,V b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static double norm(V a){return std::sqrt(dot(a,a));}
struct Tri {V a,e1,e2,lo,hi,center;};
struct Node {V lo,hi;int first,count,left=-1,right=-1;};
struct Mesh {
    std::vector<Tri> tris;std::vector<int> order;std::vector<Node> nodes;
    Mesh(const double* data,int count){
        for(int i=0;i<count;i++){
            V a=load(data+9*i),b=load(data+9*i+3),c=load(data+9*i+6);
            tris.push_back({a,b-a,c-a,
                {std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})},
                {std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})},(a+b+c)*(1./3)});
        }
        order.resize(count);std::iota(order.begin(),order.end(),0);if(count)build(0,count);
    }
    int build(int first,int count){
        Node n{{1e100,1e100,1e100},{-1e100,-1e100,-1e100},first,count};
        for(int i=first;i<first+count;i++){
            const Tri&t=tris[order[i]];
            n.lo={std::min(n.lo.x,t.lo.x),std::min(n.lo.y,t.lo.y),std::min(n.lo.z,t.lo.z)};
            n.hi={std::max(n.hi.x,t.hi.x),std::max(n.hi.y,t.hi.y),std::max(n.hi.z,t.hi.z)};
        }
        int index=int(nodes.size());nodes.push_back(n);
        if(count>6){V d=n.hi-n.lo;int axis=d.x>d.y?(d.x>d.z?0:2):(d.y>d.z?1:2),mid=first+count/2;
            std::nth_element(order.begin()+first,order.begin()+mid,order.begin()+first+count,[&](int a,int b){return tris[a].center[axis]<tris[b].center[axis];});
            int l=build(first,mid-first),r=build(mid,first+count-mid);nodes[index].left=l;nodes[index].right=r;
        }return index;
    }
    bool box(const Node&n,V o,V d,double length,double bias)const{
        double near=bias,far=length-bias;
        for(int k=0;k<3;k++){
            if(std::abs(d[k])<1e-15){if(o[k]<n.lo[k]-1e-9||o[k]>n.hi[k]+1e-9)return false;continue;}
            double a=(n.lo[k]-o[k])/d[k],b=(n.hi[k]-o[k])/d[k];if(a>b)std::swap(a,b);
            near=std::max(near,a);far=std::min(far,b);if(near>far+1e-9)return false;
        }return true;
    }
    bool occluded(V o,V d,double length,double bias)const{
        if(nodes.empty())return false;int stack[128],size=1;stack[0]=0;
        while(size){const Node&n=nodes[stack[--size]];if(!box(n,o,d,length,bias))continue;
            if(n.left>=0){stack[size++]=n.left;stack[size++]=n.right;continue;}
            for(int j=n.first;j<n.first+n.count;j++){
                const Tri&t=tris[order[j]];V h=cross(d,t.e2);double det=dot(t.e1,h);if(std::abs(det)<1e-7)continue;
                V s=o-t.a;double u=dot(s,h)/det;if(u<0||u>1)continue;V q=cross(s,t.e1);double v=dot(d,q)/det;if(v<0||u+v>1)continue;
                double hit=dot(t.e2,q)/det;if(hit>bias&&hit<length-bias)return true;
            }
        }return false;
    }
};
static double distanceEfficiency(double mm){double v=std::log(std::max(mm,1e-6)/106.8615)/.0839;return .9789+.1456*std::exp(-v*v);}
static double angleEfficiency(double angle){return std::max(1e-6,(40.81414+.73808*angle-.000838598*angle*angle)*.01);}

// params: amplitude,rotation,phiOffset,psiOffset,sigmaPhi,sigmaPsi,referenceMm,
// referenceAngle,referenceExposure,processScale,historyAmplitude,historyScale,
// referenceHistory,coolingTime,activityThreshold,cutoff,shadowBias,history,bvh,flip.
extern "C" void predict_row(
    const double* triangles,int nt,const double* positions,const double* normals,int nv,
    const double* samples,int ns,const int* checkpoints,int nc,const double* cfg,
    double* tau,double* lastTime,float* output,int threads){
    const Mesh mesh(triangles,nt);const double cp=std::cos(cfg[1]),sp=std::sin(cfg[1]);
    const double referenceProjection=std::max(1e-6,std::sin(cfg[7]*3.14159265358979323846/180.));
    const double refDistanceEfficiency=distanceEfficiency(cfg[6]),refAngleEfficiency=angleEfficiency(cfg[7]);
    const double peakScale=cfg[0]*cfg[9]/std::max(cfg[8],1e-6);
    // This bound rejects only points that are certainly below the same cutoff.
    const double angularBound=std::sqrt(-2*std::log(cfg[15]))*std::max(cfg[4],cfg[5]);
    // Pure axial rows admit exact range rejection before the per-sample loop.
    // The same angular box already rejects every omitted sample below.
    bool axialRow=ns>0, fixedXY=ns>0;std::vector<double> sortedZ(ns);
    const bool ascending=ns<2||samples[14*(ns-1)+2]>=samples[2];
    for(int j=0;j<ns;j++){
        const double*s=samples+14*j;sortedZ[j]=ascending?s[2]:-s[2];
        if(std::abs(s[5])>1e-14||std::abs(s[8])>1e-14||std::abs(s[9])>1e-14||std::abs(s[10])>1e-14||std::abs(std::abs(s[11])-1)>1e-14)axialRow=false;
        for(int k=0;k<12;k++)if(k!=2&&std::abs(s[k]-samples[k])>1e-14)axialRow=false;
        if(j&&sortedZ[j]<sortedZ[j-1])axialRow=false;
        // Exact equality is deliberate: this fast path never approximates a
        // moving or tilted tool frame. The general per-sample path stays valid.
        if(s[5]!=0||s[8]!=0||s[9]!=0||s[10]!=0||std::abs(s[11])!=1)fixedXY=false;
        for(int k=0;k<12;k++)if(k!=2&&s[k]!=samples[k])fixedXY=false;
    }
    fixedXY=fixedXY&&axialRow;
    const double limitU=std::tan(angularBound+std::abs(cfg[2]));
    const double limitW=std::tan(angularBound+std::abs(cfg[3]));
    const double coolingTime=std::max(cfg[13],1e-6);
    const double historyScale=std::max(cfg[11],1e-6);
    const double historyReference=std::exp(-cfg[12]/historyScale);
    std::vector<double> adjacentCooling(ns);
    if(cfg[17])for(int j=1;j<ns;j++)
        adjacentCooling[j]=std::exp(-std::max(0.,samples[14*j+12]-samples[14*(j-1)+12])/coolingTime);
    auto work=[&](int begin,int end){for(int v=begin;v<end;v++){
        // For an axial row, axial distance and the horizontal angular bound
        // are constant for this vertex. Reject once, rather than repeating
        // the identical rejection for every axial exposure sample. No dose
        // means no change to either history state variable.
        double rowAxial=0,rowPhi=0;
        if(fixedXY){
            const V delta=load(positions+v*3)-load(samples);
            rowAxial=dot(delta,load(samples+3));
            const double u=dot(delta,load(samples+6));
            if(!(rowAxial>.001)||!(std::abs(u)<=rowAxial*limitU)){
                for(int c=0;c<nc;c++)output[c*nv+v]=0;
                continue;
            }
            rowPhi=std::atan2(u,std::max(rowAxial,1e-6))-cfg[2];
        }
        V p=load(positions+v*3),n=load(normals+v*3);double nlength=norm(n);n=n*(1/std::max(nlength,1e-12));
        double accumulated=0,t=tau[v],last=lastTime[v];int checkpoint=0;
        int first=0,stop=ns;
        if(axialRow){
            const double axial=dot(p-load(samples),load(samples+3));
            const double limit=std::max(0.,axial)*std::tan(angularBound+std::abs(cfg[3]))+1e-9;
            const double center=ascending?p.z:-p.z;
            first=int(std::lower_bound(sortedZ.begin(),sortedZ.end(),center-limit)-sortedZ.begin());
            stop=int(std::upper_bound(sortedZ.begin(),sortedZ.end(),center+limit)-sortedZ.begin());
            while(checkpoint<nc&&checkpoints[checkpoint]<=first){output[checkpoint*nv+v]=0;checkpoint++;}
        }
        for(int j=first;j<stop;j++){
            const double*s=samples+14*j;V delta=p-load(s),axis=load(s+3);double axial=fixedXY?rowAxial:dot(delta,axis);
            if(axial>.001){
                double u=dot(delta,load(s+6)),w=dot(delta,load(s+9));
                if((fixedXY||std::abs(u)<=axial*limitU) && std::abs(w)<=axial*limitW){
                    double phi=fixedXY?rowPhi:std::atan2(u,std::max(axial,1e-6))-cfg[2],psi=std::atan2(w,std::max(axial,1e-6))-cfg[3];
                    double x=(cp*phi+sp*psi)/cfg[4],y=(-sp*phi+cp*psi)/cfg[5],pattern=std::exp(-.5*(x*x+y*y));
                    if(pattern>=cfg[15]){
                        double dist=norm(delta);V ray=delta*(-1/std::max(dist,1e-6));double incidence=dot(n,ray);if(cfg[19])incidence=std::abs(incidence);
                        if(incidence>0&&(!cfg[18]||!mesh.occluded(p,ray,dist,cfg[16]))){
                            double angle=90-std::acos(std::clamp(incidence,-1.,1.))*180/3.14159265358979323846;
                            double geometry=cfg[6]*cfg[6]/std::max(dist*dist,1e-12)*std::max(1e-6,incidence)/referenceProjection;
                            double peak=peakScale*geometry*distanceEfficiency(dist)/refDistanceEfficiency*angleEfficiency(angle)/refAngleEfficiency*std::max(1e-6,s[13]);
                            double base=peak*pattern;
                            if(base>0){
                                double factor=1;
                                if(cfg[17]){
                                    // Reuse the exact same adjacent-sample exponential.
                                    // A gap or a previous-pass state takes the general path.
                                    t*=(j>0&&last==samples[14*(j-1)+12])?adjacentCooling[j]:
                                        std::exp(-std::max(0.,s[12]-last)/coolingTime);
                                    if(base>=cfg[14]*peak)t+=s[13];
                                    factor=std::max(1.,1+cfg[10]*(std::exp(-t/historyScale)-historyReference));
                                }
                                accumulated+=base*factor;last=s[12];
                            }
                        }
                    }
                }
            }
            while(checkpoint<nc&&checkpoints[checkpoint]==j+1){output[checkpoint*nv+v]=float(accumulated);checkpoint++;}
        }while(checkpoint<nc){output[checkpoint*nv+v]=float(accumulated);checkpoint++;}tau[v]=t;lastTime[v]=last;
    }};
    if(nv<=0)return;
    threads=std::clamp(threads,1,std::min(nv,16));
    // Small chunks balance narrow spray footprints without sharing any
    // vertex's chronological accumulator between workers.
    std::atomic<int> next{0};
    auto worker=[&](){
        for(;;){const int begin=next.fetch_add(128,std::memory_order_relaxed);
            if(begin>=nv)break;work(begin,std::min(begin+128,nv));}
    };
    std::vector<std::thread> pool;
    for(int i=1;i<threads;i++)pool.emplace_back(worker);
    worker();for(auto&t:pool)t.join();
}

extern "C" void occlusion_test(const double* triangles,int nt,const double* origins,const double* directions,const double* lengths,int count,unsigned char* result){
    Mesh mesh(triangles,nt);for(int i=0;i<count;i++)result[i]=mesh.occluded(load(origins+3*i),load(directions+3*i),lengths[i],.01);
}
