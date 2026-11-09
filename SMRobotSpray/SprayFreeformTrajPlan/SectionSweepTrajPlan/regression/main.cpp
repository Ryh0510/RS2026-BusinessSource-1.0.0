#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>
#include <SlicingTrajPlan/STLProcessor.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv)
{
 try {
    ModelTrajPlan::ModelData model;ModelTrajPlan::STLProcessor reader;
    require(reader.preprocessSTL(argc>1?argv[1]:SECTION_SWEEP_TEST_STL,model),"STL load failed");
    const auto& source=model.mainModelData_;std::vector<section_sweep::Triangle> mesh;
    for(std::size_t i=0;i<source.m_indices.size();i+=3) {
        section_sweep::Triangle tri;
        for(int k=0;k<3;++k){const auto j=3*source.m_indices[i+k];tri[k]=Eigen::Vector3d(source.m_vertices[j],source.m_vertices[j+1],source.m_vertices[j+2]);}
        mesh.push_back(tri);
    }
    section_sweep::Parameters parameters;
    const auto result=section_sweep::SingleSideScanPlanner::generate(mesh,parameters);
    std::cout<<result.message<<"\npasses="<<result.passCount<<" points="<<result.points.size()<<" hidden="<<result.hiddenCandidates<<std::endl;
    require(result.success && result.passCount>10 && result.points.size()>100,"Real plate/tube scan failed");
    require(result.passCount==36,"Added supplementary passes instead of fitting the original pass budget");
    require(result.exteriorSamples>5000 && result.coveredSamples==result.exteriorSamples,"Exterior/root coverage not validated");
    require(result.maximumUsedTiltDegrees>87.9 && result.maximumUsedTiltDegrees<=88.000001,"New root tilt fitting missing");
    const double extension=.009+parameters.spraySpeed*parameters.spraySpeed/(2*.150)+parameters.spraySpeed*.100+parameters.retractClearance;
    require(std::abs(result.scanLength-(.2+2*extension))<1.e-7,"Units or automatic end extension incorrect");
    bool tube=false,left=false,right=false,air=false;double last=-1;
    std::vector<double> first(result.passCount,std::numeric_limits<double>::quiet_NaN()),end=first;
    for(std::size_t i=0;i<result.points.size();++i) {
        const auto& p=result.points[i];require(p.pose.matrix().allFinite() && p.time>last,"Invalid times/pose");last=p.time;
        require(std::abs(p.pose.linear().determinant()-1)<1.e-10,"Invalid rotation");
        require(p.pose.linear().col(0).isApprox(Eigen::Vector3d::UnitZ(),1.e-10),"Return stroke flips tool roll");
        air|=!p.spray;
        if(i>0) {
            const auto& previous=result.points[i-1];
            const double speed=(p.pose.translation()-previous.pose.translation()).norm()/(p.time-previous.time);
            require(speed<=parameters.transferSpeed+1.e-8,"Transfer exceeds commanded speed limit");
            if(!p.spray && (p.pose.translation()-previous.pose.translation()).norm()>1.e-9) {
                require(std::max(p.pose.translation().z(),previous.pose.translation().z())<0. ||
                    std::min(p.pose.translation().z(),previous.pose.translation().z())>.2,
                    "Spray-off move crosses the active workpiece height band");
            }
        }
        if(!p.spray)continue;
        const Eigen::Vector3d target=p.pose.translation()+parameters.sprayDistance*p.pose.linear().col(2);
        require(target.y()>-1.e-7,"Backside selected");
        require(-p.pose.linear().col(2).y()>=std::cos(parameters.maximumTiltDegrees*3.141592653589793/180.)-1.e-7,"Tilt limit exceeded");
        if(std::abs(target.x())>.011) {require(std::abs(target.y())<1.e-7,"Plate target off surface");left|=target.x()<0;right|=target.x()>0;}
        else if(target.y()>.011) {
            const double radius=std::hypot(target.x(),target.y()-.01);
            require(radius>.0098 && radius<.0101,"Tube inner wall selected");tube=true;
        }
        if(std::isnan(first[p.pass])) first[p.pass]=result.points[i-1].pose.translation().z();
        end[p.pass]=p.pose.translation().z();
        const double actualSpeed=(p.pose.translation()-result.points[i-1].pose.translation()).norm()/(p.time-result.points[i-1].time);
        require(actualSpeed<=parameters.spraySpeed+1.e-8 && actualSpeed>0.,"Spray ramp/cruise speed mismatch");
        if(target.z()>.001 && target.z()<.199)require(std::abs(actualSpeed-parameters.spraySpeed)<1.e-8,"Speed changes on active workpiece");
    }
    require(left&&right&&tube&&air,"Missing plate/tube/air paths");
    for(std::size_t i=0;i<first.size();++i)require((end[i]-first[i])*(i%2?-1.:1.)>0,"Scan order not alternating");
    auto invalid=parameters;invalid.sideAxis=invalid.scanAxis;
    require(!section_sweep::SingleSideScanPlanner::generate(mesh,invalid).success,"Parallel scan/side axes accepted");
    invalid=parameters;invalid.passSpacing=0.;require(!section_sweep::SingleSideScanPlanner::generate(mesh,invalid).success,"Zero spacing accepted");
    invalid=parameters;invalid.sampleSpacing=1.e-12;require(!section_sweep::SingleSideScanPlanner::generate(mesh,invalid).success,"Unbounded allocation accepted");
    invalid=parameters;invalid.sideAxis=3;
    const auto back=section_sweep::SingleSideScanPlanner::generate(mesh,invalid);require(back.success,"Opposite side scan failed");
    for(const auto& p:back.points)if(p.spray)require(p.pose.linear().col(2).y()>.99,"Opposite side includes front tube");
    invalid=parameters;invalid.sprayDistance=.002;
    const auto close=section_sweep::SingleSideScanPlanner::generate(mesh,invalid);
    require(!close.success && close.points.empty(),"Impossible short-standoff coverage silently exported an incomplete path");
    invalid=parameters;invalid.passSpacing=1.;invalid.maximumTiltDegrees=0.;
    const auto sparse=section_sweep::SingleSideScanPlanner::generate(mesh,invalid);
    std::cout<<"Sparse: "<<sparse.message<<" success="<<sparse.success<<" samples="<<sparse.coveredSamples<<"/"<<sparse.exteriorSamples<<std::endl;
    require(!sparse.success && sparse.points.empty(),
        "Insufficient tilt/pass budget silently accepted incomplete finite-footprint coverage");
    Eigen::Matrix3d permutation;
    permutation<<0,0,1,1,0,0,0,1,0;
    const Eigen::Vector3d offset(.2,-.1,.3);
    auto moved=mesh;for(auto& tri:moved)for(auto& v:tri)v=permutation*v+offset;
    invalid=parameters;invalid.scanAxis=0;invalid.sideAxis=4;
    const auto rotated=section_sweep::SingleSideScanPlanner::generate(moved,invalid);
    require(rotated.success && rotated.passCount==result.passCount && rotated.coveredSamples==rotated.exteriorSamples,
        "Coverage fitter assumes world Z/Y axes or an origin-centered model");
    require((rotated.points.front().pose.translation()-(permutation*result.points.front().pose.translation()+offset)).norm()<1.e-7,
        "Transformed geometry/units are inconsistent");
    invalid=parameters;invalid.alternateDirection=false;
    const auto oneWay=section_sweep::SingleSideScanPlanner::generate(mesh,invalid);
    require(oneWay.success && oneWay.passCount==36,"Unidirectional scan exterior return failed");
    for(std::size_t i=1;i<oneWay.points.size();++i)if(!oneWay.points[i].spray) {
        const auto& a=oneWay.points[i-1].pose.translation();const auto& b=oneWay.points[i].pose.translation();
        if(std::max(a.z(),b.z())>=0. && std::min(a.z(),b.z())<=.2)
            require(a.y()>.12 && b.y()>.12,"Unidirectional return crosses the workpiece instead of the exterior rail");
    }
    auto tapered=mesh;for(auto& tri:tapered)for(auto& p:tri)p.x()+=.05*p.z();
    require(!section_sweep::SingleSideScanPlanner::generate(tapered,parameters).success,"Non-extruded model silently accepted");
    std::cout<<"Geometry, visibility, standoff, units, non-spray transfers, timestamps, speed, roll, opposite side and rejection gates passed.\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
