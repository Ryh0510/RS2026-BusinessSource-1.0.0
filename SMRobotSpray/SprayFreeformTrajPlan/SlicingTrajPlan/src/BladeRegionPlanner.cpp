#include <SlicingTrajPlan/BladeRegionPlanner.h>
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace blade_region {
namespace {
using V = Eigen::Vector3d;
constexpr double inf = std::numeric_limits<double>::infinity();
void check(const std::atomic_bool* c) { if(c && c->load()) throw std::runtime_error("Cancelled."); }
struct Key {
    long long x,y,z;
    bool operator==(const Key& b) const {return x==b.x&&y==b.y&&z==b.z;}
};
struct Hash { std::size_t operator()(const Key& k) const {
    return std::hash<long long>{}(k.x) ^ (std::hash<long long>{}(k.y)*19349663ull) ^
        (std::hash<long long>{}(k.z)*83492791ull); } };
std::uint64_t edgeKey(int a,int b) { if(a>b)std::swap(a,b);return (std::uint64_t(a)<<32)|std::uint32_t(b); }
bool target(Region r) {return int(r)<5;}
V closestTriangle(const V& p,const V& a,const V& b,const V& c) {
    const V ab=b-a,ac=c-a,ap=p-a;const double d1=ab.dot(ap),d2=ac.dot(ap);
    if(d1<=0&&d2<=0)return a;
    const V bp=p-b;const double d3=ab.dot(bp),d4=ac.dot(bp);
    if(d3>=0&&d4<=d3)return b;
    const double vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+d1/(d1-d3)*ab;
    const V cp=p-c;const double d5=ab.dot(cp),d6=ac.dot(cp);
    if(d6>=0&&d5<=d6)return c;
    const double vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+d2/(d2-d6)*ac;
    const double va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(d4-d3)/((d4-d3)+(d5-d6))*(c-b);
    const double sum=va+vb+vc;if(std::abs(sum)<1e-30)return a;
    return a+(vb*ab+vc*ac)/sum;
}
void validate(const Parameters& p) {
    const double values[]={p.endFraction,p.bodyCosine,p.platformCosine,p.sliceSpacing,p.passSpacing,
        p.sampleSpacing,p.firstOffset,p.lastOffset,p.sprayDistance,p.spraySpeed,p.transferSpeed,p.blendLength};
    for(double v:values)if(!std::isfinite(v))throw std::runtime_error("Parameters must be finite.");
    if(p.spanAxis<0||p.spanAxis>3||p.endFraction<=.03||p.endFraction>=.45||p.bodyCosine<.02||
        p.platformCosine>.98||p.platformCosine<=p.bodyCosine+.05||p.sliceSpacing<=0||p.passSpacing<=0||
        p.sampleSpacing<=0||p.sprayDistance<=0||p.spraySpeed<=0||p.transferSpeed<=0||p.blendLength<=0||
        p.firstOffset>p.lastOffset||(p.lastOffset-p.firstOffset)/p.passSpacing>500)
        throw std::runtime_error("Invalid regional planning parameters or excessive offset count.");
    for(auto method:p.methods)if(int(method)<0||int(method)>2)throw std::runtime_error("Invalid regional method.");
}
}

struct Session::Impl {
    Parameters p;
    std::vector<section_sweep::Triangle> raw;
    std::vector<V> v,n;
    std::vector<std::array<int,3>> faces;
    std::vector<std::vector<int>> adj;
    std::vector<Region> labels,vertexLabels;
    std::array<double,7> areas{};
    std::vector<ReferenceBand> bands;
    std::vector<Curve> slices,paths;
    std::array<std::vector<double>,2> referenceDistance;
    section_sweep::Result result;
    std::vector<int> resultRegions;
    V axis=V::UnitZ(),lo=V::Zero(),hi=V::Zero();
    double tmin=0,tmax=0,tol=1e-8;
    bool ready=false,refs=false;
    struct Node {V lo,hi;int begin=0,end=0,left=-1,right=-1;};
    std::vector<Node> bvh;std::vector<int> order;
    void invalidatePaths() {paths.clear();slices.clear();result={};resultRegions.clear();}
    int build(int begin,int end) {
        Node node;node.lo=V::Constant(inf);node.hi=V::Constant(-inf);node.begin=begin;node.end=end;
        for(int j=begin;j<end;++j)for(int k:faces[order[j]]) {node.lo=node.lo.cwiseMin(v[k]);node.hi=node.hi.cwiseMax(v[k]);}
        int at=int(bvh.size());bvh.push_back(node);
        if(end-begin>12) {
            int a; (node.hi-node.lo).maxCoeff(&a);int mid=(begin+end)/2;
            std::nth_element(order.begin()+begin,order.begin()+mid,order.begin()+end,[&](int x,int y){
                return (v[faces[x][0]][a]+v[faces[x][1]][a]+v[faces[x][2]][a]) <
                    (v[faces[y][0]][a]+v[faces[y][1]][a]+v[faces[y][2]][a]);});
            const int l=build(begin,mid),r=build(mid,end);bvh[at].left=l;bvh[at].right=r;
        }return at;
    }
    double boxDistance(const V& q,const Node& b) const {
        return ((b.lo-q).cwiseMax(0.)+(q-b.hi).cwiseMax(0.)).squaredNorm();
    }
    struct Hit {int face=-1;V point=V::Zero(),normal=V::UnitZ();double d2=inf;};
    void nearest(int at,const V& q,Hit& hit,bool targetsOnly) const {
        const auto& node=bvh[at];if(boxDistance(q,node)>hit.d2)return;
        if(node.left>=0){
            int a=node.left,b=node.right;if(boxDistance(q,bvh[a])>boxDistance(q,bvh[b]))std::swap(a,b);
            nearest(a,q,hit,targetsOnly);nearest(b,q,hit,targetsOnly);return;
        }
        for(int j=node.begin;j<node.end;++j){int fi=order[j];if(targetsOnly&&!target(labels[fi]))continue;
            auto f=faces[fi];V c=closestTriangle(q,v[f[0]],v[f[1]],v[f[2]]);double d=(q-c).squaredNorm();
            if(d<hit.d2){hit.face=fi;hit.point=c;hit.d2=d;
                V a=v[f[1]]-v[f[0]],b=v[f[2]]-v[f[0]],w=c-v[f[0]];
                double den=a.dot(a)*b.dot(b)-a.dot(b)*a.dot(b);
                double y=0,z=0;if(den>1e-30){y=(b.dot(b)*a.dot(w)-a.dot(b)*b.dot(w))/den;z=(a.dot(a)*b.dot(w)-a.dot(b)*a.dot(w))/den;}
                hit.normal=((1-y-z)*n[f[0]]+y*n[f[1]]+z*n[f[2]]).normalized();
            }
        }
    }
    Hit project(const V& q,bool targetOnly=true) const {Hit h;if(!bvh.empty())nearest(0,q,h,targetOnly);return h;}
    bool rayNode(int at,const V& a,const V& b) const {
        const auto& node=bvh[at];V d=b-a;double low=0,high=1;
        for(int k=0;k<3;++k){if(std::abs(d[k])<1e-15){if(a[k]<node.lo[k]-tol||a[k]>node.hi[k]+tol)return false;}
            else {double x=(node.lo[k]-a[k])/d[k],y=(node.hi[k]-a[k])/d[k];if(x>y)std::swap(x,y);low=std::max(low,x);high=std::min(high,y);if(low>high)return false;}}
        if(node.left>=0)return rayNode(node.left,a,b)||rayNode(node.right,a,b);
        for(int j=node.begin;j<node.end;++j){auto f=faces[order[j]];V e1=v[f[1]]-v[f[0]],e2=v[f[2]]-v[f[0]],h=d.cross(e2);double det=e1.dot(h);
            if(std::abs(det)<1e-20)continue;double inv=1/det;V s=a-v[f[0]];double u=s.dot(h)*inv;if(u<0||u>1)continue;
            V q=s.cross(e1);double vv=d.dot(q)*inv;if(vv<0||u+vv>1)continue;double t=e2.dot(q)*inv;if(t>1e-6&&t<1-1e-6)return true;}
        return false;
    }
    bool visible(const V& p0,const V& nn) const {return !rayNode(0,p0+nn*std::max(tol*10.,1e-6),p0+nn*p.sprayDistance);}
    std::vector<double> distance(const std::vector<int>& seeds,const std::vector<char>& allow,const std::atomic_bool* cancel) const {
        std::vector<double> d(v.size(),inf);using Entry=std::pair<double,int>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> q;
        for(int i:seeds)if(allow.empty()||allow[i]){d[i]=0;q.push({0,i});}
        std::size_t count=0;
        while(!q.empty()){auto [dd,i]=q.top();q.pop();if(dd!=d[i])continue;if((++count%8192)==0)check(cancel);
            for(int j:adj[i])if(allow.empty()||allow[j]){double x=dd+(v[i]-v[j]).norm();if(x<d[j]){d[j]=x;q.push({x,j});}}}
        return d;
    }
    std::vector<Curve> contours(const std::vector<double>& field,double level,Region region,
        const std::vector<char>& faceAllow,const std::atomic_bool* cancel) const {
        struct Point {V p,n;std::vector<int> edges;};struct Seg {int a,b;bool used=false;};
        std::vector<Point> nodes;std::vector<Seg> segs;std::unordered_map<std::uint64_t,int> ids;
        auto node=[&](int a,int b){double t=(level-field[a])/(field[b]-field[a]);
            auto key=edgeKey(a,b);if(t<1e-9)key=edgeKey(a,a);else if(t>1-1e-9)key=edgeKey(b,b);
            auto f=ids.find(key);if(f!=ids.end())return f->second;int id=int(nodes.size());ids[key]=id;
            V nn=((1-t)*n[a]+t*n[b]).normalized();nodes.push_back({(1-t)*v[a]+t*v[b],nn,{}});return id;};
        for(std::size_t fi=0;fi<faces.size();++fi){if((fi%32768)==0)check(cancel);if(!faceAllow.empty()&&!faceAllow[fi])continue;
            auto f=faces[fi];if(!std::isfinite(field[f[0]])||!std::isfinite(field[f[1]])||!std::isfinite(field[f[2]]))continue;
            std::vector<int> cross;
            for(int k=0;k<3;++k){int a=f[k],b=f[(k+1)%3];if((field[a]<level)!=(field[b]<level))cross.push_back(node(a,b));}
            if(cross.size()==2&&cross[0]!=cross[1]){int i=int(segs.size());segs.push_back({cross[0],cross[1]});for(int x:cross)nodes[x].edges.push_back(i);}}
        std::vector<Curve> out;
        auto walk=[&](int start,int first){Curve c;c.region=region;int at=start,e=first;
            c.points.push_back(nodes[at].p);c.normals.push_back(nodes[at].n);
            while(e>=0&&!segs[e].used){auto& s=segs[e];s.used=true;at=s.a==at?s.b:s.a;c.points.push_back(nodes[at].p);c.normals.push_back(nodes[at].n);
                if(at==start){c.closed=true;break;}e=-1;for(int j:nodes[at].edges)if(!segs[j].used){e=j;break;}}
            double length=0;for(std::size_t i=1;i<c.points.size();++i)length+=(c.points[i]-c.points[i-1]).norm();
            if(length>p.sampleSpacing*2)out.push_back(std::move(c));};
        for(int i=0;i<int(nodes.size());++i)if(nodes[i].edges.size()!=2)for(int e:nodes[i].edges)if(!segs[e].used)walk(i,e);
        for(int i=0;i<int(segs.size());++i)if(!segs[i].used)walk(segs[i].a,i);
        return out;
    }
    std::vector<int> seeds(const std::vector<Curve>& curves) const {
        // Boundary interpolation lies on mesh edges; a nearest mesh vertex per point
        // is sufficient for the edge-graph distance discretization.
        std::vector<int> out;
        for(const auto& c:curves)for(const auto& p0:c.points){auto h=project(p0,false);if(h.face<0)continue;auto f=faces[h.face];int b=f[0];
            for(int j:f)if((v[j]-p0).squaredNorm()<(v[b]-p0).squaredNorm())b=j;out.push_back(b);}
        std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
    }
    Curve sample(const Curve& c) const {
        Curve r;r.region=c.region;r.closed=c.closed;if(c.points.size()<2)return r;
        std::vector<double> s(c.points.size(),0);for(std::size_t i=1;i<s.size();++i)s[i]=s[i-1]+(c.points[i]-c.points[i-1]).norm();
        if(s.back()/p.sampleSpacing>100000)throw std::runtime_error("Path sampling budget exceeded.");
        int count=std::max(1,int(std::ceil(s.back()/p.sampleSpacing)));std::size_t j=1;
        for(int i=0;i<=count;++i){double t=s.back()*i/count;while(j+1<s.size()&&s[j]<t)++j;double a=(t-s[j-1])/std::max(s[j]-s[j-1],1e-20);
            V x=(1-a)*c.points[j-1]+a*c.points[j];auto hit=project(x);if(hit.face<0)continue;
            r.points.push_back(hit.point);r.normals.push_back(hit.normal);}
        return r;
    }
    void fairProcessNormals(Curve& c) const {
        // Smooth the process direction in physical arc length. Merely adding
        // orientation interpolation knots does not repair an oscillating TCP.
        if(c.points.size()<3)return;
        const std::size_t count=c.points.size();std::vector<double> arc(count,0.);
        for(std::size_t i=1;i<count;++i)arc[i]=arc[i-1]+(c.points[i]-c.points[i-1]).norm();
        const bool periodic=(c.points.front()-c.points.back()).norm()<tol*10&&arc.back()>p.sampleSpacing*4;
        const double radius=std::max(p.sampleSpacing*3,std::min(p.passSpacing*1.5,.015));
        const auto raw=c.normals;auto filtered=raw;
        for(std::size_t i=0;i<count;++i){V sum=V::Zero();double weight=0;
            for(int wrap=periodic?-1:0;wrap<=(periodic?1:0);++wrap){const double center=arc[i]+wrap*arc.back();
                auto first=std::lower_bound(arc.begin(),arc.end(),center-radius*2),last=std::upper_bound(arc.begin(),arc.end(),center+radius*2);
                for(auto at=first;at!=last;++at){std::size_t j=std::size_t(at-arc.begin());double ds=std::abs(arc[j]-center);
                    if(raw[j].dot(raw[i])<.5)continue;
                    // Integrate per unit arc length, not per vertex count.
                    double measure=std::max(tol,(j+1<count?arc[j+1]:arc[j])-(j?arc[j-1]:arc[j]));
                    double w=std::exp(-2*ds*ds/(radius*radius))*measure;sum+=w*raw[j];weight+=w;}}
            if(weight<=0||sum.norm()<1e-12)continue;V desired=sum.normalized();
            // Limit departure from the local mesh normal to 20 degrees.
            const double angle=std::acos(std::clamp(raw[i].dot(desired),-1.,1.));
            if(angle>.3490658504)desired=Eigen::Quaterniond::Identity().slerp(.3490658504/angle,Eigen::Quaterniond::FromTwoVectors(raw[i],desired))*raw[i];
            filtered[i]=desired.normalized();
        }
        if(periodic)filtered.back()=filtered.front();c.normals=std::move(filtered);
    }
    std::vector<char> faceMask(Region region) const {std::vector<char> a(labels.size());for(std::size_t i=0;i<a.size();++i)a[i]=labels[i]==region;return a;}
};

Session::Session():m_impl(std::make_unique<Impl>()){}
Session::~Session()=default;
void Session::clear(){m_impl=std::make_unique<Impl>();}
void Session::clearPaths(){m_impl->invalidatePaths();}
void Session::clearConnection(){m_impl->result={};m_impl->resultRegions.clear();}
bool Session::identified()const{return m_impl->ready;}
bool Session::referencesReady()const{return m_impl->refs;}
const Parameters& Session::parameters()const{return m_impl->p;}
const std::vector<section_sweep::Triangle>& Session::mesh()const{return m_impl->raw;}
const std::vector<Region>& Session::labels()const{return m_impl->labels;}
const std::array<double,7>& Session::regionAreas()const{return m_impl->areas;}
const std::vector<ReferenceBand>& Session::bands()const{return m_impl->bands;}
const std::vector<Curve>& Session::slices()const{return m_impl->slices;}
const std::vector<Curve>& Session::paths()const{return m_impl->paths;}
const section_sweep::Result& Session::trajectory()const{return m_impl->result;}
const std::vector<int>& Session::trajectoryRegions()const{return m_impl->resultRegions;}
void Session::setParameters(const Parameters& p){validate(p);auto& d=*m_impl;
    if(d.p.spanAxis!=p.spanAxis||d.p.endFraction!=p.endFraction||d.p.bodyCosine!=p.bodyCosine||d.p.platformCosine!=p.platformCosine){clear();m_impl->p=p;return;}
    d.invalidatePaths();d.p=p;
}

Report Session::identify(const std::vector<section_sweep::Triangle>& mesh,const Parameters& p,const std::atomic_bool* cancel){
    try{validate(p);auto fresh=std::make_unique<Impl>();auto& d=*fresh;d.p=p;
        if(mesh.size()<10||mesh.size()>5000000)throw std::runtime_error("Mesh is empty, too small or exceeds five million triangles.");
        d.lo=V::Constant(inf);d.hi=V::Constant(-inf);
        for(const auto& tri:mesh)for(const auto& v:tri){if(!v.allFinite())throw std::runtime_error("Non-finite mesh coordinates.");d.lo=d.lo.cwiseMin(v);d.hi=d.hi.cwiseMax(v);}
        const double scale=(d.hi-d.lo).norm();if(scale<1e-9)throw std::runtime_error("Degenerate mesh.");d.tol=std::max(1e-10,scale*1e-8);
        std::unordered_map<Key,int,Hash> weld;d.raw=mesh;d.faces.reserve(mesh.size());
        for(std::size_t i=0;i<mesh.size();++i){if(i%32768==0)check(cancel);std::array<int,3> f;
            for(int k=0;k<3;++k){const V q=(mesh[i][k]-d.lo)/d.tol;Key key{llround(q.x()),llround(q.y()),llround(q.z())};auto it=weld.find(key);
                if(it==weld.end()){int id=int(d.v.size());weld.emplace(key,id);d.v.push_back(mesh[i][k]);d.n.push_back(V::Zero());f[k]=id;}else f[k]=it->second;}
            V cross=(mesh[i][1]-mesh[i][0]).cross(mesh[i][2]-mesh[i][0]);if(cross.norm()<d.tol*d.tol)throw std::runtime_error("Degenerate triangles must be repaired before regional planning.");
            d.faces.push_back(f);for(int j:f)d.n[j]+=cross;}
        d.adj.resize(d.v.size());for(auto f:d.faces)for(int k=0;k<3;++k){int a=f[k],b=f[(k+1)%3];d.adj[a].push_back(b);d.adj[b].push_back(a);}
        for(auto& a:d.adj){std::sort(a.begin(),a.end());a.erase(std::unique(a.begin(),a.end()),a.end());}
        for(auto& n:d.n){if(n.norm()<1e-20)throw std::runtime_error("Inconsistent or cancelling vertex normals.");n.normalize();}
        for(int it=0;it<6;++it){check(cancel);auto next=d.n;for(std::size_t i=0;i<d.v.size();++i){V s=V::Zero();for(int j:d.adj[i])if(d.n[i].dot(d.n[j])>0)s+=d.n[j];if(s.norm()>0)next[i]=(d.n[i]+s.normalized()).normalized();}d.n.swap(next);}
        d.axis=p.spanAxis<3?V::Unit(p.spanAxis):V::UnitZ();
        if(p.spanAxis==3){double best=-1;for(int a=0;a<3;++a){double score=0;double low=d.lo[a],span=d.hi[a]-low;
                for(std::size_t i=0;i<d.faces.size();++i){auto f=d.faces[i];V c=(d.v[f[0]]+d.v[f[1]]+d.v[f[2]])/3.;V nn=d.n[f[0]]+d.n[f[1]]+d.n[f[2]];nn.normalize();double t=(c[a]-low)/span;
                    if((t<p.endFraction&&nn[a]>.65)||(t>1-p.endFraction&&nn[a]<-.65))score+=(mesh[i][1]-mesh[i][0]).cross(mesh[i][2]-mesh[i][0]).norm()*std::abs(nn[a]);}
                if(score>best){best=score;d.axis=V::Unit(a);}}}
        d.tmin=inf;d.tmax=-inf;for(const V& x:d.v){d.tmin=std::min(d.tmin,x.dot(d.axis));d.tmax=std::max(d.tmax,x.dot(d.axis));}
        double span=d.tmax-d.tmin;d.vertexLabels.resize(d.v.size());
        // Estimate the normal derivative along the projected span tangent.
        // A leaning/twisted body has an axial normal component too; a fillet
        // additionally turns rapidly towards the inner platform. Fit the local
        // differential in the tangent plane to avoid confusing chord curvature
        // at the leading edge with spanwise transition curvature.
        std::vector<double> turning(d.v.size(),0.);
        for(std::size_t i=0;i<d.v.size();++i){if(i%8192==0)check(cancel);
            V u=d.axis-d.n[i]*d.n[i].dot(d.axis);if(u.norm()<.05){turning[i]=inf;continue;}u.normalize();V vv=d.n[i].cross(u);
            double uu=0,uv=0,vv2=0;V bu=V::Zero(),bv=V::Zero();
            for(int j:d.adj[i]){V e=d.v[j]-d.v[i],dn=d.n[j]-d.n[i];double w=1/std::max(e.squaredNorm(),d.tol*d.tol),a=e.dot(u),b=e.dot(vv);
                uu+=w*a*a;uv+=w*a*b;vv2+=w*b*b;bu+=w*a*dn;bv+=w*b*dn;}
            double den=uu*vv2-uv*uv;if(den>1e-8)turning[i]=-((bu*vv2-bv*uv)/den).dot(d.axis);
        }

        // Average the differential over the mesh before thresholding; raw STL
        // curvature on tiny irregular triangles is otherwise unstable.
        for(auto& k:turning)k=std::max(-100./span,std::min(100./span,k));
        for(int it=0;it<8;++it){check(cancel);auto next=turning;for(std::size_t i=0;i<d.v.size();++i){double sum=turning[i]*2,weight=2;
            for(int j:d.adj[i])if(d.n[i].dot(d.n[j])>.85){sum+=turning[j];weight+=1;}next[i]=sum/weight;}turning.swap(next);}
        for(std::size_t i=0;i<d.v.size();++i){double t=(d.v[i].dot(d.axis)-d.tmin)/span,cn=d.n[i].dot(d.axis);Region r=Region::Body;
            if(t<p.endFraction){if(cn<=-p.platformCosine)r=Region::Excluded;else if(cn>=p.platformCosine)r=Region::LowerInner;else if(cn>p.bodyCosine&&turning[i]>4./span)r=Region::LowerBlend;}
            else if(t>1-p.endFraction){if(cn>=p.platformCosine)r=Region::Excluded;else if(cn<=-p.platformCosine)r=Region::UpperInner;else if(cn<-p.bodyCosine&&turning[i]>4./span)r=Region::UpperBlend;}
            d.vertexLabels[i]=r;}
        // A transition must grow directly from its inner platform. Reject
        // isolated curvature responses on the body before extracting boundaries.
        for(int end=0;end<2;++end){Region blend=end?Region::UpperBlend:Region::LowerBlend,inner=end?Region::UpperInner:Region::LowerInner;
            std::vector<char> seen(d.v.size(),0);std::queue<int> q;
            for(int i=0;i<int(d.v.size());++i)if(d.vertexLabels[i]==inner){seen[i]=1;q.push(i);}
            while(!q.empty()){int i=q.front();q.pop();for(int j:d.adj[i])if(!seen[j]&&d.vertexLabels[j]==blend){seen[j]=1;q.push(j);}}
            for(int i=0;i<int(d.v.size());++i)if(d.vertexLabels[i]==blend&&!seen[i])d.vertexLabels[i]=Region::Body;
        }
        // End rim sidewalls can have the same normal as the blade. Only the
        // body/blend component connected to the central span is a spray target.
        std::vector<char> reached(d.v.size(),0);std::queue<int> flood;
        for(int i=0;i<int(d.v.size());++i){double t=(d.v[i].dot(d.axis)-d.tmin)/span;
            if(t>=p.endFraction&&t<=1-p.endFraction&&d.vertexLabels[i]==Region::Body){reached[i]=1;flood.push(i);}}
        while(!flood.empty()){int i=flood.front();flood.pop();for(int j:d.adj[i])if(!reached[j]&&int(d.vertexLabels[j])<=2){reached[j]=1;flood.push(j);}}
        for(int i=0;i<int(d.v.size());++i)if(int(d.vertexLabels[i])<=2&&!reached[i])d.vertexLabels[i]=Region::Excluded;
        for(int end=0;end<2;++end){Region blend=end?Region::UpperBlend:Region::LowerBlend,inner=end?Region::UpperInner:Region::LowerInner;
            std::fill(reached.begin(),reached.end(),0);
            for(int i=0;i<int(d.v.size());++i)if(d.vertexLabels[i]==blend){reached[i]=1;flood.push(i);}
            while(!flood.empty()){int i=flood.front();flood.pop();for(int j:d.adj[i])if(!reached[j]&&d.vertexLabels[j]==inner){reached[j]=1;flood.push(j);}}
            for(int i=0;i<int(d.v.size());++i)if(d.vertexLabels[i]==inner&&!reached[i])d.vertexLabels[i]=Region::Review;
        }
        d.labels.resize(mesh.size());for(std::size_t i=0;i<mesh.size();++i){auto f=d.faces[i];auto a=d.vertexLabels[f[0]],b=d.vertexLabels[f[1]],c=d.vertexLabels[f[2]];Region r=a==b||a==c?a:(b==c?b:Region::Review);
            d.labels[i]=r;d.areas[int(r)]+=.5*(mesh[i][1]-mesh[i][0]).cross(mesh[i][2]-mesh[i][0]).norm();}
        // Reject unsupported shapes rather than mislabelling a generic STL as a blade.
        if(d.areas[0]<scale*scale*1e-4||d.areas[1]<scale*scale*1e-7||d.areas[2]<scale*scale*1e-7||d.areas[3]<scale*scale*1e-6||d.areas[4]<scale*scale*1e-6)
            throw std::runtime_error("No reliable two-platform blade partition. Check span axis, end search fraction, normals or model repair.");
        d.order.resize(mesh.size());std::iota(d.order.begin(),d.order.end(),0);d.build(0,int(mesh.size()));d.ready=true;
        check(cancel);m_impl=std::move(fresh);return {true,"Partition complete. Body, lower/upper blends, inner platforms, excluded and review faces are retained."};
    }catch(const std::exception& e){return {false,e.what()};}}

Report Session::fitReferences(const std::atomic_bool* cancel){try{auto& d=*m_impl;if(!d.ready)throw std::runtime_error("Identify regions first.");
    std::vector<ReferenceBand> bands;std::array<std::vector<double>,2> distances;
    for(int end=0;end<2;++end){check(cancel);Region reg=end?Region::UpperBlend:Region::LowerBlend;std::vector<double> scalar(d.v.size());std::vector<char> allow(d.v.size()),faceAllow(d.faces.size());
        for(std::size_t i=0;i<d.v.size();++i){double t=(d.v[i].dot(d.axis)-d.tmin)/(d.tmax-d.tmin);bool local=end?t>1-d.p.endFraction:t<d.p.endFraction;
            scalar[i]=(end?-1:1)*d.n[i].dot(d.axis);allow[i]=target(d.vertexLabels[i])&&(d.vertexLabels[i]==Region::Body||d.vertexLabels[i]==reg||d.vertexLabels[i]==(end?Region::UpperInner:Region::LowerInner));
            // The body-side boundary follows the accepted turning band;
            // axial normal thresholds alone would cut a tilted body by height.
            if(d.vertexLabels[i]==Region::Body)scalar[i]=std::min(scalar[i],d.p.bodyCosine-1e-3);
            if(!local)scalar[i]=std::numeric_limits<double>::quiet_NaN();}
        for(std::size_t i=0;i<d.faces.size();++i){auto f=d.faces[i];faceAllow[i]=allow[f[0]]&&allow[f[1]]&&allow[f[2]];}
        ReferenceBand band;band.region=reg;
        band.bodyBoundary=d.contours(scalar,d.p.bodyCosine,reg,faceAllow,cancel);
        band.platformBoundary=d.contours(scalar,d.p.platformCosine,reg,faceAllow,cancel);
        if(band.bodyBoundary.empty()||band.platformBoundary.empty())throw std::runtime_error("Two transition boundaries could not be identified; adjust axis/thresholds or repair mesh.");
        std::vector<char> bandAllow(d.v.size(),0);
        for(std::size_t i=0;i<d.faces.size();++i)if(d.labels[i]==reg)for(int j:d.faces[i])bandAllow[j]=1;
        const auto interior=bandAllow;
        for(std::size_t i=0;i<interior.size();++i)if(interior[i])for(int j:d.adj[i])if(allow[j])bandAllow[j]=1;
        auto da=d.distance(d.seeds(band.bodyBoundary),bandAllow,cancel),db=d.distance(d.seeds(band.platformBoundary),bandAllow,cancel);
        std::vector<double> midpoint(d.v.size(),inf);
        for(std::size_t i=0;i<midpoint.size();++i)if(std::isfinite(scalar[i])&&scalar[i]>=d.p.bodyCosine&&scalar[i]<=d.p.platformCosine&&std::isfinite(da[i])&&std::isfinite(db[i]))midpoint[i]=da[i]-db[i];
        // Include both boundary edge triangles while restricting to this end.
        for(std::size_t i=0;i<midpoint.size();++i)if(allow[i]&&std::isfinite(scalar[i])&&std::isfinite(da[i])&&std::isfinite(db[i]))midpoint[i]=da[i]-db[i];
        band.reference=d.contours(midpoint,0,reg,d.faceMask(reg),cancel);
        if(band.reference.empty())throw std::runtime_error("Surface midpoint fitting failed: disconnected transition boundaries.");
        for(auto& c:band.reference){
            // Smooth locally, reproject and keep the fit in the same transition band.
            for(int iter=0;iter<3;++iter){auto next=c.points;for(std::size_t i=1;i+1<c.points.size();++i){auto hit=d.project((c.points[i-1]+2*c.points[i]+c.points[i+1])/4.);
                    if(hit.face>=0&&d.labels[hit.face]==reg){next[i]=hit.point;c.normals[i]=hit.normal;}}c.points.swap(next);}}
        auto dr=d.distance(d.seeds(band.reference),allow,cancel);
        for(std::size_t i=0;i<dr.size();++i)if(std::isfinite(dr[i])&&da[i]<db[i])dr[i]=-dr[i];
        distances[end]=std::move(dr);bands.push_back(std::move(band));}
    check(cancel);d.invalidatePaths();d.bands=std::move(bands);d.referenceDistance=std::move(distances);d.refs=true;
    return {true,"Identified body-side and platform-side boundaries; fitted surface geodesic midpoint references."};
    }catch(const std::exception& e){return {false,e.what()};}}

Report Session::generate(Region region,const std::atomic_bool* cancel){try{auto& d=*m_impl;if(!d.ready||!target(region))throw std::runtime_error("Select an identified target region.");
    Method method=d.p.methods[int(region)];if(method==Method::Disabled)throw std::runtime_error("Region is disabled.");
    auto mask=d.faceMask(region);std::vector<Curve> generated,newSlices;
    if(method==Method::ReferenceOffsets){if(!d.refs)throw std::runtime_error("Fit both transition references first.");
        int end=(region==Region::UpperBlend||region==Region::UpperInner)?1:0;
        const auto& field=d.referenceDistance[end];
        if(region==Region::Body)throw std::runtime_error("Body uses slicing; select a transition/platform region for reference offsets.");
        std::vector<double> offsets;for(double offset=d.p.firstOffset;offset<=d.p.lastOffset+d.tol;offset+=d.p.passSpacing)offsets.push_back(offset);
        // A narrow band must retain its center pass when the requested range spans zero.
        if(d.p.firstOffset<=0&&d.p.lastOffset>=0)offsets.push_back(0.);
        std::sort(offsets.begin(),offsets.end());offsets.erase(std::unique(offsets.begin(),offsets.end(),[&](double a,double b){return std::abs(a-b)<d.tol;}),offsets.end());
        for(double offset:offsets){check(cancel);auto cs=d.contours(field,offset,region,mask,cancel);for(auto& c:cs)generated.push_back(d.sample(c));}
    }else{
        V cutting=d.axis;if(region==Region::LowerInner||region==Region::UpperInner){int a;d.axis.cwiseAbs().maxCoeff(&a);cutting=V::Unit((a+1)%3);}
        std::vector<double> field(d.v.size());double low=inf,high=-inf;
        for(std::size_t i=0;i<d.v.size();++i)field[i]=d.v[i].dot(cutting);
        for(std::size_t i=0;i<d.faces.size();++i)if(mask[i])for(int j:d.faces[i]){low=std::min(low,field[j]);high=std::max(high,field[j]);}
        if((high-low)/d.p.sliceSpacing>1000)throw std::runtime_error("Slice count exceeds 1000; increase slice spacing.");
        std::vector<std::vector<Curve>> layers;
        for(double z=low+d.tol*5;z<high-d.tol;z+=d.p.sliceSpacing){auto cs=d.contours(field,z,region,mask,cancel);for(const auto& c:cs)newSlices.push_back(c);layers.push_back(std::move(cs));}
        if(region!=Region::Body){for(const auto& cs:layers)for(const auto& c:cs)generated.push_back(d.sample(c));}
        else{
            // Feature anchors: minimum/maximum chord coordinate, then track the two sides independently.
            int spanAxis;d.axis.cwiseAbs().maxCoeff(&spanAxis);int chordAxis=(spanAxis+1)%3;
            V ext=d.hi-d.lo;if(ext[(spanAxis+2)%3]>ext[chordAxis])chordAxis=(spanAxis+2)%3;
            std::array<std::vector<Curve>,2> branches;
            for(const auto& layer:layers){std::array<Curve,2> pair;for(auto& c:pair)c.region=region;
                if(layer.size()==1&&layer[0].closed){auto c=layer[0];if(c.points.size()>2)c.points.pop_back();int a=0,b=0;
                    for(int i=0;i<int(c.points.size());++i){if(c.points[i][chordAxis]<c.points[a][chordAxis])a=i;if(c.points[i][chordAxis]>c.points[b][chordAxis])b=i;}
                    for(int side=0;side<2;++side){int at=a;pair[side].points.push_back(c.points[at]);pair[side].normals.push_back(c.normals[at]);
                        while(at!=b){at=(at+(side==0?1:int(c.points.size())-1))%int(c.points.size());pair[side].points.push_back(c.points[at]);pair[side].normals.push_back(c.normals[at]);}}
                }else if(layer.size()==2){pair[0]=layer[0];pair[1]=layer[1];for(auto& c:pair)if(c.points.front()[chordAxis]>c.points.back()[chordAxis]){std::reverse(c.points.begin(),c.points.end());std::reverse(c.normals.begin(),c.normals.end());}
                }else { // Unreliable correspondence: retain honest sliced paths rather than jump across components.
                    for(const auto& c:layer)generated.push_back(d.sample(c));for(auto& b:branches)b.push_back(Curve{});continue;}
                if(!branches[0].empty()&&!branches[0].back().points.empty()){
                    V prev=branches[0].back().points[branches[0].back().points.size()/2];
                    if((pair[1].points[pair[1].points.size()/2]-prev).squaredNorm()<(pair[0].points[pair[0].points.size()/2]-prev).squaredNorm())std::swap(pair[0],pair[1]);}
                for(int side=0;side<2;++side)branches[side].push_back(std::move(pair[side]));}
            for(auto& side:branches){double maxLength=0;for(const auto& c:side){double len=0;for(std::size_t j=1;j<c.points.size();++j)len+=(c.points[j]-c.points[j-1]).norm();maxLength=std::max(maxLength,len);}
                int count=std::max(1,int(std::ceil(maxLength/d.p.passSpacing)));if(count>500)throw std::runtime_error("Too many body passes.");
                for(int pass=0;pass<=count;++pass){Curve track;track.region=region;double fraction=double(pass)/count;
                    auto flush=[&](){if(track.points.size()>1)generated.push_back(d.sample(track));track=Curve{};track.region=region;};
                    for(const auto& c:side){if(c.points.size()<2){flush();continue;}std::vector<double> s(c.points.size(),0);for(std::size_t j=1;j<s.size();++j)s[j]=s[j-1]+(c.points[j]-c.points[j-1]).norm();
                        double t=fraction*s.back();auto it=std::lower_bound(s.begin()+1,s.end(),t);std::size_t j=std::size_t(it-s.begin());double u=(t-s[j-1])/std::max(s[j]-s[j-1],1e-20);
                        auto hit=d.project((1-u)*c.points[j-1]+u*c.points[j]);if(hit.face<0||d.labels[hit.face]!=region){flush();continue;}
                        if(!track.points.empty()&&(track.points.back()-hit.point).norm()>d.p.sliceSpacing*3)flush();track.points.push_back(hit.point);track.normals.push_back(hit.normal);}
                    flush();}}
        }
    }
    for(auto& c:generated)d.fairProcessNormals(c);
    // Visibility and region clipping use the full mesh, not just the selected face subset.
    std::vector<Curve> clipped;
    for(const auto& c:generated){Curve run;run.region=region;auto flush=[&](){if(run.points.size()>1)clipped.push_back(std::move(run));run=Curve{};run.region=region;};
        for(std::size_t j=0;j<c.points.size();++j){check(cancel);auto hit=d.project(c.points[j],false);
            if(hit.face<0||d.labels[hit.face]!=region||!d.visible(c.points[j],c.normals[j])){flush();continue;}
            if(!run.points.empty()&&d.rayNode(0,run.points.back()+d.p.sprayDistance*run.normals.back(),c.points[j]+d.p.sprayDistance*c.normals[j]))flush();
            run.points.push_back(c.points[j]);run.normals.push_back(c.normals[j]);}flush();}
    if(clipped.empty())throw std::runtime_error("No visible paths in the selected region/offset interval; adjust parameters and inspect labels.");
    std::vector<Curve> distinct;
    for(auto& c:clipped){bool duplicate=false;
        for(const auto& prior:distinct){if(prior.points.size()!=c.points.size())continue;
            bool same=true,flipped=true;for(std::size_t i=0;i<c.points.size()&&(same||flipped);++i){
                same&=(c.points[i]-prior.points[i]).norm()<d.tol*10;
                flipped&=(c.points[i]-prior.points[c.points.size()-1-i]).norm()<d.tol*10;}
            if(same||flipped){duplicate=true;break;}}
        if(!duplicate)distinct.push_back(std::move(c));}
    clipped=std::move(distinct);
    check(cancel);d.result={};d.resultRegions.clear();
    d.paths.erase(std::remove_if(d.paths.begin(),d.paths.end(),[&](const Curve& c){return c.region==region;}),d.paths.end());
    d.slices.erase(std::remove_if(d.slices.begin(),d.slices.end(),[&](const Curve& c){return c.region==region;}),d.slices.end());
    d.paths.insert(d.paths.end(),clipped.begin(),clipped.end());d.slices.insert(d.slices.end(),newSlices.begin(),newSlices.end());
    return {true,"Generated "+std::to_string(clipped.size())+" visible regional paths; generate other regions then connect."};
    }catch(const std::exception& e){return {false,e.what()};}}

Report Session::connect(const std::atomic_bool* cancel){try{auto& d=*m_impl;if(d.paths.empty())throw std::runtime_error("Generate regional paths first.");
    section_sweep::Result result;std::vector<int> regions;result.sprayDistance=d.p.sprayDistance;int pass=0;V previousTangent=V::UnitX();
    auto append=[&](const V& position,const V& normal,const V& tangent,bool spray,int region){
        V z=-normal.normalized(),x=tangent-z*tangent.dot(z);if(x.norm()<1e-8)x=previousTangent-z*previousTangent.dot(z);if(x.norm()<1e-8)x=z.unitOrthogonal();x.normalize();
        if(!result.points.empty()&&x.dot(previousTangent)<0)x=-x;previousTangent=x;
        section_sweep::Point p;p.pose.translation()=position;p.pose.linear().col(0)=x;p.pose.linear().col(1)=z.cross(x).normalized();p.pose.linear().col(2)=z;
        p.spray=spray;p.pass=pass;p.speed=spray?d.p.spraySpeed:d.p.transferSpeed;
        if(!result.points.empty()) {
            const auto prev=result.points.back();Eigen::Quaterniond q0(prev.pose.linear()),q1(p.pose.linear());
            double angle=Eigen::AngleAxisd(prev.pose.linear().transpose()*p.pose.linear()).angle();
            const double length=(prev.pose.translation()-position).norm();
            const int count=std::max(1,int(std::ceil(std::max(length/d.p.sampleSpacing,angle/.0872664626))));
            const double duration=std::max({length/p.speed,angle/1.0,1e-5});
            for(int i=1;i<count;++i){double u=double(i)/count;auto mid=p;
                mid.pose.translation()=prev.pose.translation()*(1-u)+position*u;
                mid.pose.linear()=q0.slerp(u,q1).toRotationMatrix();mid.time=prev.time+duration*u;
                result.points.push_back(mid);regions.push_back(region);}
            p.time=prev.time+duration;
        }
        result.points.push_back(p);regions.push_back(region);
        if(result.points.size()>2000000)throw std::runtime_error("Connected output exceeds two million points.");};
    std::vector<char> used(d.paths.size(),0);std::size_t current=0;bool reverse=false;int smoothJoins=0,airJoins=0;
    for(std::size_t step=0;step<d.paths.size();++step){check(cancel);Curve c=d.paths[current];used[current]=1;if(reverse){std::reverse(c.points.begin(),c.points.end());std::reverse(c.normals.begin(),c.normals.end());}
        if(c.points.size()<2)continue;
        V start=c.points.front()+d.p.sprayDistance*c.normals.front();
        if(!result.points.empty()){
            const auto prev=result.points.back();V end=prev.pose.translation();V n0=-prev.pose.linear().col(2),n1=c.normals.front();
            V dir0=result.points.size()>1?V(end-result.points[result.points.size()-2].pose.translation()):previousTangent;
            if(dir0.norm()<1e-9)dir0=previousTangent;dir0.normalize();
            V dir1=c.points[1]+d.p.sprayDistance*c.normals[1]-start;if(dir1.norm()<1e-9)dir1=c.points[1]-c.points[0];dir1.normalize();double length=(start-end).norm();
            struct Sample {V x,n,t;};std::vector<Sample> bridge;bool surface=length<d.p.blendLength*3;
            int count=std::max(8,int(std::ceil(length/d.p.sampleSpacing)));if(count>10000)throw std::runtime_error("Connection sampling budget exceeded.");
            auto hermite=[&](double u,const V& a,const V& b,const V& ta,const V& tb){return V((2*u*u*u-3*u*u+1)*a+(u*u*u-2*u*u+u)*ta+(-2*u*u*u+3*u*u)*b+(u*u*u-u*u)*tb);};
            V a=end-d.p.sprayDistance*n0,b=c.points.front();
            V prevX=end;
            for(int i=1;i<count&&surface;++i){double u=double(i)/count;V q=hermite(u,a,b,dir0*std::min(length,d.p.blendLength),dir1*std::min(length,d.p.blendLength));auto hit=d.project(q,false);
                if(hit.face<0||!target(d.labels[hit.face])||hit.d2>std::pow(d.p.sampleSpacing*2,2)||!d.visible(hit.point,hit.normal)){surface=false;break;}
                V normal=Eigen::Quaterniond::Identity().slerp(u,Eigen::Quaterniond::FromTwoVectors(n0,n1))*n0;
                if(normal.dot(hit.normal)<.8660254||!d.visible(hit.point,normal)){surface=false;break;}
                V x=hit.point+d.p.sprayDistance*normal;
                if((x-prevX).norm()>d.p.sampleSpacing*6||d.rayNode(0,prevX,x)){surface=false;break;}
                bridge.push_back({x,normal,dir0*(1-u)+dir1*u});prevX=x;}
            if(surface&& !d.rayNode(0,prevX,start)){
                for(const auto& q:bridge)append(q.x,q.n,q.t,true,int(c.region));++smoothJoins;
            }else{
                // Cubic Bezier with tangent-matched endpoints, lifted into free space.
                // Reject instead of exporting a transfer that intersects the full workpiece.
                bool safe=false;std::vector<Sample> air;
                for(int attempt=0;attempt<7&&!safe;++attempt){air.clear();double clearance=std::min(d.p.blendLength,std::max(d.p.sampleSpacing*2,length*.25));
                    double lift=attempt==0?0.:clearance*std::pow(1.6,attempt-1);V up=n0+n1;if(up.norm()<.1)up=(end+start)/2-(d.lo+d.hi)/2;up.normalize();
                    const double handle=std::min(d.p.blendLength,length*.3);
                    V ca=end+dir0*handle,cb=start-dir1*handle;prevX=end;safe=true;
                    int nc=std::max(count,int(std::ceil((length+4*lift)/d.p.sampleSpacing)));
                    for(int i=1;i<nc;++i){check(cancel);double u=double(i)/nc;double v1=1-u;
                        V x=v1*v1*v1*end+3*v1*v1*u*ca+3*v1*u*u*cb+u*u*u*start+16*u*u*v1*v1*lift*up;
                        V nn=(n0*(1-u)+n1*u);if(nn.norm()<.01)nn=n0;nn.normalize();
                        if(d.rayNode(0,prevX,x)){safe=false;break;}air.push_back({x,nn,dir0*(1-u)+dir1*u});prevX=x;}
                    if(safe&&d.rayNode(0,prevX,start))safe=false;}
                if(!safe){
                    // Visibility graph outside an expanded bounding box handles
                    // concave platform transfers that a single lifted cubic cannot.
                    for(int attempt=0;attempt<8&&!safe;++attempt){
                        const double handle=std::min(d.p.blendLength,d.p.sampleSpacing*2)*std::pow(.4,attempt);
                        V depart=end+dir0*handle,arrive=start-dir1*handle;
                        if(d.rayNode(0,end,depart)||d.rayNode(0,arrive,start))continue;
                        std::vector<V> nodes{depart,arrive};V margin=V::Constant(std::max(d.p.blendLength,d.p.sampleSpacing*2)*(1.+attempt*.5));
                        V low=d.lo-margin,high=d.hi+margin,center=(low+high)*.5;
                        for(int ix=-1;ix<=1;++ix)for(int iy=-1;iy<=1;++iy)for(int iz=-1;iz<=1;++iz){
                            if(ix==0&&iy==0&&iz==0)continue;nodes.push_back(center+V(ix*(high.x()-low.x())*.5,iy*(high.y()-low.y())*.5,iz*(high.z()-low.z())*.5));}
                        for(double height:{.1,.25,.5,1.,2.,4.}){
                            nodes.push_back(end+n0*d.p.sprayDistance*height);nodes.push_back(start+n1*d.p.sprayDistance*height);}
                        // Local clearance probes around both TCPs can reach
                        // narrow platform mouths before routing to the box.
                        for(const auto& base:{end,start})for(int a=0;a<3;++a)for(int sign:{-1,1})nodes.push_back(base+sign*V::Unit(a)*d.p.sprayDistance*.5);
                        std::vector<double> cost(nodes.size(),inf);std::vector<int> parent(nodes.size(),-1);std::vector<char> done(nodes.size(),0);cost[0]=0;
                        for(std::size_t round=0;round<nodes.size();++round){check(cancel);int at=-1;double best=inf;
                            for(int j=0;j<int(nodes.size());++j)if(!done[j]&&cost[j]<best){best=cost[j];at=j;}
                            if(at<0||at==1)break;done[at]=1;
                            for(int j=0;j<int(nodes.size());++j)if(!done[j]){double next=best+(nodes[at]-nodes[j]).norm();
                                if(next<cost[j]&&!d.rayNode(0,nodes[at],nodes[j])){cost[j]=next;parent[j]=at;}}}
                        if(!std::isfinite(cost[1]))continue;
                        std::vector<V> poly;for(int at=1;at>=0;at=parent[at])poly.push_back(nodes[at]);std::reverse(poly.begin(),poly.end());
                        poly.insert(poly.begin(),end);poly.push_back(start);
                        // Trim each corner with a quadratic Bezier. Decrease trim
                        // until every sampled segment clears the original mesh.
                        for(int trim=0;trim<8&&!safe;++trim){std::vector<V> samples{end};double fraction=.22*std::pow(.4,trim);
                            auto line=[&](const V& b){V a=samples.back();int n=std::max(1,int(std::ceil((b-a).norm()/d.p.sampleSpacing)));for(int j=1;j<=n;++j)samples.push_back(a+(b-a)*(double(j)/n));};
                            for(std::size_t j=1;j+1<poly.size();++j){V a=poly[j]+(poly[j-1]-poly[j])*fraction,b=poly[j]+(poly[j+1]-poly[j])*fraction;
                                line(a);int n=std::max(4,int(std::ceil(((a-poly[j]).norm()+(b-poly[j]).norm())/d.p.sampleSpacing)));
                                for(int k=1;k<=n;++k){double u=double(k)/n;samples.push_back((1-u)*(1-u)*a+2*u*(1-u)*poly[j]+u*u*b);}}
                            line(start);safe=true;for(std::size_t j=1;j<samples.size();++j)if(d.rayNode(0,samples[j-1],samples[j])){safe=false;break;}
                            if(safe){air.clear();Eigen::Quaterniond rotation=Eigen::Quaterniond::FromTwoVectors(n0,n1);double total=0;
                                for(std::size_t j=1;j<samples.size();++j)total+=(samples[j]-samples[j-1]).norm();double travelled=0;
                                for(std::size_t j=1;j+1<samples.size();++j){travelled+=(samples[j]-samples[j-1]).norm();double u=travelled/std::max(total,1e-20);
                                    air.push_back({samples[j],Eigen::Quaterniond::Identity().slerp(u,rotation)*n0,samples[j+1]-samples[j]});}}
                        }
                    }
                }
                if(!safe)throw std::runtime_error("No collision-free smooth transfer at path "+std::to_string(step)+", region "+std::to_string(int(c.region))+"; inspect paths or reduce spray distance.");
                for(const auto& q:air)append(q.x,q.n,q.t,false,int(c.region));append(start,n1,dir1,false,int(c.region));++airJoins;
            }
        }
        for(std::size_t i=0;i<c.points.size();++i){V tangent=i+1<c.points.size()?V(c.points[i+1]-c.points[i]):V(c.points[i]-c.points[i-1]);
            V x=c.points[i]+d.p.sprayDistance*c.normals[i];
            if(i&&d.rayNode(0,result.points.back().pose.translation(),x))throw std::runtime_error("Offset spray path intersects the full mesh; adjust distance or sample spacing.");
            append(x,c.normals[i],tangent,true,int(c.region));}
        ++pass;double best=inf;std::size_t next=current;bool rev=false;
        const V last=c.points.back()+d.p.sprayDistance*c.normals.back();
        for(std::size_t j=0;j<d.paths.size();++j)if(!used[j]&&d.paths[j].points.size()>1){
            for(int k=0;k<2;++k){const auto& candidate=d.paths[j];std::size_t at=k?candidate.points.size()-1:0;
                const V endpoint=candidate.points[at]+d.p.sprayDistance*candidate.normals[at];
                const double length=(last-endpoint).norm();
                const double angular=std::acos(std::clamp(c.normals.back().dot(candidate.normals[at]),-1.,1.));
                const double regionPenalty=candidate.region==c.region?0.:d.p.blendLength;
                // Order actual gun positions, preserving nearby strokes and
                // discouraging opposite-face jumps across a thin blade.
                double dd=length+d.p.sprayDistance*.25*angular+regionPenalty;
                if(dd<best){best=dd;next=j;rev=k!=0;}}}
        current=next;reverse=rev;
    }
    result.passCount=pass;result.success=true;result.message="Connected "+std::to_string(pass)+" paths; "+std::to_string(smoothJoins)+" surface blends and "+std::to_string(airJoins)+" spray-off smooth transfers. Workpiece geometry checked; robot/tool validation is downstream.";
    check(cancel);d.result=std::move(result);d.resultRegions=std::move(regions);return {true,d.result.message};
    }catch(const std::exception& e){return {false,e.what()};}}
}
