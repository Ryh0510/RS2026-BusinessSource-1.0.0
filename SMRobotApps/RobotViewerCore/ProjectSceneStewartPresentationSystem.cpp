#include "ProjectSceneStewartPresentationSystem.h"

#include <SimulationProject/ProjectDocument.h>
#include <Utility/MathConvert.hpp>

#include <Eigen/Geometry>

namespace
{
    Eigen::Matrix4d makeLegFollowerAffine(
        const collision::Transform3& platformHomeTransform,
        const kine::StewartPlatformPose& pose,
        const collision::Vec3& homeStart,
        const collision::Vec3& homeEnd)
    {
        const Eigen::Vector3d currentStart = homeStart;
        const Eigen::Vector3d currentEnd =
            kine::StewartPlatformKinematics::poseTransform(pose) * homeEnd;
        const Eigen::Vector3d homeVector = homeEnd - homeStart;
        const Eigen::Vector3d currentVector = currentEnd - currentStart;
        const double homeLength = homeVector.norm();
        const double currentLength = currentVector.norm();
        if(homeLength < 1.0e-9 || currentLength < 1.0e-9) {
            return Eigen::Matrix4d::Identity();
        }

        const Eigen::Vector3d homeAxis = homeVector / homeLength;
        const Eigen::Quaterniond rotation =
            Eigen::Quaterniond::FromTwoVectors(homeVector, currentVector).normalized();
        const Eigen::Matrix3d stretch =
            Eigen::Matrix3d::Identity() +
            ((currentLength / homeLength) - 1.0) *
                (homeAxis * homeAxis.transpose());
        const Eigen::Matrix3d linear = rotation.toRotationMatrix() * stretch;

        Eigen::Matrix4d local = Eigen::Matrix4d::Identity();
        local.block<3, 3>(0, 0) = linear;
        local.block<3, 1>(0, 3) = currentStart - linear * homeStart;
        return platformHomeTransform.matrix() * local *
            platformHomeTransform.inverse().matrix();
    }

    void applyInternalVisualOverride(RuntimeRobot& platform)
    {
        if(!platform.parallelControlEnabled ||
            !platform.parallelInternalPlatformVisualsEnabled ||
            !platform.visualBridge) {
            return;
        }

        const collision::Transform3 poseTransform =
            kine::StewartPlatformKinematics::poseTransform(platform.parallelPose);
        for(const std::string& linkName : platform.parallelInternalPlatformDrivenLinks) {
            const auto homeIt =
                platform.parallelInternalPlatformHomeLocalTransforms.find(linkName);
            if(homeIt == platform.parallelInternalPlatformHomeLocalTransforms.end()) {
                continue;
            }
            const auto node = platform.visualBridge->linkNode(linkName);
            if(!node) {
                continue;
            }

            const collision::Transform3 linkTransform =
                platform.parallelHomeBaseTransform * poseTransform * homeIt->second;
            node->setLocal(math::eigenToGlm(linkTransform.matrix()));
            if(platform.collisionInstance) {
                platform.collisionInstance->setLinkTransform(linkName, linkTransform);
            }
        }
    }

    void applyFollowerVisualOverride(
        const RuntimeRobot& platform,
        RuntimeRobot& follower)
    {
        if(!platform.parallelControlEnabled ||
            !follower.parallelFollowerEnabled ||
            !follower.parallelFollowerAnchorsValid ||
            !follower.visualBridge ||
            follower.parallelFollowerDrivenLinks.empty()) {
            return;
        }

        const Eigen::Matrix4d followerAffine = makeLegFollowerAffine(
            platform.parallelHomeBaseTransform,
            platform.parallelPose,
            follower.parallelFollowerHomeBaseAnchor,
            follower.parallelFollowerHomePlatformAnchor);
        for(const std::string& linkName : follower.parallelFollowerDrivenLinks) {
            const auto homeIt = follower.parallelFollowerHomeLinkTransforms.find(linkName);
            if(homeIt == follower.parallelFollowerHomeLinkTransforms.end()) {
                continue;
            }
            const auto node = follower.visualBridge->linkNode(linkName);
            if(!node) {
                continue;
            }
            const Eigen::Matrix4d linkTransform =
                followerAffine * homeIt->second.matrix();
            node->setLocal(math::eigenToGlm(linkTransform));
        }
    }
}

void ProjectSceneStewartPresentationSystem::configureParallelControlIfNeeded(
    RuntimeRobot& runtime,
    const simulation_project::RobotDesc& robotDesc,
    const robot::RobotModel& model)
{
    simulation_runtime::ProjectParallelMechanismRuntime::configureParallelControlIfNeeded(
        *runtime.parallelState,
        robotDesc,
        model);
}

void ProjectSceneStewartPresentationSystem::configureParallelFollowerIfNeeded(
    RuntimeRobot& runtime,
    const simulation_project::RobotDesc& robotDesc,
    const robot::RobotModel& model)
{
    simulation_runtime::ProjectParallelMechanismRuntime::configureParallelFollowerIfNeeded(
        *runtime.parallelState,
        robotDesc,
        model);
}

void ProjectSceneStewartPresentationSystem::solveInternalLegControls(RuntimeRobot& platform)
{
    simulation_runtime::ProjectParallelMechanismRuntime::solveInternalLegControls(
        *platform.parallelState);
}

bool ProjectSceneStewartPresentationSystem::sameSource(
    const RuntimeRobot& a,
    const RuntimeRobot& b)
{
    return simulation_runtime::ProjectParallelMechanismRuntime::sameSource(
        *a.parallelState,
        *b.parallelState);
}

void ProjectSceneStewartPresentationSystem::applyInternalVisualOverrides(
    RuntimeRobot& platform)
{
    applyInternalVisualOverride(platform);
}

void ProjectSceneStewartPresentationSystem::applyFollowerVisualOverrides(
    const RuntimeRobot& platform,
    std::vector<RuntimeRobot>& robots)
{
    if(!platform.parallelControlEnabled) {
        return;
    }
    for(RuntimeRobot& follower : robots) {
        if(follower.documentId != platform.documentId &&
            follower.parallelFollowerEnabled &&
            sameSource(platform, follower)) {
            applyFollowerVisualOverride(platform, follower);
        }
    }
}

void ProjectSceneStewartPresentationSystem::applyAllVisualOverrides(
    std::vector<RuntimeRobot>& robots)
{
    for(RuntimeRobot& platform : robots) {
        applyInternalVisualOverrides(platform);
        applyFollowerVisualOverrides(platform, robots);
    }
}
