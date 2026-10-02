#pragma once

#include "ProjectScene.h"

#include <CameraCore/ICameraController.h>
#include <Collision/CollisionObject.h>
#include <Collision/CollisionTypes.h>
#include <SceneCore/CameraNode.h>

#include <Eigen/Core>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rendercore
{
    class OffscreenRenderTarget;
}

namespace scenecore
{
    class Renderer;
    class SceneGraph;
}

namespace simulation_runtime
{
    struct RuntimeMountedCamera;
}

class ProjectSceneCameraSystem
{
public:
    struct Bounds
    {
        bool valid = false;
        collision::Vec3 min = collision::Vec3::Zero();
        collision::Vec3 max = collision::Vec3::Zero();

        void includePoint(const collision::Vec3& point);
        void includeAabb(const collision::Aabb& aabb);
        void includeTransformedBounds(
            const collision::Transform3& transform,
            const collision::Vec3& localMin,
            const collision::Vec3& localMax);
        collision::Vec3 center() const;
        double radius() const;
    };

    struct LookAtState
    {
        Eigen::Vector3f position = Eigen::Vector3f(0.0f, -5.0f, 3.0f);
        Eigen::Vector3f target = Eigen::Vector3f::Zero();
        Eigen::Vector3f up = Eigen::Vector3f(0.0f, 0.0f, 1.0f);
        bool valid = false;
    };

    ProjectSceneCameraSystem();
    ~ProjectSceneCameraSystem();

    bool initialize(scenecore::SceneGraph& graph);
    std::shared_ptr<scenecore::CameraNode> mainCameraNode() const;

    void onMouseMove(float dx, float dy, int button);
    void onScroll(float delta);
    void setView(ProjectSceneCameraView view, const Bounds& bounds, bool animate);
    void animateTo(const LookAtState& state, double duration);
    void updateAnimation(double timeSeconds);

    bool focusMountFrameLink(
        const std::string& robotId,
        const std::string& linkName,
        const Bounds& bounds);
    bool clearMountFrameLinkFocus();
    bool focusObjectFrameObject(const std::string& objectId, const Bounds& bounds);
    bool clearObjectFrameObjectFocus();
    bool focusMountedAttachment(const std::string& attachmentId, const Bounds& bounds);
    bool clearMountedAttachmentFocus();

    bool mountFrameLinkFocusActive() const;
    const std::string& mountFrameFocusRobotId() const;
    const std::string& mountFrameFocusLinkName() const;
    bool objectFrameObjectFocusActive() const;
    const std::string& objectFrameFocusObjectId() const;
    bool mountedAttachmentFocusActive() const;
    const std::string& mountedAttachmentFocusId() const;

    bool renderMountedCamera(
        const simulation_runtime::RuntimeMountedCamera& camera,
        int width,
        int height,
        const Eigen::Vector4f& backgroundColor,
        scenecore::SceneGraph& graph,
        scenecore::Renderer& renderer,
        std::vector<unsigned char>& rgbaPixels,
        std::string* errorMessage);

    static LookAtState makeLookAtState(
        const Bounds& bounds,
        ProjectSceneCameraView view,
        double distanceScale);
    static LookAtState interpolateLookAt(
        const LookAtState& start,
        const LookAtState& end,
        double t);

private:
    struct RenderState;

    void applyLookAt(const LookAtState& state);

    cameracore::CameraControllerPtr m_controller;
    std::shared_ptr<scenecore::CameraNode> m_mainCameraNode;
    std::unordered_map<std::string, std::unique_ptr<RenderState>> m_renderStates;
    LookAtState m_lookAt;
    bool m_animationActive = false;
    LookAtState m_animationStart;
    LookAtState m_animationEnd;
    double m_animationElapsed = 0.0;
    double m_animationDuration = 0.45;
    double m_lastAnimationUpdateTime = -1.0;
    bool m_mountFrameLinkFocusActive = false;
    std::string m_mountFrameFocusRobotId;
    std::string m_mountFrameFocusLinkName;
    bool m_objectFrameObjectFocusActive = false;
    std::string m_objectFrameFocusObjectId;
    bool m_mountedAttachmentFocusActive = false;
    std::string m_mountedAttachmentFocusId;
};
