#include "UserCoordinateFrame.h"
#include "SceneEntitySemantics.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace robot_qt_viewer {
namespace {
QJsonObject frameState(const simulation_project::ProjectDocument& doc,const std::string& id) {
    for(const auto& ext:doc.extensions) if(ext.key=="rws.frame."+id)
        return QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object();
    return {};
}
}
bool coordinateFrameDeleted(const simulation_project::ProjectDocument& doc,const std::string& id) {
    return id!="world" && frameState(doc,id).value("deleted").toBool(false);
}
bool coordinateFrameVisible(const simulation_project::ProjectDocument& doc,const std::string& id) {
    return !coordinateFrameDeleted(doc,id) && frameState(doc,id).value("visible").toBool(true);
}
void setCoordinateFrameState(simulation_project::ProjectDocument& doc,const std::string& id,bool visible,bool deleted) {
    simulation_project::ProjectExtensionDesc ext;
    ext.key="rws.frame."+id; ext.version=1;
    ext.serializedPayload=QJsonDocument(QJsonObject{{"visible",visible},{"deleted",deleted && id!="world"}}).toJson(QJsonDocument::Compact).toStdString();
    auto it=std::find_if(doc.extensions.begin(),doc.extensions.end(),[&](const auto& e){return e.key==ext.key;});
    if(it==doc.extensions.end()) doc.extensions.push_back(ext); else *it=ext;
}
Eigen::Isometry3d coordinateTransform(const simulation_project::TransformDesc& p)
{
    Eigen::Isometry3d t = Eigen::Isometry3d::Identity();
    t.translation() = Eigen::Vector3d(p.x,p.y,p.z);
    t.linear() = (Eigen::AngleAxisd(p.yaw,Eigen::Vector3d::UnitZ()) *
        Eigen::AngleAxisd(p.pitch,Eigen::Vector3d::UnitY()) *
        Eigen::AngleAxisd(p.roll,Eigen::Vector3d::UnitX())).toRotationMatrix();
    if(!t.matrix().allFinite()) throw std::runtime_error("User frame pose must be finite");
    return t;
}
simulation_project::TransformDesc coordinatePose(const Eigen::Isometry3d& t)
{
    simulation_project::TransformDesc p;
    p.x=t.translation().x(); p.y=t.translation().y(); p.z=t.translation().z();
    const auto& r=t.linear();
    p.pitch=std::atan2(-r(2,0),std::hypot(r(0,0),r(1,0)));
    if(std::hypot(r(0,0),r(1,0))>1e-10) {
        p.roll=std::atan2(r(2,1),r(2,2)); p.yaw=std::atan2(r(1,0),r(0,0));
    } else { p.roll=0; p.yaw=std::atan2(-r(0,1),r(1,1)); }
    return p;
}
simulation_project::TransformDesc userCoordinateFrame(const simulation_project::ProjectDocument& doc)
{
    for(const auto& ext:doc.extensions) if(ext.key=="rws.user_frame") {
        const auto json=QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload));
        if(ext.version!=1 || !json.isObject()) throw std::runtime_error("Invalid user frame extension");
        const auto o=json.object();
        simulation_project::TransformDesc p;
        double* fields[]={&p.x,&p.y,&p.z,&p.roll,&p.pitch,&p.yaw};
        const char* names[]={"x","y","z","roll","pitch","yaw"};
        for(int i=0;i<6;++i) {
            if(!o.value(names[i]).isDouble() || !std::isfinite(o.value(names[i]).toDouble()))
                throw std::runtime_error("Invalid user frame pose");
            *fields[i]=o.value(names[i]).toDouble();
        }
        return p;
    }
    return {};
}
bool belongsToUserFrame(const simulation_project::ProjectDocument& doc,const simulation_project::SceneObjectDesc& o)
{
    return !objectAttachment(doc,o.id) && sceneObjectRole(doc,o)=="workpiece";
}
void changeUserCoordinateFrame(simulation_project::ProjectDocument& doc,const simulation_project::TransformDesc& pose)
{
    const Eigen::Isometry3d delta=coordinateTransform(pose)*coordinateTransform(userCoordinateFrame(doc)).inverse();
    auto candidate=doc;
    if(coordinateFrameDeleted(candidate,"user")) setCoordinateFrameState(candidate,"user",true,false);
    for(auto& object:candidate.objects) if(belongsToUserFrame(doc,object))
        object.transform=coordinatePose(delta*coordinateTransform(object.transform));
    const QJsonObject json{{"x",pose.x},{"y",pose.y},{"z",pose.z},
        {"roll",pose.roll},{"pitch",pose.pitch},{"yaw",pose.yaw}};
    simulation_project::ProjectExtensionDesc ext;
    ext.key="rws.user_frame"; ext.version=1;
    ext.serializedPayload=QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
    auto it=std::find_if(candidate.extensions.begin(),candidate.extensions.end(),
        [](const auto& e){return e.key=="rws.user_frame";});
    if(it==candidate.extensions.end()) candidate.extensions.push_back(ext); else *it=ext;
    doc=std::move(candidate);
}
}
