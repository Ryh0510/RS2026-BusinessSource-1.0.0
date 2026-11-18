#pragma once
#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>
#include <cmath>
#include <utility>
using V=Eigen::Vector3d;
inline std::vector<section_sweep::Triangle> makeBladeFixture(){
    constexpr double pi=3.141592653589793;
    std::vector<std::pair<double,double>> stations{{-.004,1.7},{0,1.7},{0,1.35}};
    for(int i=1;i<=24;++i){double a=pi*.5*i/24;stations.push_back({.012*(1-std::cos(a)),1+.35*(1-std::sin(a))});}
    for(int i=1;i<=36;++i)stations.push_back({.012+(.276*i/36),1.});
    for(int i=23;i>=0;--i){double a=pi*.5*i/24;stations.push_back({.300-.012*(1-std::cos(a)),1+.35*(1-std::sin(a))});}
    stations.push_back({.300,1.7});stations.push_back({.304,1.7});
    const int count=100;std::vector<std::vector<V>> rings;
    for(auto [z,r]:stations){std::vector<V> ring;for(int j=0;j<count;++j){double a=2*pi*j/count;double x=.04*r*std::cos(a),y=.023*r*std::sin(a);double twist=.22*z/.3;
            ring.push_back(V(x*std::cos(twist)-y*std::sin(twist)+z*.13,x*std::sin(twist)+y*std::cos(twist),z));}rings.push_back(std::move(ring));}
    std::vector<section_sweep::Triangle> mesh;for(std::size_t i=1;i<rings.size();++i)for(int j=0;j<count;++j){int k=(j+1)%count;
        mesh.push_back({rings[i-1][j],rings[i-1][k],rings[i][k]});mesh.push_back({rings[i-1][j],rings[i][k],rings[i][j]});}
    for(int j=0;j<count;++j){int k=(j+1)%count;mesh.push_back({V(0,0,-.004),rings.front()[k],rings.front()[j]});mesh.push_back({V(.304*.13,0,.304),rings.back()[j],rings.back()[k]});}
    return mesh;
}
