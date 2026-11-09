#include "SectionCoverage.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <tuple>

namespace section_sweep::detail
{
    namespace
    {
        constexpr double pi=3.14159265358979323846;
        struct V2 {
            double x,y;
            V2 operator+(V2 b)const{return {x+b.x,y+b.y};}
            V2 operator-(V2 b)const{return {x-b.x,y-b.y};}
            V2 operator*(double s)const{return {x*s,y*s};}
            double dot(V2 b)const{return x*b.x+y*b.y;}
            double cross(V2 b)const{return x*b.y-y*b.x;}
            double norm()const{return std::hypot(x,y);}
        };
        struct Edge {V2 a,b,n;};
        struct Sample {V2 p,n;};
        struct Candidate {V2 target,normal;};

        // Exact 2D cross-section occlusion for an already validated extrusion.
        bool blocked(V2 a,V2 b,const std::vector<Edge>& edges,double bias)
        {
            const double dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
            const Edge* finish=edges.data()+edges.size();
            for(const Edge* edge=edges.data();edge!=finish;++edge) {
                const double ex=edge->b.x-edge->a.x,ey=edge->b.y-edge->a.y;
                const double sx=edge->a.x-a.x,sy=edge->a.y-a.y;
                const double det=dx*ey-dy*ex;
                if(std::abs(det)<1.e-18)continue;
                const double t=(sx*ey-sy*ex)/det,u=(sx*dy-sy*dx)/det;
                if(u>=-1.e-9 && u<=1.+1.e-9 && t*length>bias && t<1.-1.e-8)return true;
            }
            return false;
        }

        float visibility(const Sample& s,const Candidate& c,double distance,
            const std::vector<Edge>& edges)
        {
            const V2 nozzle{c.target.x+c.normal.x*distance,c.target.y+c.normal.y*distance};
            const double dx=s.p.x-nozzle.x,dy=s.p.y-nozzle.y;
            const double axial=-(dx*c.normal.x+dy*c.normal.y),length=std::hypot(dx,dy);
            if(axial<=0. || length<=1.e-12)return 0.f;
            const double incidence=-(s.n.x*dx+s.n.y*dy)*(1./length);
            if(incidence<=.02)return 0.f;
            // Same finite 5%-peak angular footprint as the webpage's RS2026
            // paper-Gaussian model. No deposition/history/thickness objective.
            const double phi=std::atan2(-dx*c.normal.y+dy*c.normal.x,axial)-.0002884115775;
            const double psi=.002083376641;
            static const double cp=std::cos(-.04494047503),sp=std::sin(-.04494047503);
            const double u=(cp*phi+sp*psi)/.02793467718;
            const double v=(-sp*phi+cp*psi)/.03265327731;
            const double pattern=std::exp(-.5*(u*u+v*v));
            if(pattern<.05 || blocked(s.p,nozzle,edges,1.e-5))return 0.f;
            return static_cast<float>(pattern*incidence);
        }
    }

    bool fitExteriorCoverage(const std::vector<SectionSegment>& section,
        const Eigen::Vector3d& scan,const Eigen::Vector3d& side,
        const Parameters& p,std::vector<ScanPass>& passes,Result& result)
    {
        const Eigen::Vector3d lateral=side.cross(scan);
        const double station=section.front().a.dot(scan);
        const auto xy=[&](const Eigen::Vector3d& a){return V2{a.dot(lateral),a.dot(side)};};
        const auto xyz=[&](V2 a)->Eigen::Vector3d{return a.x*lateral+a.y*side+station*scan;};
        std::vector<Edge> edges;
        double minX=std::numeric_limits<double>::infinity(),maxX=-minX;
        double minY=minX,maxY=-minX,plate=-minX,longest=0.;
        for(const auto& s:section) {
            const Edge e{xy(s.a),xy(s.b),xy(s.normal)};edges.push_back(e);
            for(auto a:{e.a,e.b}) {
                minX=std::min(minX,a.x);maxX=std::max(maxX,a.x);
                minY=std::min(minY,a.y);maxY=std::max(maxY,a.y);
            }
            if(e.n.y>1.-1.e-6 && (e.b-e.a).norm()>longest) {
                longest=(e.b-e.a).norm();plate=.5*(e.a.y+e.b.y);
            }
        }
        if(!std::isfinite(plate))plate=minY;
        const double extent=std::hypot(maxX-minX,maxY-minY),outside=2*extent+p.sprayDistance;
        const double center=.5*(minX+maxX);
        const auto mirror=[&](Candidate c){c.target.x=2*center-c.target.x;c.normal.x=-c.normal.x;return c;};
        std::vector<Sample> samples;
        // Resolve narrow root lips at 25 micrometres, independently of axial
        // playback sampling and the pass-spacing control.
        for(const auto& e:edges) {
            const double countValue=std::ceil((e.b-e.a).norm()/.000025);
            if(countValue>100000) {result.message="Cross-section coverage sampling exceeds the geometry budget.";return false;}
            const auto count=std::max<std::size_t>(1,static_cast<std::size_t>(countValue));
            for(std::size_t k=0;k<count;++k) {
                const V2 a=e.a+(e.b-e.a)*((k+.5)/count);
                if(a.y<plate-1.e-8)continue; // The selected plate side, never its back/edge.
                bool visible=false;
                // Scope is physical exterior access; a reduced user tilt must
                // not silently delete hard-to-reach roots from the denominator.
                for(double degrees:{0.,-15.,15.,-30.,30.,-45.,45.,-60.,60.,-75.,75.,-85.,85.,-88.,88.}) {
                    const double angle=degrees*pi/180.;const V2 n{-std::sin(angle),std::cos(angle)};
                    if(e.n.dot(n)>.02 && !blocked(a,a+n*outside,edges,2.e-6)) {visible=true;break;}
                }
                if(visible)samples.push_back({a,e.n});
                if(samples.size()>100000) {result.message="Too many exterior coverage samples.";return false;}
            }
        }
        if(samples.empty()){result.message="No visible exterior samples on the selected side.";return false;}
        std::vector<ScanPass> fixed;
        std::size_t slots=0;
        for(const auto& pass:passes) {
            if(pass.normal.dot(side)>1.-1.e-6)fixed.push_back(pass);
            else ++slots;
        }
        std::vector<unsigned char> covered(samples.size(),0);
        std::vector<float> bestStrength(samples.size(),0.f);
        const Sample* sampleData=samples.data();
        auto* coveredData=covered.data();auto* bestData=bestStrength.data();
        for(const auto& pass:fixed) {
            const Candidate c{xy(pass.target),xy(pass.normal)};
            for(std::size_t i=0;i<samples.size();++i) {
                const float strength=visibility(sampleData[i],c,p.sprayDistance,edges);
                coveredData[i]|=strength>0;bestData[i]=std::max(bestData[i],strength);
            }
        }
        bool symmetric=slots>0 && slots%2==0;
        for(const auto& e:edges) {
            const V2 a{2*center-.5*(e.a.x+e.b.x),.5*(e.a.y+e.b.y)},n{-e.n.x,e.n.y};
            bool found=false;
            for(const auto& f:edges) {
                const V2 d=f.b-f.a;
                const double u=std::clamp((a-f.a).dot(d)/d.dot(d),0.,1.);
                if((a-(f.a+d*u)).norm()<2.e-7 && n.dot(f.n)>1.-1.e-5){found=true;break;}
            }
            if(!found){symmetric=false;break;}
        }
        std::vector<Candidate> candidates;
        std::set<std::tuple<long long,long long,int,int>> bins;
        if(slots)for(const auto& s:samples) {
            if(symmetric && s.p.x>=center)continue;
            // Preserve the old planar groups; candidate aim points belong to
            // the curved/junction portion, including recessed flat surfaces.
            if(s.n.y>1.-1.e-6 && std::abs(s.p.y-plate)<1.e-8)continue;
            const auto key=std::make_tuple(static_cast<long long>(std::floor((s.p.x-center)/.00035)),
                static_cast<long long>(std::floor((s.p.y-plate)/.00035)),
                static_cast<int>(std::round(s.n.x*1000)),static_cast<int>(std::round(s.n.y*1000)));
            if(!bins.insert(key).second)continue;
            std::vector<double> angles{0.,10.,20.,30.,45.,60.,70.,72.5,75.,77.5,80.,85.,88.,p.maximumTiltDegrees};
            if(!symmetric) {
                const auto positives=angles;for(double angle:positives)if(angle>0)angles.push_back(-angle);
            }
            std::sort(angles.begin(),angles.end());
            angles.erase(std::unique(angles.begin(),angles.end()),angles.end());
            for(double degrees:angles) {
                if(std::abs(degrees)>p.maximumTiltDegrees+1.e-9)continue;
                const double angle=degrees*pi/180.;const V2 n{-std::sin(angle),std::cos(angle)};
                if(s.n.dot(n)<=.02 || blocked(s.p,s.p+n*p.sprayDistance,edges,2.e-6) ||
                    blocked(s.p,s.p+n*outside,edges,2.e-6))continue;
                const Candidate c{s.p,n};
                if(symmetric) {
                    const auto m=mirror(c);
                    if(blocked(m.target,m.target+m.normal*p.sprayDistance,edges,2.e-6))continue;
                }
                candidates.push_back(c);
            }
        }
        if(candidates.size()*static_cast<double>(samples.size())>5.e7) {
            result.message="Coverage candidate budget exceeded; simplify the section or increase pass spacing.";return false;
        }
        std::vector<std::vector<float>> strength(candidates.size(),std::vector<float>(samples.size()));
        std::vector<std::size_t> alternatives(samples.size(),0);
        auto* alternativesData=alternatives.data();
        for(std::size_t j=0;j<candidates.size();++j) {
            const auto& candidate=candidates[j];const auto paired=mirror(candidate);
            auto* row=strength[j].data();
            for(std::size_t i=0;i<samples.size();++i) {
                float value=visibility(sampleData[i],candidate,p.sprayDistance,edges);
                if(symmetric)value+=visibility(sampleData[i],paired,p.sprayDistance,edges);
                row[i]=value;if(value>0)++alternativesData[i];
            }
        }
        const std::size_t selections=symmetric?slots/2:slots;
        std::vector<std::size_t> selected;
        for(std::size_t k=0;k<selections;++k) {
            const bool missing=std::find(covered.begin(),covered.end(),0)!=covered.end();
            double best=-1.;std::size_t chosen=candidates.size();
            for(std::size_t j=0;j<candidates.size();++j) {
                if(std::find(selected.begin(),selected.end(),j)!=selected.end())continue;
                double score=0.;const auto* row=strength[j].data();
                for(std::size_t i=0;i<samples.size();++i) {
                    if(missing) {if(!coveredData[i] && row[i]>0)score+=1./std::max<std::size_t>(1,alternativesData[i]);}
                    else score+=std::max(0.f,row[i]-bestData[i]);
                }
                if(score>best){best=score;chosen=j;}
            }
            if(chosen==candidates.size())break;
            selected.push_back(chosen);
            const auto* row=strength[chosen].data();
            for(std::size_t i=0;i<samples.size();++i) {
                coveredData[i]|=row[i]>0;bestData[i]=std::max(bestData[i],row[i]);
            }
        }
        result.exteriorSamples=samples.size();
        result.coveredSamples=static_cast<std::size_t>(std::count(covered.begin(),covered.end(),1));
        if(result.coveredSamples!=result.exteriorSamples || selected.size()!=selections) {
            result.message="The retained scan-pass budget cannot cover the selected exterior at this standoff/tilt. Increase maximum tilt or reduce pass spacing.";
            return false;
        }
        std::vector<Candidate> fitted;
        for(auto j:selected)fitted.push_back(candidates[j]);
        if(symmetric) {
            std::stable_sort(fitted.begin(),fitted.end(),[](const Candidate& a,const Candidate& b){
                return std::atan2(-a.normal.x,a.normal.y)>std::atan2(-b.normal.x,b.normal.y);
            });
            const auto half=fitted;
            for(auto i=half.rbegin();i!=half.rend();++i)fitted.push_back(mirror(*i));
        } else std::stable_sort(fitted.begin(),fitted.end(),[](const Candidate& a,const Candidate& b){return a.target.x<b.target.x;});
        std::size_t j=0;
        for(auto& pass:passes)if(pass.normal.dot(side)<=1.-1.e-6) {
            const auto& c=fitted[j++];pass={xyz(c.target),xyz(c.normal)-station*scan};
            result.maximumUsedTiltDegrees=std::max(result.maximumUsedTiltDegrees,std::acos(std::clamp(c.normal.y,-1.,1.))*180./pi);
        }
        return true;
    }
}
