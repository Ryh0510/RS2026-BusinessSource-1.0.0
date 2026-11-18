#include <SlicingTrajPlan/BladeRegionPlanner.h>
#include "BladeFixture.h"
#include <SlicingTrajPlan/STLProcessor.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <iomanip>

using namespace blade_region;
using V=Eigen::Vector3d;
void require(bool b,const std::string& s){if(!b)throw std::runtime_error(s);}

int main(int argc,char** argv){try{
    Session s;Parameters p;p.sliceSpacing=.012;p.passSpacing=.006;p.sampleSpacing=.003;p.firstOffset=-.006;p.lastOffset=.006;p.sprayDistance=.080;
    auto mesh=makeBladeFixture();bool real=argc>1;
    if(real){ModelTrajPlan::ModelData data;ModelTrajPlan::STLProcessor loader;require(loader.preprocessSTL(argv[1],data),"real STL load");const auto& m=data.mainModelData_;
        mesh.clear();for(std::size_t i=0;i<m.m_indices.size();i+=3){section_sweep::Triangle tri;for(int k=0;k<3;++k){auto j=m.m_indices[i+k]*3;tri[k]=.001*V(m.m_vertices[j],m.m_vertices[j+1],m.m_vertices[j+2]);}mesh.push_back(tri);}
        p.sprayDistance=.030;p.sliceSpacing=.010;p.passSpacing=.008;p.sampleSpacing=.003;
    }
    if(argc>3){p.sprayDistance=std::stod(argv[3])*.001;p.sliceSpacing=.008;p.firstOffset=-.012;p.lastOffset=.012;}
    std::cout<<"spray distance "<<p.sprayDistance<<std::endl;
    auto stage=[&](const char* name,auto op){auto begin=std::chrono::steady_clock::now();auto r=op();std::cout<<name<<": "<<r.success<<" "<<r.message<<" "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"s"<<std::endl;require(r.success,r.message);};
    require(!s.fitReferences().success,"references without mesh must reject");std::atomic_bool cancelled{true};require(!s.identify(mesh,p,&cancelled).success,"cancel identification");
    stage("identify",[&](){return s.identify(mesh,p);});
    double sum=0;for(auto a:s.regionAreas()){std::cout<<"area "<<a<<std::endl;sum+=a;}require(sum>0,"partition area");require(s.labels().size()==mesh.size(),"preserve all source faces");
    if(argc>2){std::ofstream labels(std::string(argv[2])+".labels");for(auto r:s.labels())labels<<int(r)<<"\n";}
    stage("reference",[&](){return s.fitReferences();});require(s.bands().size()==2,"two reference bands");
    for(const auto& b:s.bands()){require(!b.bodyBoundary.empty()&&!b.platformBoundary.empty()&&!b.reference.empty(),"both boundaries and reference");
        std::cout<<"band "<<int(b.region)<<" boundaries "<<b.bodyBoundary.size()<<","<<b.platformBoundary.size()<<" refs "<<b.reference.size()<<std::endl;}
    for(int i=0;i<5;++i)stage(("region"+std::to_string(i)).c_str(),[&](){return s.generate(static_cast<Region>(i));});
    std::vector<double> rawNormalAngles;
    for(const auto& c:s.paths()){for(std::size_t i=1;i<c.normals.size();++i)rawNormalAngles.push_back(std::acos(std::clamp(c.normals[i-1].dot(c.normals[i]),-1.,1.)));require(int(c.region)<5,"no excluded paths");require(c.normals.size()==c.points.size(),"normal count");for(const auto& n:c.normals)require(n.allFinite()&&std::abs(n.norm()-1)<1e-6,"unit normals");}
    if(argc>2){std::ofstream curves(std::string(argv[2])+".curves.csv");curves<<std::setprecision(17)<<"kind,curve,region,x,y,z,nx,ny,nz\n";int id=0;
        auto write=[&](const Curve& c,int kind){for(std::size_t j=0;j<c.points.size();++j){const auto& x=c.points[j];const auto& n=c.normals[j];curves<<kind<<","<<id<<","<<int(c.region)<<","<<x.x()<<","<<x.y()<<","<<x.z()<<","<<n.x()<<","<<n.y()<<","<<n.z()<<"\n";}++id;};
        for(const auto& b:s.bands()){for(const auto& c:b.bodyBoundary)write(c,0);for(const auto& c:b.platformBoundary)write(c,1);for(const auto& c:b.reference)write(c,2);}for(const auto& c:s.paths())write(c,3);}
    stage("connect",[&](){return s.connect();});require(s.trajectoryRegions().size()==s.trajectory().points.size(),"region provenance");
    auto& pts=s.trajectory().points;std::size_t air=0;double maxStep=0,maxAngle=0,transferLength=0;
    std::sort(rawNormalAngles.begin(),rawNormalAngles.end());double p99=rawNormalAngles[std::min(rawNormalAngles.size()-1,std::size_t(rawNormalAngles.size()*.99))];
    std::cout<<"raw process-normal p99 degrees "<<p99*180/3.141592653589793<<std::endl;
    if(real&&argc>3)require(p99<.174532926,"80 mm raw normals must be fair before pose interpolation");
    for(std::size_t i=0;i<pts.size();++i){require(pts[i].pose.matrix().allFinite(),"finite pose");require(std::abs(pts[i].pose.linear().determinant()-1)<1e-6,"right handed frame");air+=!pts[i].spray;
        if(i){if(!pts[i].spray)transferLength+=(pts[i].pose.translation()-pts[i-1].pose.translation()).norm();require(pts[i].time>pts[i-1].time,"strict time");maxStep=std::max(maxStep,(pts[i].pose.translation()-pts[i-1].pose.translation()).norm());maxAngle=std::max(maxAngle,Eigen::AngleAxisd(pts[i-1].pose.linear().transpose()*pts[i].pose.linear()).angle());}}
    std::cout<<"transfer length "<<transferLength<<std::endl;
    if(real&&argc>3)require(transferLength<4.,"80 mm transfers must not contain excessive lift loops");
    require(maxStep<=p.sampleSpacing*1.001&&maxAngle<.088,"bounded pose and orientation sampling");
    std::cout<<"points "<<pts.size()<<" air "<<air<<" max step "<<maxStep<<" max angular step "<<maxAngle<<std::endl;
    if(argc>2){std::ofstream f(argv[2]);f<<std::setprecision(17)<<"x,y,z,nx,ny,nz,region,spray,time\n";for(std::size_t i=0;i<pts.size();++i){auto x=pts[i].pose.translation();V n=-pts[i].pose.linear().col(2);f<<x.x()<<","<<x.y()<<","<<x.z()<<","<<n.x()<<","<<n.y()<<","<<n.z()<<","<<s.trajectoryRegions()[i]<<","<<pts[i].spray<<","<<pts[i].time<<"\n";}
        std::ofstream labels(std::string(argv[2])+".labels");for(auto r:s.labels())labels<<int(r)<<"\n";}
    p.passSpacing*=1.1;s.setParameters(p);require(s.paths().empty()&&!s.trajectory().success&&s.referencesReady(),"process edits invalidate paths only");
    p.endFraction=.25;s.setParameters(p);require(!s.identified()&&!s.referencesReady(),"partition edits invalidate all");
    require(!s.identify({},p).success,"empty rejection");
    if(!real){auto leaning=makeBladeFixture();for(auto& t:leaning)for(auto& x:t)x.x()+=x.z()*.7;
        Session leaned;stage("leaning blade partition",[&](){return leaned.identify(leaning,p);});std::size_t bodyFaces=0,totalFaces=0;
        for(std::size_t i=0;i<leaning.size();++i){double z=(leaning[i][0].z()+leaning[i][1].z()+leaning[i][2].z())/3;
            if((z>.025&&z<.05)||(z>.25&&z<.275)){++totalFaces;bodyFaces+=leaned.labels()[i]==Region::Body;}}
        require(totalFaces>0&&bodyFaces>totalFaces*.98,"leaning end body must not become excluded or a blend");
        auto moved=makeBladeFixture();Eigen::AngleAxisd r(1.5707963267948966,V::UnitY());for(auto& t:moved)for(auto& x:t)x=r*x+V(.1,-.2,.3);p.spanAxis=3;
        stage("cardinal-axis transformed blade",[&](){return s.identify(moved,p);});require(s.regionAreas()[0]>0&&s.regionAreas()[3]>0,"auto cardinal axis and translation");}
    std::cout<<"PASS"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
