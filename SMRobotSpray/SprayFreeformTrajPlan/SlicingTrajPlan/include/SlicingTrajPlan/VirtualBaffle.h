// SMROBOT_PARENT_BAFFLE_COMPAT
#pragma once
#include <SlicingTrajPlan/BoundaryDetector.h>
#include <array>
#include <string>
#include <vector>

namespace ModelTrajPlan {
struct GeneralData_SHAREDLIB VirtualBaffle {
    using Triangle = std::array<Eigen::Vector3d, 3>;
    bool enabled = false;
    std::string diagnostic;
    BoundaryTopologyDiagnostics topology;
    Eigen::Vector3d center = Eigen::Vector3d::Zero();
    Eigen::Vector3d normal = Eigen::Vector3d::UnitZ();
    Eigen::Vector3d axisX = Eigen::Vector3d::UnitX();
    Eigen::Vector3d axisY = Eigen::Vector3d::UnitY();
    double thickness = .001, rmsResidual = 0, maxResidual = 0, correspondenceRms = 0;
    std::vector<std::vector<Eigen::Vector2d>> contours;
    bool apertureContains(const Eigen::Vector2d& point) const;
    bool intersectsTriangle(const Triangle& triangle, double clearance = 0) const;
    std::vector<Triangle> displayTriangles(double extent) const;
};
GeneralData_SHAREDLIB VirtualBaffle buildVirtualBaffle(
    const ModelData& model, double thickness = .001, double fitTolerance = .005);
}
