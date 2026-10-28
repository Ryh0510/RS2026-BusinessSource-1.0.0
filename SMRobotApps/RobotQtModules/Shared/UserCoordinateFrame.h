#pragma once
#include <SimulationProject/ProjectDocument.h>
#include <Eigen/Geometry>

namespace robot_qt_viewer {
bool coordinateFrameDeleted(const simulation_project::ProjectDocument& document, const std::string& id);
bool coordinateFrameVisible(const simulation_project::ProjectDocument& document, const std::string& id);
void setCoordinateFrameState(simulation_project::ProjectDocument& document, const std::string& id, bool visible, bool deleted);
// Project objects remain materialized in world coordinates for existing render,
// collision and planning consumers. Their local pose is inverse(U) * world.
Eigen::Isometry3d coordinateTransform(const simulation_project::TransformDesc& pose);
simulation_project::TransformDesc coordinatePose(const Eigen::Isometry3d& pose);
simulation_project::TransformDesc userCoordinateFrame(const simulation_project::ProjectDocument& document);
bool belongsToUserFrame(const simulation_project::ProjectDocument& document,
    const simulation_project::SceneObjectDesc& object);
void changeUserCoordinateFrame(simulation_project::ProjectDocument& document,
    const simulation_project::TransformDesc& pose);
}
