// SMROBOT_PARENT_BAFFLE_COMPAT
#include <SlicingTrajPlan/VirtualBaffle.h>
#include <SlicingTrajPlan/UniformSlicer.h>
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ModelTrajPlan {
namespace {
using V2 = Eigen::Vector2d;
using V3 = Eigen::Vector3d;
constexpr double eps = 1e-9;
double cross(const V2& a, const V2& b) { return a.x()*b.y()-a.y()*b.x(); }
double distance(const V2& p, const V2& a, const V2& b) {
    const V2 edge = b-a;
    if(edge.squaredNorm() < eps*eps) return (p-a).norm();
    return (p-a-std::clamp((p-a).dot(edge)/edge.squaredNorm(),0.,1.)*edge).norm();
}
bool inside(const std::vector<V2>& ring, const V2& p) {
    bool hit = false;
    for(size_t i=0,j=ring.size()-1;i<ring.size();j=i++) {
        const auto& a=ring[i]; const auto& b=ring[j];
        if((a.y()>p.y()) != (b.y()>p.y()) &&
            p.x() < (b.x()-a.x())*(p.y()-a.y())/(b.y()-a.y())+a.x()) hit=!hit;
    }
    return hit;
}
bool segments(const V2& a,const V2& b,const V2& c,const V2& d,double margin) {
    const double u=cross(b-a,c-a),v=cross(b-a,d-a);
    const double w=cross(d-c,a-c),z=cross(d-c,b-c);
    if(((u>eps && v<-eps)||(u<-eps && v>eps)) &&
       ((w>eps && z<-eps)||(w<-eps && z>eps))) return true;
    return std::min({distance(a,c,d),distance(b,c,d),distance(c,a,b),distance(d,a,b)}) <= margin+eps;
}
std::vector<V3> resample(const BoundaryLine& line) {
    std::vector<V3> points;
    for(const auto& p:line.points) {
        if(!p.position.allFinite()) throw std::runtime_error("nonfinite boundary");
        if(points.empty() || (p.position-points.back()).norm()>eps) points.push_back(p.position);
    }
    if(points.size()>1 && (points.front()-points.back()).norm()<eps) points.pop_back();
    if(points.size()<3) throw std::runtime_error("degenerate boundary");
    std::vector<double> cumulative{0};
    for(size_t i=0;i<points.size();++i) cumulative.push_back(cumulative.back()+(points[(i+1)%points.size()]-points[i]).norm());
    if(cumulative.back()<eps) throw std::runtime_error("zero boundary length");
    std::vector<V3> out; size_t edge=0;
    for(int i=0;i<256;++i) {
        const double s=i*cumulative.back()/256.;
        while(edge+1<points.size() && cumulative[edge+1]<s) ++edge;
        const double t=(s-cumulative[edge])/(cumulative[edge+1]-cumulative[edge]);
        out.push_back((1-t)*points[edge]+t*points[(edge+1)%points.size()]);
    }
    return out;
}
std::vector<V3> clip(const std::vector<V3>& input,const VirtualBaffle& b,double sign,double half) {
    std::vector<V3> out;
    if(input.empty()) return out;
    V3 prev=input.back(); double dp=sign*b.normal.dot(prev-b.center)-half;
    for(const auto& p:input) {
        const double d=sign*b.normal.dot(p-b.center)-half;
        if((d<=0)!=(dp<=0)) out.push_back(prev+dp/(dp-d)*(p-prev));
        if(d<=0) out.push_back(p);
        prev=p;dp=d;
    }
    return out;
}
}

VirtualBaffle buildVirtualBaffle(const ModelData& model,double thickness,double fitTolerance) {
    VirtualBaffle b; b.thickness=thickness;
    try {
        if(!std::isfinite(thickness)||thickness<=0||!std::isfinite(fitTolerance)||fitTolerance<=0)
            throw std::runtime_error("invalid thickness or fit tolerance");
        BoundaryDetector detector; BoundaryData boundary; Epiphyseal ignored;
        if(!detector.detectBoundaries(model,boundary,ignored,&b.topology)||!b.topology.usable())
            throw std::runtime_error("boundary_topology: expected two closed unbranched manifold rims; loops=" +
                std::to_string(b.topology.loopCount) + ", branch_vertices=" +
                std::to_string(b.topology.branchVertices) + ", open_vertices=" +
                std::to_string(b.topology.openVertices) + ", nonmanifold_edges=" +
                std::to_string(b.topology.nonManifoldEdges));
        const auto a=resample(boundary.lowerBoundary_),c=resample(boundary.upperBoundary_);
        V3 ca=V3::Zero(),cc=V3::Zero();
        for(size_t i=0;i<a.size();++i){ca+=a[i];cc+=c[i];} ca/=a.size();cc/=c.size();
        double sa=0,sc=0; for(size_t i=0;i<a.size();++i){sa+=(a[i]-ca).squaredNorm();sc+=(c[i]-cc).squaredNorm();}
        sa=std::sqrt(sa/a.size());sc=std::sqrt(sc/c.size());
        if(sa<eps||sc<eps) throw std::runtime_error("degenerate rim size");
        struct Match { double score; int shift,sign; }; std::vector<Match> matches;
        const int n=static_cast<int>(a.size());
        for(int sign:{1,-1})for(int shift=0;shift<n;++shift){double score=0;
            for(int i=0;i<n;++i) score+=((a[i]-ca)/sa-(c[(shift+sign*i+n)%n]-cc)/sc).squaredNorm();
            matches.push_back({score/n,shift,sign});}
        std::sort(matches.begin(),matches.end(),[](const auto& x,const auto& y){return x.score<y.score;});
        const auto best=matches.front();
        b.correspondenceRms=std::sqrt(best.score);
        // Equivalent cyclic matches are valid for symmetric rims. The actual
        // mesh section, fit residual and topology below decide usability.
        if(!std::isfinite(b.correspondenceRms))
            throw std::runtime_error("invalid_boundary_correspondence");
        std::vector<V3> mids; b.center.setZero();
        for(int i=0;i<n;++i){mids.push_back((a[i]+c[(best.shift+best.sign*i+n)%n])*.5);b.center+=mids.back();}b.center/=n;
        Eigen::Matrix3d covariance=Eigen::Matrix3d::Zero();
        for(const auto& p:mids){const V3 d=p-b.center;covariance+=d*d.transpose();} covariance/=n;
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> solver(covariance);
        if(solver.info()!=Eigen::Success || solver.eigenvalues()[1]<1e-12)
            throw std::runtime_error("degenerate midpoint plane fit");
        b.normal=solver.eigenvectors().col(0).normalized();
        Eigen::Index largest; b.normal.cwiseAbs().maxCoeff(&largest);if(b.normal[largest]<0)b.normal=-b.normal;
        b.axisX=b.normal.unitOrthogonal().normalized();b.axisY=b.normal.cross(b.axisX).normalized();
        double sum=0;for(const auto& p:mids){double r=std::abs(b.normal.dot(p-b.center));sum+=r*r;b.maxResidual=std::max(b.maxResidual,r);}
        b.rmsResidual=std::sqrt(sum/n);
        if(b.maxResidual>fitTolerance) throw std::runtime_error("midpoint plane exceeds fit tolerance");
        size_t invalid=0;
        const auto sections=UniformSlicer::traceContoursFromIntersections(model,SlicePlane{b.center,b.normal},false,&invalid);
        if(invalid || sections.empty()) throw std::runtime_error("open, branched or missing plane section");
        for(const auto& section:sections){std::vector<V2> ring;
            for(const auto& p:section.points){const V3 d=p.position-b.center;V2 uv(d.dot(b.axisX),d.dot(b.axisY));if(ring.empty()||(uv-ring.back()).norm()>eps)ring.push_back(uv);}
            if(ring.size()>1 && (ring.front()-ring.back()).norm()<eps)ring.pop_back();
            if(ring.size()<3)throw std::runtime_error("degenerate aperture section");
            double area=0;for(size_t i=0;i<ring.size();++i)area+=cross(ring[i],ring[(i+1)%ring.size()]);
            if(std::abs(area)<1e-12)throw std::runtime_error("zero aperture area");
            b.contours.push_back(std::move(ring));}
        // Reject self-intersecting or mutually crossing aperture boundaries.
        for(size_t r=0;r<b.contours.size();++r)for(size_t s=r;s<b.contours.size();++s){
            const auto& x=b.contours[r];const auto& y=b.contours[s];
            for(size_t i=0;i<x.size();++i)for(size_t j=0;j<y.size();++j){
                if(r==s&&(i==j||(i+1)%x.size()==j||(j+1)%x.size()==i))continue;
                if(segments(x[i],x[(i+1)%x.size()],y[j],y[(j+1)%y.size()],0))
                    throw std::runtime_error("crossing or touching aperture contours");}}
        b.enabled=true;
        b.diagnostic=b.maxResidual>thickness
            ? "enabled_with_fit_warning: residual exceeds slab thickness; infinite slab uses the least-squares plane"
            : "enabled: infinite slab, closed-section even-odd aperture";
    }catch(const std::exception& e){b.enabled=false;b.diagnostic=e.what();}
    return b;
}

bool VirtualBaffle::apertureContains(const V2& point) const {
    if(!enabled || !point.allFinite()) return false;
    bool aperture=false;
    for(const auto& ring:contours) if(ring.size()>=3) {
        for(size_t i=0;i<ring.size();++i)
            if(distance(point,ring[i],ring[(i+1)%ring.size()])<=eps) return false;
        aperture^=inside(ring,point);
    }
    return aperture;
}

bool VirtualBaffle::intersectsTriangle(const Triangle& triangle,double clearance) const {
    if(!enabled)return false;
    if(!std::isfinite(clearance)||clearance<0)return true;
    for(const auto& p:triangle)if(!p.allFinite())return true;
    auto poly=clip({triangle[0],triangle[1],triangle[2]},*this,1,thickness*.5+clearance);
    poly=clip(poly,*this,-1,thickness*.5+clearance);if(poly.empty())return false;
    std::vector<V2> uv;
    for(const auto& p:poly){const V3 d=p-center;uv.emplace_back(d.dot(axisX),d.dot(axisY));}
    for(const auto& p:uv){bool aperture=false;for(const auto& ring:contours)aperture^=inside(ring,p);if(!aperture)return true;}
    for(const auto& ring:contours){for(size_t i=0;i<ring.size();++i){const auto& a=ring[i];const auto& b=ring[(i+1)%ring.size()];
        for(size_t j=0;j<uv.size();++j)if(segments(a,b,uv[j],uv[(j+1)%uv.size()],clearance))return true;
        if(uv.size()>=3&&inside(uv,a))return true;}}
    return false;
}

std::vector<VirtualBaffle::Triangle> VirtualBaffle::displayTriangles(double extent) const {
    std::vector<Triangle> out;
    if(!enabled||!std::isfinite(extent)||extent<=0)return out;
    for(const auto& ring:contours)for(const auto& p:ring)extent=std::max(extent,p.cwiseAbs().maxCoeff()*1.1+eps);
    std::vector<double> heights{-extent,extent};
    for(const auto& ring:contours)for(const auto& p:ring)heights.push_back(p.y());
    std::sort(heights.begin(),heights.end());heights.erase(std::unique(heights.begin(),heights.end(),[](double a,double b){return std::abs(a-b)<eps;}),heights.end());
    const auto point=[&](double x,double y)->V3{return center+x*axisX+y*axisY;};
    for(size_t k=1;k<heights.size();++k){const double low=heights[k-1],high=heights[k],mid=(low+high)*.5;if(high-low<eps)continue;
        struct Edge{V2 a,b;double x;};std::vector<Edge> edges;
        for(const auto& ring:contours)for(size_t i=0;i<ring.size();++i){const auto& a=ring[i];const auto& b=ring[(i+1)%ring.size()];
            if((a.y()>mid)!=(b.y()>mid))edges.push_back({a,b,a.x()+(b.x()-a.x())*(mid-a.y())/(b.y()-a.y())});}
        std::sort(edges.begin(),edges.end(),[](const auto& a,const auto& b){return a.x<b.x;});
        if(edges.size()%2)continue;
        const auto xAt=[](const Edge& e,double y){return e.a.x()+(e.b.x()-e.a.x())*(y-e.a.y())/(e.b.y()-e.a.y());};
        for(size_t j=0;j<=edges.size();j+=2){const double l0=j?xAt(edges[j-1],low):-extent,l1=j?xAt(edges[j-1],high):-extent;
            const double r0=j<edges.size()?xAt(edges[j],low):extent,r1=j<edges.size()?xAt(edges[j],high):extent;
            const V3 a=point(l0,low),b=point(r0,low),c=point(r1,high),d=point(l1,high);
            if((b-a).cross(c-a).norm()>eps*eps)out.push_back({a,b,c});
            if((c-a).cross(d-a).norm()>eps*eps)out.push_back({a,c,d});}
    }
    return out;
}
}
