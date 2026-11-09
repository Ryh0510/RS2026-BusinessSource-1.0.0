#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>
#include "SectionCoverage.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace section_sweep
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr std::size_t maximumOutputPoints = 500000;
        struct RayTriangle
        {
            double a[3],e1[3],e2[3];
            explicit RayTriangle(const Triangle& tri) {
                for(int k=0;k<3;++k) {a[k]=tri[0][k];e1[k]=tri[1][k]-tri[0][k];e2[k]=tri[2][k]-tri[0][k];}
            }
        };

        std::vector<SectionSegment> cut(const std::vector<Triangle>& mesh,
            const Eigen::Vector3d& axis, double station, double tolerance)
        {
            std::vector<SectionSegment> result;
            for(const auto& triangle : mesh) {
                Eigen::Vector3d normal = (triangle[1]-triangle[0]).cross(triangle[2]-triangle[0]);
                if(normal.norm() <= tolerance*tolerance) continue;
                normal.normalize();
                if(std::abs(normal.dot(axis)) > 1.-1.e-8) continue;
                std::vector<Eigen::Vector3d> hits;
                const auto add = [&](const Eigen::Vector3d& point) {
                    if(std::none_of(hits.begin(),hits.end(),[&](const auto& old) {
                        return (old-point).norm() <= tolerance;
                    })) hits.push_back(point);
                };
                for(int edge=0;edge<3;++edge) {
                    const auto& a=triangle[edge]; const auto& b=triangle[(edge+1)%3];
                    const double da=a.dot(axis)-station, db=b.dot(axis)-station;
                    if(std::abs(da)<=tolerance) add(a);
                    if((da>tolerance && db<-tolerance)||(db>tolerance && da<-tolerance))
                        add(a+(b-a)*(da/(da-db)));
                }
                if(hits.size()==2 && (hits[0]-hits[1]).norm()>tolerance)
                    result.push_back({hits[0],hits[1],normal});
            }
            // Remove tessellation seams without changing corners or normals.
            bool changed=true;
            while(changed) {
                changed=false;
                for(std::size_t i=0;i<result.size() && !changed;++i)
                    for(std::size_t j=i+1;j<result.size();++j) {
                        auto& a=result[i]; auto b=result[j];
                        if(a.normal.dot(b.normal)<1.-1.e-7) continue;
                        if((a.b-b.b).norm()<=tolerance || (a.a-b.a).norm()<=tolerance)
                            std::swap(b.a,b.b);
                        if((a.b-b.a).norm()<=tolerance) a.b=b.b;
                        else if((b.b-a.a).norm()<=tolerance) a.a=b.a;
                        else continue;
                        result.erase(result.begin()+j);changed=true;break;
                    }
            }
            return result;
        }

        double distanceToSection(const Eigen::Vector3d& point,
            const std::vector<SectionSegment>& section)
        {
            double best=std::numeric_limits<double>::infinity();
            for(const auto& line:section) {
                const Eigen::Vector3d delta=line.b-line.a;
                const double u=std::clamp((point-line.a).dot(delta)/delta.squaredNorm(),0.,1.);
                best=std::min(best,(point-line.a-u*delta).norm());
            }
            return best;
        }

        bool obstructed(const Eigen::Vector3d& target,const Eigen::Vector3d& eye,
            const std::vector<RayTriangle>& mesh,double epsilon)
        {
            // Cache static triangle edges and use scalar Moller-Trumbore here.
            // Avoid millions of Eigen expression/debug-check temporaries without
            // changing endpoints, barycentric tolerances or intersection gates.
            const double dx=eye.x()-target.x(),dy=eye.y()-target.y(),dz=eye.z()-target.z();
            const double length=std::sqrt(dx*dx+dy*dy+dz*dz);
            for(const auto& tri:mesh) {
                const double hx=dy*tri.e2[2]-dz*tri.e2[1],hy=dz*tri.e2[0]-dx*tri.e2[2],hz=dx*tri.e2[1]-dy*tri.e2[0];
                const double determinant=tri.e1[0]*hx+tri.e1[1]*hy+tri.e1[2]*hz;
                if(std::abs(determinant)<1.e-15) continue;
                const double sx=target.x()-tri.a[0],sy=target.y()-tri.a[1],sz=target.z()-tri.a[2];
                const double u=(sx*hx+sy*hy+sz*hz)/determinant;
                if(u<-1.e-9 || u>1.+1.e-9) continue;
                const double qx=sy*tri.e1[2]-sz*tri.e1[1],qy=sz*tri.e1[0]-sx*tri.e1[2],qz=sx*tri.e1[1]-sy*tri.e1[0];
                const double v=(dx*qx+dy*qy+dz*qz)/determinant;
                if(v<-1.e-9 || u+v>1.+1.e-9) continue;
                const double t=(tri.e2[0]*qx+tri.e2[1]*qy+tri.e2[2]*qz)/determinant;
                if(t*length>epsilon && t<1.-1.e-8) return true;
            }
            return false;
        }
    }

    Eigen::Vector3d SingleSideScanPlanner::axis(int signedAxis)
    {
        Eigen::Vector3d result=Eigen::Vector3d::Zero();
        if(signedAxis>=0 && signedAxis<6) result[signedAxis/2]=(signedAxis%2)?-1.:1.;
        return result;
    }

    Result SingleSideScanPlanner::generate(const std::vector<Triangle>& mesh,const Parameters& p)
    {
        Result r;
        const auto fail=[&](const char* message) {r.success=false;r.message=message;r.points.clear();return r;};
        const Eigen::Vector3d scan=axis(p.scanAxis),side=axis(p.sideAxis);
        const double values[]={p.passSpacing,p.sampleSpacing,p.sprayDistance,p.spraySpeed,
            p.transferSpeed,p.endExtension,p.retractClearance,p.maximumTiltDegrees};
        if(mesh.empty() || scan.norm()<.5 || side.norm()<.5 || std::abs(scan.dot(side))>.01 ||
            std::any_of(std::begin(values),std::end(values),[](double x){return !std::isfinite(x);}) ||
            p.passSpacing<=0. || p.sampleSpacing<=0. || p.sprayDistance<=0. || p.spraySpeed<=0. ||
            p.transferSpeed<=0. || p.endExtension<0. || p.retractClearance<=0. ||
            p.maximumTiltDegrees<0. || p.maximumTiltDegrees>=90.)
            return fail("Invalid scan axes, mesh or finite positive parameters.");
        double low=std::numeric_limits<double>::infinity(),high=-low,outer=-low;
        Eigen::AlignedBox3d bounds;
        for(const auto& tri:mesh) for(const auto& v:tri) {
            if(!v.allFinite()) return fail("The mesh contains non-finite coordinates.");
            low=std::min(low,v.dot(scan));high=std::max(high,v.dot(scan));
            outer=std::max(outer,v.dot(side));bounds.extend(v);
        }
        const double scale=bounds.diagonal().norm();
        std::vector<RayTriangle> rayMesh;rayMesh.reserve(mesh.size());
        for(const auto& tri:mesh)rayMesh.emplace_back(tri);
        const double tolerance=std::max(1.e-9,scale*1.e-7);
        if(high-low<=tolerance) return fail("The model has no extent along the scan axis.");
        if((high-low)/p.sampleSpacing>maximumOutputPoints)
            return fail("Requested sampling exceeds the output limit; increase sample spacing.");
        const double station=.5*(low+high);
        r.section=cut(mesh,scan,station,tolerance);
        if(r.section.empty()) return fail("No transverse section was found.");
        // A swept section must represent the actual side faces along its full length.
        for(const auto& tri:mesh) {
            const Eigen::Vector3d n=(tri[1]-tri[0]).cross(tri[2]-tri[0]);
            if(n.norm()>tolerance*tolerance && std::abs(n.normalized().dot(scan))>1.e-6 &&
                std::abs(n.normalized().dot(scan))<1.-1.e-6)
                return fail("The model is not a constant section along the selected scan axis.");
        }
        for(double fraction:{.1,.3,.7,.9}) {
            const double z=low+(high-low)*fraction;
            const auto sample=cut(mesh,scan,z,tolerance);
            if(sample.empty()) return fail("The model has an incomplete swept section.");
            for(const auto& segment:r.section) for(const auto& v:{segment.a,segment.b})
                if(distanceToSection(v+(z-station)*scan,sample)>10*tolerance)
                    return fail("Cross sections vary along the selected scan axis.");
            for(const auto& segment:sample) for(const auto& v:{segment.a,segment.b})
                if(distanceToSection(v+(station-z)*scan,r.section)>10*tolerance)
                    return fail("Cross sections vary along the selected scan axis.");
        }
        std::vector<detail::ScanPass> passes;
        // The original spacing/order determines the pass budget. Raising the
        // aiming limit does not add supplementary root or thickness passes.
        const double minDot=std::cos(std::min(80.,p.maximumTiltDegrees)*pi/180.);
        for(const auto& segment:r.section) {
            if(segment.normal.dot(side)<minDot-1.e-8) continue;
            const double countValue=std::ceil((segment.b-segment.a).norm()/p.passSpacing);
            if(countValue>10000) return fail("Pass spacing requests too many scan paths.");
            const std::size_t count=std::max<std::size_t>(1,static_cast<std::size_t>(countValue));
            for(std::size_t k=0;k<count;++k) {
                const Eigen::Vector3d target=segment.a+((k+.5)/count)*(segment.b-segment.a);
                bool visible=true;
                for(double z:{low+10*tolerance,station,high-10*tolerance}) {
                    const Eigen::Vector3d s=target+(z-station)*scan;
                    // Visibility from the selected exterior side also excludes inner walls at short standoffs.
                    const Eigen::Vector3d outside=s+(outer-s.dot(side)+std::max(scale,p.sprayDistance))*side;
                    if(obstructed(s,outside,rayMesh,10*tolerance) ||
                        obstructed(s,s+p.sprayDistance*segment.normal,rayMesh,10*tolerance)) visible=false;
                }
                if(!visible){++r.hiddenCandidates;continue;}
                passes.push_back({target,segment.normal});
                if(passes.size()>10000) return fail("Too many scan paths; increase pass spacing.");
            }
        }
        if(passes.empty()) return fail("No exposed target satisfies the selected side and tilt limit.");
        const Eigen::Vector3d lateral=side.cross(scan).normalized();
        std::stable_sort(passes.begin(),passes.end(),[&](const auto& a,const auto& b){
            return a.target.dot(lateral)<b.target.dot(lateral);
        });
        if((high-low)/p.sampleSpacing*passes.size()>maximumOutputPoints)
            return fail("Requested sampling exceeds the output limit; increase sample spacing.");
        if(!detail::fitExteriorCoverage(r.section,scan,side,p,passes,r))return fail(r.message.c_str());
        constexpr double scanAcceleration=.150,valveDelay=.100;
        const double rampTime=p.spraySpeed/scanAcceleration;
        const double rampDistance=p.spraySpeed*rampTime*.5;
        const double extension=std::max(p.endExtension,.009+rampDistance+p.spraySpeed*valveDelay+p.retractClearance);
        const double z0=low-extension,z1=high+extension;
        r.scanLength=z1-z0;r.sprayDistance=p.sprayDistance;
        const double scanDuration=(r.scanLength-2*rampDistance)/p.spraySpeed+2*rampTime;
        const double samplesValue=std::max(std::ceil(r.scanLength/p.sampleSpacing),std::ceil(scanDuration/.04));
        if(samplesValue>maximumOutputPoints || (samplesValue+2)*passes.size()>maximumOutputPoints)
            return fail("Requested sampling exceeds the output limit; increase sample spacing.");
        const std::size_t samples=std::max<std::size_t>(1,static_cast<std::size_t>(samplesValue));
        const double safetyPlane=outer+p.sprayDistance+p.retractClearance;
        bool outputLimit=false;
        const auto append=[&](const Eigen::Isometry3d& pose,bool spray,int pass,double speed,double dt=0.) {
            if(r.points.size()>=maximumOutputPoints){outputLimit=true;return;}
            double time=0.;
            if(!r.points.empty()) {
                const auto& previous=r.points.back();
                const double angle=Eigen::AngleAxisd(previous.pose.linear().transpose()*pose.linear()).angle();
                time=previous.time+(dt>0.?dt:std::max({1.e-5,(pose.translation()-previous.pose.translation()).norm()/speed,
                    angle/(60.*pi/180.)}));
            }
            r.points.push_back({pose,time,speed,pass,spray});
        };
        const auto transfer=[&](const Eigen::Isometry3d& destination,int pass) {
            if(r.points.empty()){append(destination,false,pass,p.transferSpeed);return;}
            const Eigen::Isometry3d from=r.points.back().pose;
            const double angle=Eigen::AngleAxisd(from.linear().transpose()*destination.linear()).angle();
            const double length=(destination.translation()-from.translation()).norm();
            const double duration=std::max({.04,1.5*length/p.transferSpeed,1.5*angle/(60.*pi/180.),
                std::sqrt(6*std::max(length,.001)/.600)});
            const double nValue=std::max({std::ceil(length/.005),std::ceil(angle/(5.*pi/180.)),std::ceil(duration/.025)});
            const std::size_t n=std::max<std::size_t>(1,static_cast<std::size_t>(nValue));
            if(n>maximumOutputPoints || r.points.size()+n>maximumOutputPoints){outputLimit=true;return;}
            for(std::size_t k=1;k<=n;++k) {
                const double f=static_cast<double>(k)/n,u=f*f*(3-2*f);
                Eigen::Isometry3d pose=Eigen::Isometry3d::Identity();
                pose.translation()=(1-u)*from.translation()+u*destination.translation();
                pose.linear()=Eigen::Quaterniond(from.linear()).slerp(u,Eigen::Quaterniond(destination.linear())).toRotationMatrix();
                append(pose,false,pass,p.transferSpeed,duration/n);
            }
        };
        for(std::size_t i=0;i<passes.size();++i) {
            const auto& pass=passes[i];
            Eigen::Isometry3d start=Eigen::Isometry3d::Identity();
            start.linear().col(0)=scan; // Keep roll fixed on the return stroke.
            start.linear().col(2)=-pass.normal;
            start.linear().col(1)=start.linear().col(2).cross(scan).normalized();
            const bool reverse=p.alternateDirection && (i%2);
            const double begin=reverse?z1:z0,end=reverse?z0:z1;
            start.translation()=pass.target+(begin-station)*scan+p.sprayDistance*pass.normal;
            // Alternating strokes meet at the same end, entirely outside the
            // workpiece. A deliberately unidirectional scan uses an exterior
            // rail for the long return, with both turns still beyond the ends.
            if(!p.alternateDirection && !r.points.empty()) {
                Eigen::Isometry3d rail=r.points.back().pose;
                rail.translation()+=(safetyPlane-rail.translation().dot(side))*side;
                transfer(rail,static_cast<int>(i));
                rail.translation()+=(begin-rail.translation().dot(scan))*scan;
                transfer(rail,static_cast<int>(i));
            }
            transfer(start,static_cast<int>(i));
            append(start,false,static_cast<int>(i),p.spraySpeed,valveDelay);
            std::vector<double> grid;
            for(std::size_t k=1;k<=samples;++k)grid.push_back(scanDuration*k/samples);
            grid.push_back(rampTime);grid.push_back(scanDuration-rampTime);
            std::sort(grid.begin(),grid.end());
            grid.erase(std::unique(grid.begin(),grid.end(),[](double a,double b){return std::abs(a-b)<1.e-10;}),grid.end());
            double previous=0.;
            for(double tau:grid) {
                double distance;
                if(tau<=rampTime)distance=.5*scanAcceleration*tau*tau;
                else if(tau<=scanDuration-rampTime)distance=rampDistance+(tau-rampTime)*p.spraySpeed;
                else distance=r.scanLength-.5*scanAcceleration*(scanDuration-tau)*(scanDuration-tau);
                Eigen::Isometry3d pose=start;
                pose.translation()+=(end>begin?distance:-distance)*scan;
                append(pose,tau<=scanDuration-rampTime+1.e-12,static_cast<int>(i),p.spraySpeed,tau-previous);
                previous=tau;
            }
            if(outputLimit || r.points.size()>=maximumOutputPoints) return fail("Transfer sampling exceeds the output limit.");
        }
        // Independently reject a geometric tool-point segment entering the mesh.
        for(std::size_t i=1;i<r.points.size();++i)
            if(obstructed(r.points[i-1].pose.translation(),r.points[i].pose.translation(),rayMesh,tolerance))
                return fail("A transfer intersects the workpiece; increase end extension or change the selected side.");
        r.passCount=passes.size();r.success=true;
        r.message="Generated coverage-fitted exterior scans with spray-off end transfers; all finite-footprint exterior samples covered. Robot IK/full-tool collision checks remain downstream.";
        return r;
    }
}
