#include "ProjectSceneCameraSystem.h"

#include <CameraCore/CameraFactory.h>
#include <RenderCore/OffscreenRenderTarget.h>
#include <SceneCore/Renderer.h>
#include <SceneCore/SceneGraph.h>
#include <SimulationRuntime/RuntimeMountedAttachment.h>

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    Eigen::Vector3f cameraDirection(ProjectSceneCameraView view)
    {
        switch(view) {
        case ProjectSceneCameraView::Front:
            return Eigen::Vector3f(0.0f, -1.0f, 0.0f);
        case ProjectSceneCameraView::Back:
            return Eigen::Vector3f(0.0f, 1.0f, 0.0f);
        case ProjectSceneCameraView::Left:
            return Eigen::Vector3f(-1.0f, 0.0f, 0.0f);
        case ProjectSceneCameraView::Right:
            return Eigen::Vector3f(1.0f, 0.0f, 0.0f);
        case ProjectSceneCameraView::Top:
            return Eigen::Vector3f(0.0f, 0.0f, 1.0f);
        case ProjectSceneCameraView::Bottom:
            return Eigen::Vector3f(0.0f, 0.0f, -1.0f);
        case ProjectSceneCameraView::Home:
        case ProjectSceneCameraView::Isometric:
        default:
            return Eigen::Vector3f(1.0f, -1.0f, 0.75f).normalized();
        }
    }

    Eigen::Vector3f cameraUp(ProjectSceneCameraView view)
    {
        switch(view) {
        case ProjectSceneCameraView::Top:
            return Eigen::Vector3f(0.0f, 1.0f, 0.0f);
        case ProjectSceneCameraView::Bottom:
            return Eigen::Vector3f(0.0f, -1.0f, 0.0f);
        default:
            return Eigen::Vector3f(0.0f, 0.0f, 1.0f);
        }
    }
}

struct ProjectSceneCameraSystem::RenderState
{
    cameracore::PerspectiveCameraPtr camera;
    rendercore::OffscreenRenderTarget target;
};

ProjectSceneCameraSystem::ProjectSceneCameraSystem() = default;
ProjectSceneCameraSystem::~ProjectSceneCameraSystem() = default;

void ProjectSceneCameraSystem::Bounds::includePoint(const collision::Vec3& point)
{
    if(!valid) {
        min = point;
        max = point;
        valid = true;
        return;
    }
    min = min.cwiseMin(point);
    max = max.cwiseMax(point);
}

void ProjectSceneCameraSystem::Bounds::includeAabb(const collision::Aabb& aabb)
{
    includePoint(aabb.min);
    includePoint(aabb.max);
}

void ProjectSceneCameraSystem::Bounds::includeTransformedBounds(
    const collision::Transform3& transform,
    const collision::Vec3& localMin,
    const collision::Vec3& localMax)
{
    for(int x = 0; x < 2; ++x) {
        for(int y = 0; y < 2; ++y) {
            for(int z = 0; z < 2; ++z) {
                const collision::Vec3 local(
                    x == 0 ? localMin.x() : localMax.x(),
                    y == 0 ? localMin.y() : localMax.y(),
                    z == 0 ? localMin.z() : localMax.z());
                includePoint(transform * local);
            }
        }
    }
}

collision::Vec3 ProjectSceneCameraSystem::Bounds::center() const
{
    if(!valid) {
        return collision::Vec3::Zero();
    }
    return (min + max) * 0.5;
}

double ProjectSceneCameraSystem::Bounds::radius() const
{
    return valid ? std::max(0.5, (max - min).norm() * 0.5) : 2.0;
}

bool ProjectSceneCameraSystem::initialize(scenecore::SceneGraph& graph)
{
    auto camera = cameracore::CameraFactory::createOrbitCamera();
    if(!camera) {
        return false;
    }
    camera->setUpAxis(cameracore::UpAxis::Z_UP);
    m_mainCameraNode = std::make_shared<scenecore::CameraNode>(camera, "mainCameraNode");
    graph.addNode(m_mainCameraNode);
    graph.registerNode(m_mainCameraNode);
    m_controller = cameracore::CameraFactory::createOrbitController(m_mainCameraNode->camera());
    return m_controller != nullptr;
}

std::shared_ptr<scenecore::CameraNode> ProjectSceneCameraSystem::mainCameraNode() const
{
    return m_mainCameraNode;
}

void ProjectSceneCameraSystem::onMouseMove(float dx, float dy, int button)
{
    if(m_controller) {
        const Eigen::Vector3f previousPosition =
            m_mainCameraNode && m_mainCameraNode->camera()
                ? m_mainCameraNode->camera()->position()
                : Eigen::Vector3f::Zero();
        m_animationActive = false;
        m_controller->onMouseMove(dx, dy, button);
        if(m_mainCameraNode && m_mainCameraNode->camera() && m_lookAt.valid) {
            const Eigen::Vector3f currentPosition = m_mainCameraNode->camera()->position();
            if(button == 1) {
                m_lookAt.target += currentPosition - previousPosition;
            }
            m_lookAt.position = currentPosition;
        }
    }
}

void ProjectSceneCameraSystem::onScroll(float delta)
{
    if(m_controller) {
        m_animationActive = false;
        m_controller->onScroll(delta);
        if(m_mainCameraNode && m_mainCameraNode->camera() && m_lookAt.valid) {
            m_lookAt.position = m_mainCameraNode->camera()->position();
        }
    }
}

ProjectSceneCameraSystem::LookAtState ProjectSceneCameraSystem::makeLookAtState(
    const Bounds& bounds,
    ProjectSceneCameraView view,
    double distanceScale)
{
    const collision::Vec3 center = bounds.center();
    constexpr double kVerticalFovDegrees = 45.0;
    const double fovRadians = kVerticalFovDegrees * kPi / 180.0;
    const double distance = std::max(
        2.0,
        bounds.radius() / std::sin(fovRadians * 0.5) * distanceScale);

    LookAtState state;
    state.target = center.cast<float>();
    state.position = state.target + cameraDirection(view) * static_cast<float>(distance);
    state.up = cameraUp(view);
    state.valid = true;
    return state;
}

ProjectSceneCameraSystem::LookAtState ProjectSceneCameraSystem::interpolateLookAt(
    const LookAtState& start,
    const LookAtState& end,
    double t)
{
    const float weight = static_cast<float>(std::clamp(t, 0.0, 1.0));
    LookAtState state;
    state.position = start.position + (end.position - start.position) * weight;
    state.target = start.target + (end.target - start.target) * weight;
    state.up = (start.up + (end.up - start.up) * weight).normalized();
    state.valid = true;
    return state;
}

void ProjectSceneCameraSystem::applyLookAt(const LookAtState& state)
{
    if(!state.valid || !m_mainCameraNode || !m_mainCameraNode->camera()) {
        return;
    }
    m_mainCameraNode->camera()->lookAt(state.position, state.target, state.up);
    m_lookAt = state;
}

void ProjectSceneCameraSystem::animateTo(const LookAtState& state, double duration)
{
    if(!state.valid || !m_mainCameraNode || !m_mainCameraNode->camera()) {
        return;
    }
    if(!m_lookAt.valid) {
        applyLookAt(state);
        return;
    }

    m_animationStart = m_lookAt;
    m_animationEnd = state;
    m_animationElapsed = 0.0;
    m_animationDuration = std::max(0.05, duration);
    m_lastAnimationUpdateTime = -1.0;
    m_animationActive = true;
}

void ProjectSceneCameraSystem::setView(
    ProjectSceneCameraView view,
    const Bounds& bounds,
    bool animate)
{
    const LookAtState state = makeLookAtState(bounds, view, 1.10);
    if(animate) {
        animateTo(state, 0.35);
    } else {
        applyLookAt(state);
    }
}

void ProjectSceneCameraSystem::updateAnimation(double timeSeconds)
{
    if(!m_animationActive) {
        return;
    }
    if(m_lastAnimationUpdateTime < 0.0) {
        m_lastAnimationUpdateTime = timeSeconds;
        return;
    }

    const double delta = std::clamp(timeSeconds - m_lastAnimationUpdateTime, 0.0, 0.1);
    m_lastAnimationUpdateTime = timeSeconds;
    m_animationElapsed += delta;
    const double linearT = std::clamp(m_animationElapsed / m_animationDuration, 0.0, 1.0);
    const double smoothT = linearT * linearT * (3.0 - 2.0 * linearT);
    applyLookAt(interpolateLookAt(m_animationStart, m_animationEnd, smoothT));
    if(linearT >= 1.0) {
        m_animationActive = false;
        m_lastAnimationUpdateTime = -1.0;
    }
}

bool ProjectSceneCameraSystem::focusMountFrameLink(
    const std::string& robotId,
    const std::string& linkName,
    const Bounds& bounds)
{
    if(robotId.empty() || linkName.empty() || !bounds.valid) {
        return false;
    }
    if(m_mountFrameLinkFocusActive &&
        m_mountFrameFocusRobotId == robotId &&
        m_mountFrameFocusLinkName == linkName) {
        return false;
    }
    m_mountFrameLinkFocusActive = true;
    m_mountFrameFocusRobotId = robotId;
    m_mountFrameFocusLinkName = linkName;
    m_objectFrameObjectFocusActive = false;
    m_objectFrameFocusObjectId.clear();
    m_mountedAttachmentFocusActive = false;
    m_mountedAttachmentFocusId.clear();
    animateTo(makeLookAtState(bounds, ProjectSceneCameraView::Front, 1.8), 0.55);
    return true;
}

bool ProjectSceneCameraSystem::clearMountFrameLinkFocus()
{
    if(!m_mountFrameLinkFocusActive) {
        return false;
    }
    m_mountFrameLinkFocusActive = false;
    m_mountFrameFocusRobotId.clear();
    m_mountFrameFocusLinkName.clear();
    return true;
}

bool ProjectSceneCameraSystem::focusObjectFrameObject(
    const std::string& objectId,
    const Bounds& bounds)
{
    if(objectId.empty() || !bounds.valid) {
        return false;
    }
    if(m_objectFrameObjectFocusActive && m_objectFrameFocusObjectId == objectId) {
        return false;
    }
    m_objectFrameObjectFocusActive = true;
    m_objectFrameFocusObjectId = objectId;
    m_mountFrameLinkFocusActive = false;
    m_mountFrameFocusRobotId.clear();
    m_mountFrameFocusLinkName.clear();
    m_mountedAttachmentFocusActive = false;
    m_mountedAttachmentFocusId.clear();
    animateTo(makeLookAtState(bounds, ProjectSceneCameraView::Front, 1.8), 0.55);
    return true;
}

bool ProjectSceneCameraSystem::clearObjectFrameObjectFocus()
{
    if(!m_objectFrameObjectFocusActive) {
        return false;
    }
    m_objectFrameObjectFocusActive = false;
    m_objectFrameFocusObjectId.clear();
    return true;
}

bool ProjectSceneCameraSystem::focusMountedAttachment(
    const std::string& attachmentId,
    const Bounds& bounds)
{
    if(attachmentId.empty() || !bounds.valid) {
        return false;
    }
    if(m_mountedAttachmentFocusActive && m_mountedAttachmentFocusId == attachmentId) {
        return false;
    }
    m_mountedAttachmentFocusActive = true;
    m_mountedAttachmentFocusId = attachmentId;
    m_mountFrameLinkFocusActive = false;
    m_mountFrameFocusRobotId.clear();
    m_mountFrameFocusLinkName.clear();
    m_objectFrameObjectFocusActive = false;
    m_objectFrameFocusObjectId.clear();
    animateTo(makeLookAtState(bounds, ProjectSceneCameraView::Front, 1.8), 0.55);
    return true;
}

bool ProjectSceneCameraSystem::clearMountedAttachmentFocus()
{
    if(!m_mountedAttachmentFocusActive) {
        return false;
    }
    m_mountedAttachmentFocusActive = false;
    m_mountedAttachmentFocusId.clear();
    return true;
}

bool ProjectSceneCameraSystem::mountFrameLinkFocusActive() const
{
    return m_mountFrameLinkFocusActive;
}

const std::string& ProjectSceneCameraSystem::mountFrameFocusRobotId() const
{
    return m_mountFrameFocusRobotId;
}

const std::string& ProjectSceneCameraSystem::mountFrameFocusLinkName() const
{
    return m_mountFrameFocusLinkName;
}

bool ProjectSceneCameraSystem::objectFrameObjectFocusActive() const
{
    return m_objectFrameObjectFocusActive;
}

const std::string& ProjectSceneCameraSystem::objectFrameFocusObjectId() const
{
    return m_objectFrameFocusObjectId;
}

bool ProjectSceneCameraSystem::mountedAttachmentFocusActive() const
{
    return m_mountedAttachmentFocusActive;
}

const std::string& ProjectSceneCameraSystem::mountedAttachmentFocusId() const
{
    return m_mountedAttachmentFocusId;
}

bool ProjectSceneCameraSystem::renderMountedCamera(
    const simulation_runtime::RuntimeMountedCamera& mountedCamera,
    int width,
    int height,
    const Eigen::Vector4f& backgroundColor,
    scenecore::SceneGraph& graph,
    scenecore::Renderer& renderer,
    std::vector<unsigned char>& rgbaPixels,
    std::string* errorMessage)
{
    rgbaPixels.clear();
    if(width <= 0 || height <= 0 || !mountedCamera.enabled) {
        if(errorMessage != nullptr) {
            *errorMessage = "Camera render request is invalid: " + mountedCamera.attachmentId;
        }
        return false;
    }

    std::unique_ptr<RenderState>& state = m_renderStates[mountedCamera.attachmentId];
    if(!state) {
        state = std::make_unique<RenderState>();
        state->camera = cameracore::CameraFactory::asPerspectiveCamera(
            cameracore::CameraFactory::createPerspectiveCamera());
    }
    if(!state->camera || !state->target.initialize(width, height)) {
        if(errorMessage != nullptr) {
            *errorMessage = "Camera offscreen target initialization failed: " + mountedCamera.attachmentId;
        }
        return false;
    }

    const simulation_project::SensorIntrinsicsDesc& intrinsics = mountedCamera.intrinsics;
    if(!state->camera->setPerspective(
        static_cast<float>(intrinsics.fovY),
        static_cast<float>(width) / static_cast<float>(height),
        static_cast<float>(intrinsics.nearPlane),
        static_cast<float>(intrinsics.farPlane))) {
        if(errorMessage != nullptr) {
            *errorMessage = "Camera intrinsics are invalid: " + mountedCamera.attachmentId;
        }
        return false;
    }

    const Eigen::Isometry3d& worldOptical = mountedCamera.worldOptical;
    const Eigen::Vector3f eye = worldOptical.translation().cast<float>();
    const Eigen::Vector3f forward =
        (worldOptical.linear() * Eigen::Vector3d(0.0, 0.0, 1.0)).cast<float>();
    const Eigen::Vector3f up =
        (worldOptical.linear() * Eigen::Vector3d(0.0, -1.0, 0.0)).cast<float>();
    state->camera->lookAt(eye, eye + forward, up);

    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = { 0, 0, 1, 1 };
    GLfloat previousClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor);
    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);

    state->target.bind();
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(backgroundColor.x(), backgroundColor.y(), backgroundColor.z(), backgroundColor.w());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    scenecore::CameraNode cameraNode(state->camera, "sensorCamera");
    renderer.updateSceneUBO(graph, &cameraNode);
    scenecore::RendererOptions options;
    options.includeGrid = false;
    options.filter.layerMask = scenecore::renderLayerMask({
        scenecore::RenderLayer::VisualMesh,
        scenecore::RenderLayer::PointCloud });
    options.filter.categoryMask = scenecore::renderCategoryMask({
        scenecore::RenderCategory::Default,
        scenecore::RenderCategory::Robot,
        scenecore::RenderCategory::Environment });
    options.filter.featureMask = scenecore::renderFeatureMask({
        scenecore::RenderFeature::Default,
        scenecore::RenderFeature::Visual,
        scenecore::RenderFeature::PointCloud });
    renderer.render(graph, options);
    rgbaPixels = state->target.readRgba8();

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    glClearColor(
        previousClearColor[0],
        previousClearColor[1],
        previousClearColor[2],
        previousClearColor[3]);
    depthEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    blendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    cullEnabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);

    if(rgbaPixels.empty()) {
        if(errorMessage != nullptr) {
            *errorMessage = "Camera readback failed: " + mountedCamera.attachmentId;
        }
        return false;
    }
    return true;
}
