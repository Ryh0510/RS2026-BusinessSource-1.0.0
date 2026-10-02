#include "RobotViewport.h"

#include "ProjectScene.h"
#include "ProjectScenePickingService.h"
#include "CameraPreviewWidget.h"

#include <CustomLog/CustomLog.h>
#include <SimulationProject/ProjectDocument.h>

#include <QKeyEvent>
#include <QAction>
#include <QEvent>
#include <QImage>
#include <QMenu>
#include <QMouseEvent>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <chrono>
#include <algorithm>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

namespace
{
    class CurrentContextGuard
    {
    public:
        explicit CurrentContextGuard(QOpenGLWidget& widget)
            : m_widget(widget)
        {
        }

        ~CurrentContextGuard()
        {
            m_widget.doneCurrent();
        }

    private:
        QOpenGLWidget& m_widget;
    };

    double elapsedMilliseconds(const std::chrono::steady_clock::time_point& start)
    {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    void logProfileRow(const std::string& stage, double ms, const std::string& detail)
    {
        std::ostringstream out;
        out << "| " << std::left << std::setw(32) << stage
            << " | " << std::right << std::setw(10) << std::fixed << std::setprecision(2) << ms
            << " ms | " << detail;
        const std::string line = out.str();
        std::cout << line << "\n";
        LOG_DEBUG("rs2026") << line;
    }

    bool modeAllowsMountedAttachmentSelection(ProjectSceneInteractionMode mode)
    {
        return mode == ProjectSceneInteractionMode::Browse ||
            mode == ProjectSceneInteractionMode::SelectMount ||
            mode == ProjectSceneInteractionMode::SelectAttachment ||
            mode == ProjectSceneInteractionMode::SelectCollisionTarget;
    }

    QString pickKindName(ProjectScenePickTargetKind kind)
    {
        switch(kind) {
        case ProjectScenePickTargetKind::Robot:
            return QStringLiteral("robot");
        case ProjectScenePickTargetKind::RobotLink:
            return QStringLiteral("robotLink");
        case ProjectScenePickTargetKind::RobotMount:
            return QStringLiteral("robotMount");
        case ProjectScenePickTargetKind::MountedAttachment:
            return QStringLiteral("mountedAttachment");
        case ProjectScenePickTargetKind::SceneObject:
            return QStringLiteral("sceneObject");
        case ProjectScenePickTargetKind::PointCloud:
            return QStringLiteral("pointCloud");
        case ProjectScenePickTargetKind::None:
            break;
        }
        return QString();
    }

    QString toQString(const std::string& text)
    {
        return QString::fromStdString(text);
    }
}

struct RobotViewportCameraStreamState
{
    ProjectScene::CameraInfo info;
    CameraPreviewWidget* preview = nullptr;
    bool running = true;
    bool visible = true;
    int targetFps = 15;
    std::chrono::steady_clock::time_point nextFrame;
    std::chrono::steady_clock::time_point lastFrame;
    QString lastError;
};

RobotViewport::RobotViewport(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_scene(std::make_unique<ProjectScene>())
{
    setMinimumSize(640, 480);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_cameraStreamsMenu = new QMenu(this);
    m_cameraStreamsButton = new QToolButton(this);
    m_cameraStreamsButton->setObjectName(QStringLiteral("cameraStreamsButton"));
    m_cameraStreamsButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    m_cameraStreamsButton->setText(QStringLiteral("Cameras"));
    m_cameraStreamsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_cameraStreamsButton->setToolTip(QStringLiteral("Open, show, hide, and manage camera previews"));
    m_cameraStreamsButton->setPopupMode(QToolButton::InstantPopup);
    m_cameraStreamsButton->setMenu(m_cameraStreamsMenu);
    m_cameraStreamsButton->setMinimumSize(112, 36);
    m_cameraStreamsButton->hide();

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(16);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        update();
    });
    m_updateTimer->start();
}

RobotViewport::~RobotViewport()
{
    releaseScene();
}

void RobotViewport::releaseScene() noexcept
{
    clearCameraStreams();
    if(m_scene == nullptr) {
        return;
    }

    if(context() != nullptr && isValid()) {
        makeCurrent();
        m_scene.reset();
        doneCurrent();
        return;
    }

    m_scene.reset();
}

void RobotViewport::setJointPreview(int degrees)
{
    m_jointPreviewDegrees = degrees;
}

void RobotViewport::setRobotSummary(const char* name, std::size_t linkCount, std::size_t jointCount)
{
    m_robotName = name != nullptr ? name : "";
    m_linkCount = linkCount;
    m_jointCount = jointCount;
}

bool RobotViewport::loadProjectDocument(
    const simulation_project::ProjectDocument& document,
    const std::filesystem::path& basePath)
{
    const auto loadStart = std::chrono::steady_clock::now();
    m_lastError.clear();
    bool ok = true;
    try {
        const auto createStart = std::chrono::steady_clock::now();
        releaseScene();
        m_scene = std::make_unique<ProjectScene>();
        m_scene->setDefaultBackgroundColor(m_defaultBackgroundColor);
        m_scene->setEnvironmentPreset(m_environmentPreset);
        m_scene->setProjectDocument(document, basePath);
        m_scene->resize(width(), height());
        logProfileRow(
            "RobotViewport prepare scene",
            elapsedMilliseconds(createStart),
            "size=" + std::to_string(width()) + "x" + std::to_string(height()));
        m_treePublished = false;
        m_pendingProjectDocument = document;
        m_pendingProjectBasePath = basePath;
        m_hasPendingProjectDocument = context() == nullptr || !isValid();

        if(!m_hasPendingProjectDocument) {
            const auto initStart = std::chrono::steady_clock::now();
            ok = initializeSceneWithCurrentContext(true);
            logProfileRow(
                "RobotViewport initialize",
                elapsedMilliseconds(initStart),
                ok ? "ok=true" : "ok=false");
        }
    } catch(const std::exception& e) {
        ok = false;
        m_lastError = QString::fromLocal8Bit(e.what());
        LOG_ERROR("rs2026") << "RobotViewport loadProjectDocument failed: " << e.what();
    } catch(...) {
        ok = false;
        m_lastError = "Unknown viewport project load error.";
        LOG_ERROR("rs2026") << "RobotViewport loadProjectDocument failed: unknown error";
    }

    if(m_scene != nullptr) {
        m_scene->setInteractionMode(m_interactionMode);
    }
    m_startTime = Clock::now();
    m_firstPaintPending = true;
    m_cameraDragFramePending = false;
    const auto repaintStart = std::chrono::steady_clock::now();
    update();
    repaint();
    logProfileRow("RobotViewport repaint request", elapsedMilliseconds(repaintStart), "");
    LOG_DEBUG("rs2026") << "RobotViewport loadProjectDocument: elapsedMs=" << elapsedMilliseconds(loadStart)
        << ", initializedNow=" << (!m_hasPendingProjectDocument)
        << ", ok=" << ok;
    logProfileRow(
        "RobotViewport total",
        elapsedMilliseconds(loadStart),
        std::string("initializedNow=") + (!m_hasPendingProjectDocument ? "true" : "false") +
            " ok=" + (ok ? "true" : "false"));
    return ok;
}

std::filesystem::path RobotViewport::projectBasePath() const
{
    return m_scene ? m_scene->projectBasePath() : m_pendingProjectBasePath;
}

void RobotViewport::setDefaultBackgroundColor(const simulation_project::ColorDesc& color)
{
    m_defaultBackgroundColor = color;
    if(m_scene != nullptr) {
        m_scene->setDefaultBackgroundColor(color);
    }
    update();
}

void RobotViewport::setEnvironmentPreset(ProjectSceneEnvironmentPreset preset)
{
    m_environmentPreset = preset;
    if(m_scene != nullptr) {
        m_scene->setEnvironmentPreset(preset);
    }
    update();
}

ProjectSceneEnvironmentPreset RobotViewport::environmentPreset() const
{
    return m_scene != nullptr ? m_scene->environmentPreset() : m_environmentPreset;
}

bool RobotViewport::refreshCollisionConfiguration(
    const simulation_project::ProjectDocument& document,
    const std::filesystem::path& basePath)
{
    if(m_scene == nullptr) {
        return false;
    }

    if(context() == nullptr || !isValid()) {
        m_lastError = "Viewport OpenGL context is not valid for collision refresh.";
        return false;
    }

    bool ok = false;
    makeCurrent();
    try {
        ok = m_scene->refreshCollisionConfiguration(document, basePath);
    } catch(const std::exception& e) {
        ok = false;
        m_lastError = QString::fromLocal8Bit(e.what());
        LOG_ERROR("rs2026") << "RobotViewport refreshCollisionConfiguration failed: " << e.what();
    } catch(...) {
        ok = false;
        m_lastError = "Unknown viewport collision refresh error.";
        LOG_ERROR("rs2026") << "RobotViewport refreshCollisionConfiguration failed: unknown error";
    }
    doneCurrent();

    if(ok) {
        update();
    }
    return ok;
}

QString RobotViewport::lastError() const
{
    return m_lastError;
}

bool RobotViewport::loadToolAssetPreview(
    const simulation_project::AttachmentAssetDesc& asset,
    const std::filesystem::path& basePath)
{
    releaseScene();
    m_scene = std::make_unique<ProjectScene>();
    m_scene->setDefaultBackgroundColor(m_defaultBackgroundColor);
    m_scene->setEnvironmentPreset(ProjectSceneEnvironmentPreset::Studio);
    m_scene->setToolAssetPreview(asset, basePath);
    m_scene->setInteractionMode(m_interactionMode);
    m_scene->resize(width(), height());
    m_treePublished = true;
    m_hasPendingProjectDocument = false;

    bool ok = true;
    if(context() == nullptr || !isValid()) {
        ok = true;
    } else {
        ok = initializeSceneWithCurrentContext(true);
    }

    m_startTime = Clock::now();
    update();
    repaint();
    return ok;
}

bool RobotViewport::setActivePreviewRobotMount(const QString& robotMountId)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setActivePreviewRobotMount(robotMountId.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setPreviewRobotMountTransform(
    const QString& robotMountId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setPreviewRobotMountTransform(robotMountId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setPreviewMountedAttachmentTransform(
    const QString& attachmentId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setPreviewMountedAttachmentTransform(attachmentId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setPreviewAttachmentAsset(
    const simulation_project::AttachmentAssetDesc& asset)
{
    const bool changed = m_scene != nullptr && m_scene->setPreviewAttachmentAsset(asset);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setPreviewRobotMountLink(
    const QString& robotMountId,
    const QString& linkName)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setPreviewRobotMountLink(robotMountId.toStdString(), linkName.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::upsertPreviewRobotMount(const simulation_project::RobotMountDesc& mount)
{
    const bool changed = m_scene != nullptr && m_scene->upsertPreviewRobotMount(mount);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::removePreviewRobotMount(const QString& robotMountId)
{
    const bool changed = m_scene != nullptr && m_scene->removePreviewRobotMount(robotMountId.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

void RobotViewport::setRobotMountFrameVisibility(bool selectedLinkFrameVisible, bool mountFrameVisible)
{
    if(m_scene != nullptr) {
        m_scene->setRobotMountFrameVisibility(selectedLinkFrameVisible, mountFrameVisible);
        update();
    }
}

void RobotViewport::setPinnedRobotMountFrames(const QStringList& robotMountIds)
{
    std::vector<std::string> ids;
    ids.reserve(static_cast<std::size_t>(robotMountIds.size()));
    for(const QString& id : robotMountIds) {
        if(!id.isEmpty()) {
            ids.push_back(id.toStdString());
        }
    }

    if(m_scene != nullptr) {
        m_scene->setPinnedRobotMountFrames(ids);
        update();
    }
}

void RobotViewport::focusMountFrameLink(const QString& robotId, const QString& linkName)
{
    if(m_scene != nullptr) {
        m_scene->focusMountFrameLink(robotId.toStdString(), linkName.toStdString());
        update();
    }
}

void RobotViewport::clearMountFrameLinkFocus()
{
    if(m_scene != nullptr) {
        m_scene->clearMountFrameLinkFocus();
        update();
    }
}

void RobotViewport::focusObjectFrameObject(const QString& objectId)
{
    if(m_scene != nullptr) {
        m_scene->focusObjectFrameObject(objectId.toStdString());
        update();
    }
}

void RobotViewport::clearObjectFrameObjectFocus()
{
    if(m_scene != nullptr) {
        m_scene->clearObjectFrameObjectFocus();
        update();
    }
}

void RobotViewport::focusMountedAttachment(const QString& attachmentId)
{
    if(m_scene != nullptr) {
        m_scene->focusMountedAttachment(attachmentId.toStdString());
        update();
    }
}

void RobotViewport::clearMountedAttachmentFocus()
{
    if(m_scene != nullptr) {
        m_scene->clearMountedAttachmentFocus();
        update();
    }
}

void RobotViewport::previewObjectCollisionModelVariant(const QString& objectId, const QString& variantId)
{
    if(m_scene != nullptr) {
        if(context() == nullptr || !isValid()) {
            m_lastError = "Viewport OpenGL context is not valid for collision model preview.";
            return;
        }

        makeCurrent();
        try {
            m_scene->previewObjectCollisionModelVariant(objectId.toStdString(), variantId.toStdString());
        } catch(const std::exception& e) {
            m_lastError = QString::fromLocal8Bit(e.what());
            LOG_ERROR("rs2026") << "RobotViewport previewObjectCollisionModelVariant failed: " << e.what();
        } catch(...) {
            m_lastError = "Unknown viewport collision model preview error.";
            LOG_ERROR("rs2026") << "RobotViewport previewObjectCollisionModelVariant failed: unknown error";
        }
        doneCurrent();
        update();
    }
}

void RobotViewport::clearObjectCollisionModelVariantPreview()
{
    if(m_scene != nullptr) {
        m_scene->clearObjectCollisionModelVariantPreview();
        update();
    }
}

void RobotViewport::previewCollisionPairTargets(
    const QString& robotAId,
    const QString& linkAName,
    const QString& objectAId,
    const QString& attachmentAId,
    const QString& robotBId,
    const QString& linkBName,
    const QString& objectBId,
    const QString& attachmentBId)
{
    if(m_scene != nullptr) {
        m_scene->previewCollisionPairTargets(
            robotAId.toStdString(),
            linkAName.toStdString(),
            objectAId.toStdString(),
            attachmentAId.toStdString(),
            robotBId.toStdString(),
            linkBName.toStdString(),
            objectBId.toStdString(),
            attachmentBId.toStdString());
        update();
    }
}

void RobotViewport::selectRobotLink(const QString& robotId, const QString& linkName)
{
    if(m_scene != nullptr) {
        m_scene->setSelectedLink(robotId.toStdString(), linkName.toStdString());
        update();
    }
}

void RobotViewport::selectRobotJointFrame(const QString& robotId, const QString& jointName)
{
    if(m_scene != nullptr) {
        m_scene->setSelectedJointFrame(robotId.toStdString(), jointName.toStdString());
        update();
    }
}

void RobotViewport::selectRobotMount(const QString& robotId, const QString& linkName, const QString& robotMountId)
{
    if(m_scene != nullptr) {
        m_scene->setSelectedRobotMount(
            robotId.toStdString(),
            linkName.toStdString(),
            robotMountId.toStdString());
        update();
    }
}

void RobotViewport::selectSceneObject(const QString& objectId)
{
    if(m_scene != nullptr) {
        m_scene->setSelectedSceneObject(objectId.toStdString());
        update();
    }
}

void RobotViewport::selectToolAttachment(const QString& attachmentId)
{
    selectMountedAttachment(attachmentId);
}

void RobotViewport::selectMountedAttachment(const QString& attachmentId)
{
    if(m_scene != nullptr) {
        m_scene->setSelectedMountedAttachment(attachmentId.toStdString());
        update();
    }
}

void RobotViewport::previewRobotBaseTransform(
    const QString& robotId,
    const simulation_project::TransformDesc& transform)
{
    if(m_scene != nullptr) {
        m_scene->setRobotBaseTransform(robotId.toStdString(), transform);
        update();
    }
}

void RobotViewport::setRobotJointValue(
    const QString& robotId,
    const QString& jointName,
    double value)
{
    if(m_scene != nullptr) {
        m_scene->setRobotJointValue(robotId.toStdString(), jointName.toStdString(), value);
        update();
    }
}

double RobotViewport::robotJointValue(
    const QString& robotId,
    const QString& jointName,
    bool* ok) const
{
    double value = 0.0;
    const bool found = m_scene != nullptr
        && m_scene->robotJointValue(robotId.toStdString(), jointName.toStdString(), value);
    if(ok != nullptr) {
        *ok = found;
    }
    return value;
}

void RobotViewport::setRobotAutoMotion(
    const QString& robotId,
    bool enabled,
    double amplitude,
    double speed)
{
    if(m_scene != nullptr) {
        m_scene->setRobotAutoMotion(robotId.toStdString(), enabled, amplitude, speed);
        update();
    }
}

void RobotViewport::previewSceneObjectTransform(
    const QString& objectId,
    const simulation_project::TransformDesc& transform)
{
    if(m_scene != nullptr) {
        m_scene->previewSceneObjectTransform(objectId.toStdString(), transform);
        update();
    }
}

bool RobotViewport::setSceneObjectTransform(
    const QString& objectId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setSceneObjectTransform(objectId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::removeSceneObject(const QString& objectId)
{
    const bool changed = m_scene != nullptr && m_scene->removeSceneObject(objectId.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

void RobotViewport::selectObjectFrame(const QString& objectId, const QString& frameId)
{
    if(m_scene != nullptr) {
        m_scene->setSelectedObjectFrame(objectId.toStdString(), frameId.toStdString());
        update();
    }
}

bool RobotViewport::previewObjectFrameTransform(
    const QString& objectId,
    const QString& frameId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setPreviewObjectFrameTransform(
            objectId.toStdString(),
            frameId.toStdString(),
            transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::upsertPreviewObjectFrame(
    const QString& objectId,
    const simulation_project::ObjectFrameDesc& frame)
{
    const bool changed = m_scene != nullptr &&
        m_scene->upsertPreviewObjectFrame(objectId.toStdString(), frame);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setRobotMountTransform(
    const QString& robotMountId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setRobotMountTransform(robotMountId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setToolAttachmentTransform(
    const QString& attachmentId,
    const simulation_project::TransformDesc& transform)
{
    return setMountedAttachmentTransform(attachmentId, transform);
}

bool RobotViewport::setMountedAttachmentTransform(
    const QString& attachmentId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setMountedAttachmentTransform(attachmentId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setToolAssetMountToVisual(
    const QString& assetId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setToolAssetMountToVisual(assetId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setToolAssetTcp(
    const QString& assetId,
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setToolAssetTcp(assetId.toStdString(), transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setToolAssetPreviewMountToVisual(
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setToolAssetPreviewMountToVisual(transform);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setToolAssetPreviewTcp(
    const simulation_project::TransformDesc& transform)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setToolAssetPreviewTcp(transform);
    if(changed) {
        update();
    }
    return changed;
}

void RobotViewport::setCollisionGeometryVisible(bool visible)
{
    if(m_scene != nullptr) {
        m_scene->setShowCollisionGeometry(visible);
        update();
    }
}

bool RobotViewport::setVisibleRobotCollisionVariant(
    const QString& robotId,
    const QString& linkName,
    const QString& variantId)
{
    const bool changed = m_scene != nullptr &&
        m_scene->setVisibleRobotCollisionVariant(
            robotId.toStdString(),
            linkName.toStdString(),
            variantId.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

QString RobotViewport::visibleRobotCollisionVariant(
    const QString& robotId,
    const QString& linkName) const
{
    return m_scene != nullptr
        ? QString::fromStdString(m_scene->visibleRobotCollisionVariant(
              robotId.toStdString(),
              linkName.toStdString()))
        : QString();
}

std::vector<ProjectScene::CollisionDetectorInfo> RobotViewport::collisionDetectors() const
{
    return m_scene != nullptr ? m_scene->collisionDetectors() : std::vector<ProjectScene::CollisionDetectorInfo>();
}

bool RobotViewport::collisionQueriesEnabled() const
{
    return m_scene != nullptr && m_scene->collisionQueriesEnabled();
}

bool RobotViewport::setCollisionQueriesEnabled(bool enabled)
{
    const bool changed = m_scene != nullptr && m_scene->setCollisionQueriesEnabled(enabled);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setActiveCollisionDetector(const QString& id)
{
    const bool changed = m_scene != nullptr && m_scene->setActiveCollisionDetector(id.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::refreshCollisionDetectorNearest(const QString& id)
{
    const bool refreshed =
        m_scene != nullptr && m_scene->refreshCollisionDetectorNearest(id.toStdString());
    if(refreshed) {
        update();
    }
    return refreshed;
}

bool RobotViewport::setCollisionDetectorEnabled(const QString& id, bool enabled)
{
    const bool changed = m_scene != nullptr && m_scene->setCollisionDetectorEnabled(id.toStdString(), enabled);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::setCollisionDetectorVisible(const QString& id, bool visible)
{
    const bool changed = m_scene != nullptr && m_scene->setCollisionDetectorVisible(id.toStdString(), visible);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::updateCollisionDetectorRuntimeOptions(const simulation_project::CollisionDetectorDesc& desc)
{
    const bool changed = m_scene != nullptr && m_scene->updateCollisionDetectorRuntimeOptions(desc);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::rebuildCollisionDetectorsFromDocument(const simulation_project::ProjectDocument& document)
{
    const bool changed = m_scene != nullptr && m_scene->rebuildCollisionDetectorsFromDocument(document);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::removeCollisionDetector(const QString& id)
{
    const bool changed = m_scene != nullptr && m_scene->removeCollisionDetector(id.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::generateRobotCollisionProxy(
    const QString& robotId,
    const QString& linkName,
    const QString& proxyType,
    simulation_project::CollisionElementOverrideDesc& element) const
{
    return m_scene != nullptr &&
        m_scene->generateRobotCollisionProxy(
            robotId.toStdString(),
            linkName.toStdString(),
            proxyType.toStdString(),
            element);
}

bool RobotViewport::generateRobotCollisionProxies(
    const QString& robotId,
    const QString& linkName,
    const RobotCollisionProxyRequest& request,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateRobotCollisionProxies(
            robotId.toStdString(),
            linkName.toStdString(),
            request,
            elements);
}

bool RobotViewport::generateRobotCollisionProxiesFromExistingCollision(
    const QString& robotId,
    const QString& linkName,
    const RobotCollisionProxyRequest& request,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateRobotCollisionProxiesFromExistingCollision(
            robotId.toStdString(),
            linkName.toStdString(),
            request,
            elements);
}

bool RobotViewport::generateRobotCollisionProxiesFromExistingCollision(
    const QString& robotId,
    const RobotCollisionProxyRequest& request,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateRobotCollisionProxiesFromExistingCollision(
            robotId.toStdString(),
            request,
            elements);
}

bool RobotViewport::generateRobotCollisionCoacdFromVisual(
    const QString& robotId,
    const QString& linkName,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateRobotCollisionCoacdFromVisual(
            robotId.toStdString(),
            linkName.toStdString(),
            elements);
}

bool RobotViewport::generateRobotCollisionCoacdFromExistingCollision(
    const QString& robotId,
    const QString& linkName,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateRobotCollisionCoacdFromExistingCollision(
            robotId.toStdString(),
            linkName.toStdString(),
            elements);
}

bool RobotViewport::generateObjectCollisionCoacdFromVisual(
    const QString& objectId,
    std::vector<simulation_project::ObjectCollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateObjectCollisionCoacdFromVisual(
            objectId.toStdString(),
            elements);
}

bool RobotViewport::evaluateRobotCollisionProxyQuality(
    const QString& robotId,
    const QString& linkName,
    const RobotCollisionProxyRequest& request,
    const std::vector<simulation_project::CollisionElementOverrideDesc>& elements,
    bool useExistingCollisionInput,
    RobotCollisionProxyQualitySummary& summary) const
{
    return m_scene != nullptr &&
        m_scene->evaluateRobotCollisionProxyQuality(
            robotId.toStdString(),
            linkName.toStdString(),
            request,
            elements,
            useExistingCollisionInput,
            summary);
}

bool RobotViewport::generateMissingRobotCollisionProxies(
    const QString& robotId,
    const QString& proxyType,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateMissingRobotCollisionProxies(
            robotId.toStdString(),
            proxyType.toStdString(),
            elements);
}

bool RobotViewport::generateMissingRobotCollisionProxies(
    const QString& robotId,
    const RobotCollisionProxyRequest& request,
    std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
{
    return m_scene != nullptr &&
        m_scene->generateMissingRobotCollisionProxies(
            robotId.toStdString(),
            request,
            elements);
}

RobotCollisionRobotSummary RobotViewport::robotCollisionSummary(
    const QString& robotId) const
{
    return m_scene != nullptr
        ? m_scene->robotCollisionSummary(robotId.toStdString())
        : RobotCollisionRobotSummary();
}

std::vector<ProjectScene::ToolAttachmentInfo> RobotViewport::toolAttachments() const
{
    return mountedAttachments();
}

std::vector<ProjectScene::MountedAttachmentInfo> RobotViewport::mountedAttachments() const
{
    return m_scene != nullptr ? m_scene->mountedAttachments() : std::vector<ProjectScene::MountedAttachmentInfo>();
}

bool RobotViewport::setActiveToolAttachment(const QString& id)
{
    return setActiveMountedAttachment(id);
}

bool RobotViewport::setActiveMountedAttachment(const QString& id)
{
    const bool changed = m_scene != nullptr && m_scene->setActiveMountedAttachment(id.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

void RobotViewport::setActiveToolFrameRobot(const QString& robotId)
{
    if(m_scene != nullptr) {
        m_scene->setActiveToolFrameRobot(robotId.toStdString());
        update();
    }
}

void RobotViewport::setToolFrameVisibility(const ProjectScene::ToolFrameVisibility& visibility)
{
    if(m_scene != nullptr) {
        m_scene->setToolFrameVisibility(visibility);
        update();
    }
}

ProjectScene::RobotForwardKinematics RobotViewport::robotForwardKinematics(
    const QString& robotId, const std::vector<std::string>& jointNames, bool includeTool) const
{
    return m_scene ? m_scene->robotForwardKinematics(robotId.toStdString(), jointNames, includeTool)
        : ProjectScene::RobotForwardKinematics{};
}

void RobotViewport::setSprayRangeVisible(const QString& robotId, bool visible)
{
    if(m_scene != nullptr) {
        m_scene->setSprayRangeVisible(robotId.toStdString(), visible);
        update();
    }
}

void RobotViewport::setEndEffectorTraceVisible(const QString& robotId, bool visible)
{
    if(m_scene) {
        m_scene->setEndEffectorTraceVisible(robotId.toStdString(), visible);
        update();
    }
}

void RobotViewport::clearEndEffectorTrace()
{
    if(m_scene) {
        m_scene->clearEndEffectorTrace();
        update();
    }
}

void RobotViewport::appendEndEffectorTraceSample()
{
    if(m_scene) {
        m_scene->appendEndEffectorTraceSample();
        update();
    }
}

ProjectScene::SprayMeasurement RobotViewport::sprayMeasurement(const QString& robotId) const
{
    if(m_scene) {
        return m_scene->sprayMeasurement(robotId.toStdString());
    }
    ProjectScene::SprayMeasurement result;
    result.errorMessage = "Scene unavailable";
    return result;
}

std::vector<ProjectScene::RobotLinkMaterialInfo> RobotViewport::robotLinkMaterials(
    const QString& robotId,
    const QString& linkName) const
{
    return m_scene != nullptr
        ? m_scene->robotLinkMaterials(robotId.toStdString(), linkName.toStdString())
        : std::vector<ProjectScene::RobotLinkMaterialInfo>();
}

void RobotViewport::resetCamera()
{
    m_lastMousePos = QPoint();
    setCameraView(ProjectSceneCameraView::Home);
}

void RobotViewport::setCameraView(ProjectSceneCameraView view)
{
    m_lastMousePos = QPoint();
    if(m_scene != nullptr) {
        m_scene->setCameraView(view);
        update();
    }
}

void RobotViewport::setInteractionMode(ProjectSceneInteractionMode mode)
{
    if(m_interactionMode == mode) {
        return;
    }
    m_interactionMode = mode;
    if(m_scene != nullptr) {
        m_scene->setInteractionMode(mode);
        update();
    }
}

ProjectSceneInteractionMode RobotViewport::interactionMode() const
{
    return m_interactionMode;
}

bool RobotViewport::applySurfaceScalarOverlay(
    const smrobot::visualization::SurfaceScalarOverlay& overlay,
    QString* errorMessage)
{
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        if(errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Viewport OpenGL context is not available.");
        }
        return false;
    }

    std::string error;
    bool ok = false;
    makeCurrent();
    try {
        ok = m_scene->applySurfaceScalarOverlay(overlay, &error);
    } catch(const std::exception& exception) {
        error = exception.what();
    } catch(...) {
        error = "Unknown surface scalar overlay error.";
    }
    doneCurrent();
    if(errorMessage != nullptr) {
        *errorMessage = QString::fromStdString(error);
    }
    if(ok) {
        update();
    }
    return ok;
}

bool RobotViewport::setSurfaceScalarOverlayVisible(const QString& objectId, bool visible)
{
    if(m_scene == nullptr) {
        return false;
    }
    const bool ok = m_scene->setSurfaceScalarOverlayVisible(objectId.toStdString(), visible);
    if(ok) {
        update();
    }
    return ok;
}

bool RobotViewport::clearSurfaceScalarOverlay(const QString& objectId)
{
    if(m_scene == nullptr) {
        return false;
    }
    const bool ok = m_scene->clearSurfaceScalarOverlay(objectId.toStdString());
    if(ok) {
        update();
    }
    return ok;
}

void RobotViewport::setTrajectoryControlPointOverlay(
    const QString& trajectoryId,
    const std::vector<simulation_project::TransformDesc>& controlPoints, bool showPoints)
{
    if(m_scene != nullptr) {
        m_scene->setTrajectoryControlPointOverlay(trajectoryId.toStdString(), controlPoints, showPoints);
        update();
    }
}

void RobotViewport::clearTrajectoryControlPointOverlay(const QString& trajectoryId)
{
    if(m_scene != nullptr) {
        m_scene->clearTrajectoryControlPointOverlay(trajectoryId.toStdString());
        update();
    }
}

smrobot::visualization::CustomMeshResult RobotViewport::upsertCustomMesh(
    const smrobot::visualization::CustomMeshDesc& desc,
    smrobot::visualization::CustomMeshHandle& handle)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    CustomMeshResult result;
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    try {
        result = m_scene->upsertCustomMesh(desc, handle);
    }
    catch(const std::exception& exception) {
        result = CustomMeshResult::fail(CustomMeshError::InternalError, exception.what());
    }
    catch(...) {
        result = CustomMeshResult::fail(CustomMeshError::InternalError, "Unknown custom mesh error.");
    }
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::updateCustomMeshGeometry(
    smrobot::visualization::CustomMeshHandle handle,
    const smrobot::visualization::MeshData& mesh)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->updateCustomMeshGeometry(handle, mesh);
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::updateCustomMeshColors(
    smrobot::visualization::CustomMeshHandle handle,
    const std::vector<smrobot::visualization::Color4f>& colors)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->updateCustomMeshColors(handle, colors);
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::setCustomMeshTransform(
    smrobot::visualization::CustomMeshHandle handle,
    const smrobot::visualization::TransformMatrix& transform)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->setCustomMeshTransform(handle, transform);
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::setCustomMeshAppearance(
    smrobot::visualization::CustomMeshHandle handle,
    const smrobot::visualization::MeshAppearance& appearance)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->setCustomMeshAppearance(handle, appearance);
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::setCustomMeshVisible(
    smrobot::visualization::CustomMeshHandle handle,
    bool visible)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->setCustomMeshVisible(handle, visible);
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::removeCustomMesh(
    smrobot::visualization::CustomMeshHandle handle)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->removeCustomMesh(handle);
    if(result.success) {
        update();
    }
    return result;
}

smrobot::visualization::CustomMeshResult RobotViewport::clearCustomMeshes(
    const std::string& ownerId)
{
    using namespace smrobot::visualization;
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return CustomMeshResult::fail(
            CustomMeshError::GraphicsContextUnavailable,
            "Viewport OpenGL context is not available.");
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    CustomMeshResult result = m_scene->clearCustomMeshes(ownerId);
    if(result.success) {
        update();
    }
    return result;
}

void RobotViewport::setSurfaceScalarProbeEnabled(bool enabled, const QString& objectId)
{
    m_surfaceScalarProbeEnabled = enabled;
    m_surfaceScalarProbeObjectId = enabled ? objectId : QString();
    m_lastSurfaceScalarProbeTime = Clock::time_point();
    if(!enabled) {
        emit surfaceScalarHovered(QString(), 0.0, QPoint(), false);
    }
}

bool RobotViewport::initializeSceneWithCurrentContext(bool releaseContext)
{
    const auto initializeStart = std::chrono::steady_clock::now();
    if(m_scene == nullptr || context() == nullptr) {
        return false;
    }

    if(releaseContext) {
        makeCurrent();
    }
    bool ok = true;
    try {
        ok = m_scene->initialize();
    } catch(const std::exception& e) {
        ok = false;
        m_lastError = QString::fromLocal8Bit(e.what());
        LOG_ERROR("rs2026") << "RobotViewport initializeSceneWithCurrentContext failed: " << e.what();
    } catch(...) {
        ok = false;
        m_lastError = "Unknown viewport scene initialize error.";
        LOG_ERROR("rs2026") << "RobotViewport initializeSceneWithCurrentContext failed: unknown error";
    }
    if(ok) {
        publishRobotLinks();
        syncCameraStreams();
    }
    if(releaseContext) {
        doneCurrent();
    }
    LOG_DEBUG("rs2026") << "RobotViewport initializeSceneWithCurrentContext: elapsedMs="
        << elapsedMilliseconds(initializeStart)
        << ", releaseContext=" << releaseContext
        << ", ok=" << ok;
    return ok;
}

void RobotViewport::initializeGL()
{
    if(m_hasPendingProjectDocument) {
        if(m_scene == nullptr) {
            m_scene = std::make_unique<ProjectScene>();
            m_scene->setDefaultBackgroundColor(m_defaultBackgroundColor);
            m_scene->setEnvironmentPreset(m_environmentPreset);
            m_scene->setProjectDocument(m_pendingProjectDocument, m_pendingProjectBasePath);
            m_scene->resize(width(), height());
            m_treePublished = false;
        }
        m_hasPendingProjectDocument = false;
    }
    initializeSceneWithCurrentContext(false);
    m_startTime = Clock::now();
}

void RobotViewport::publishRobotLinks()
{
    if(m_scene == nullptr || !m_scene->isInitialized() || m_treePublished) {
        return;
    }

    for(const auto& group : m_scene->robotLinks()) {
        QStringList links;
        for(const auto& link : group.links) {
            links.push_back(QString::fromStdString(link));
        }
        QStringList joints;
        for(const auto& joint : group.joints) {
            joints.push_back(QString::fromStdString(joint));
        }
        QStringList movableJoints;
        QStringList movableJointTypes;
        for(const auto& joint : group.movableJoints) {
            movableJoints.push_back(QString::fromStdString(joint.jointName));
            movableJointTypes.push_back(QString::fromStdString(joint.jointType));
        }
        emit robotLinksAvailable(
            QString::fromStdString(group.robotId),
            QString::fromStdString(group.robotName),
            links,
            joints,
            movableJoints,
            movableJointTypes);
    }

    for(const auto& object : m_scene->sceneObjects()) {
        emit sceneObjectAvailable(
            QString::fromStdString(object.objectId),
            QString::fromStdString(object.objectName));
    }
    m_treePublished = true;
}

void RobotViewport::resizeGL(int width, int height)
{
    if(m_scene != nullptr) {
        m_scene->resize(width, height);
    }
    updateCameraOverlayGeometry();
}

void RobotViewport::paintGL()
{
    if(m_scene == nullptr || !m_scene->isInitialized()) {
        return;
    }

    const auto frameStart = Clock::now();
    const auto now = Clock::now();
    const std::chrono::duration<double> elapsed = now - m_startTime;
    const auto updateStart = Clock::now();
    m_scene->update(elapsed.count());
    emit robotStateUpdated();
    const double updateMs = elapsedMilliseconds(updateStart);
    const auto renderStart = Clock::now();
    m_scene->render();
    const double renderMs = elapsedMilliseconds(renderStart);
    const double cameraRenderMs = renderDueCameraStreams();
    const double totalMs = elapsedMilliseconds(frameStart);

    if(m_firstPaintPending || m_cameraDragFramePending || totalMs > 16.0) {
        const auto detectors = m_scene->collisionDetectors();
        const auto active = std::find_if(detectors.begin(), detectors.end(),
            [](const ProjectScene::CollisionDetectorInfo& detector) {
                return detector.active;
            });
        const ProjectScene::CollisionDetectorInfo* info =
            active != detectors.end() ? &*active : nullptr;
        LOG_DEBUG("rs2026") << "RobotViewport frame profile: reason="
            << (m_firstPaintPending ? "firstPaint" :
                (m_cameraDragFramePending ? "cameraDrag" : "slowFrame"))
            << ", totalMs=" << totalMs
            << ", updateMs=" << updateMs
            << ", renderMs=" << renderMs
            << ", cameraRenderMs=" << cameraRenderMs
            << ", poseMs=" << (info ? info->frameRobotPoseMs : 0.0)
            << ", collisionWorldMs=" << (info ? info->frameCollisionWorldUpdateMs : 0.0)
            << ", queryMs=" << (info ? info->lastQueryMs : 0.0)
            << ", overlayMs=" << (info ? info->frameOverlayMs : 0.0)
            << ", overlayDebugBuildMs=" << (info ? info->frameOverlayDebugBuildMs : 0.0)
            << ", overlaySubmitMs=" << (info ? info->frameOverlayDebugSubmitMs : 0.0)
            << ", overlayGeometries=" << (info ? info->frameOverlayGeometryCount : 0);
    }
    m_firstPaintPending = false;
    m_cameraDragFramePending = false;
}

QStringList RobotViewport::cameraIds() const
{
    QStringList result;
    for(const auto& stream : m_cameraStreams) {
        result.push_back(QString::fromStdString(stream->info.attachmentId));
    }
    return result;
}

RobotViewportCameraStreamState* RobotViewport::cameraStream(const QString& cameraId) const
{
    for(const auto& stream : m_cameraStreams) {
        if(QString::fromStdString(stream->info.attachmentId) == cameraId) {
            return stream.get();
        }
    }
    return nullptr;
}

bool RobotViewport::setCameraStreamRunning(const QString& cameraId, bool running)
{
    RobotViewportCameraStreamState* stream = cameraStream(cameraId);
    if(stream == nullptr || (running && !stream->info.enabled)) {
        return false;
    }
    stream->running = running;
    stream->nextFrame = Clock::time_point();
    if(stream->preview != nullptr && stream->preview->streamRunning() != running) {
        stream->preview->setStreamRunning(running);
    }
    QTimer::singleShot(0, this, [this]() { rebuildCameraStreamsMenu(); });
    update();
    return true;
}

bool RobotViewport::setCameraPreviewVisible(const QString& cameraId, bool visible)
{
    RobotViewportCameraStreamState* stream = cameraStream(cameraId);
    if(stream == nullptr || stream->preview == nullptr) {
        return false;
    }
    stream->visible = visible;
    stream->preview->setVisible(visible);
    if(visible) {
        stream->preview->clampToParent();
        stream->preview->raise();
        m_cameraStreamsButton->raise();
    }
    QTimer::singleShot(0, this, [this]() { rebuildCameraStreamsMenu(); });
    update();
    return true;
}

void RobotViewport::setAllCameraStreamsRunning(bool running)
{
    for(const auto& stream : m_cameraStreams) {
        if(!running || stream->info.enabled) {
            stream->running = running;
            stream->nextFrame = Clock::time_point();
            stream->preview->setStreamRunning(running);
        }
    }
    QTimer::singleShot(0, this, [this]() { rebuildCameraStreamsMenu(); });
    update();
}

void RobotViewport::setAllCameraPreviewsVisible(bool visible)
{
    for(const auto& stream : m_cameraStreams) {
        stream->visible = visible;
        stream->preview->setVisible(visible);
        if(visible) {
            stream->preview->clampToParent();
        }
    }
    if(m_cameraStreamsButton != nullptr) {
        m_cameraStreamsButton->raise();
    }
    QTimer::singleShot(0, this, [this]() { rebuildCameraStreamsMenu(); });
    update();
}

void RobotViewport::syncCameraStreams()
{
    if(m_scene == nullptr || !m_scene->isInitialized()) {
        clearCameraStreams();
        return;
    }

    const std::vector<ProjectScene::CameraInfo> cameras = m_scene->cameras();
    for(auto it = m_cameraStreams.begin(); it != m_cameraStreams.end();) {
        const bool exists = std::any_of(cameras.begin(), cameras.end(), [&](const ProjectScene::CameraInfo& camera) {
            return camera.attachmentId == (*it)->info.attachmentId;
        });
        if(!exists) {
            delete (*it)->preview;
            it = m_cameraStreams.erase(it);
        } else {
            ++it;
        }
    }

    for(const ProjectScene::CameraInfo& camera : cameras) {
        const QString cameraId = QString::fromStdString(camera.attachmentId);
        RobotViewportCameraStreamState* existing = cameraStream(cameraId);
        if(existing != nullptr) {
            existing->info = camera;
            if(!camera.enabled) {
                existing->running = false;
                existing->preview->setStreamRunning(false);
            }
            continue;
        }

        auto stream = std::make_unique<RobotViewportCameraStreamState>();
        stream->info = camera;
        stream->running = camera.enabled;
        stream->visible = camera.enabled;
        stream->preview = new CameraPreviewWidget(
            cameraId,
            QString::fromStdString(camera.name),
            this);
        stream->preview->setStreamRunning(stream->running);
        RobotViewportCameraStreamState* raw = stream.get();
        connect(stream->preview, &CameraPreviewWidget::streamRunningChanged, this,
            [this, raw](bool running) {
                if(running && !raw->info.enabled) {
                    raw->preview->setStreamRunning(false);
                    return;
                }
                raw->running = running;
                raw->nextFrame = Clock::time_point();
                QTimer::singleShot(0, this, [this]() { rebuildCameraStreamsMenu(); });
            });
        connect(stream->preview, &CameraPreviewWidget::previewVisibilityChanged, this,
            [this, raw](bool visible) {
                raw->visible = visible;
                QTimer::singleShot(0, this, [this]() { rebuildCameraStreamsMenu(); });
            });
        layoutNewCameraPreview(*stream, static_cast<int>(m_cameraStreams.size()));
        stream->preview->show();
        m_cameraStreams.push_back(std::move(stream));
    }

    m_cameraStreamsButton->setVisible(!m_cameraStreams.empty());
    updateCameraOverlayGeometry();
    rebuildCameraStreamsMenu();
}

void RobotViewport::clearCameraStreams()
{
    for(auto& stream : m_cameraStreams) {
        delete stream->preview;
        stream->preview = nullptr;
    }
    m_cameraStreams.clear();
    m_cameraRoundRobinIndex = 0;
    if(m_cameraStreamsButton != nullptr) {
        m_cameraStreamsButton->hide();
    }
    if(m_cameraStreamsMenu != nullptr) {
        m_cameraStreamsMenu->clear();
    }
}

void RobotViewport::layoutNewCameraPreview(RobotViewportCameraStreamState& stream, int index)
{
    const int margin = 12;
    const int gap = 8;
    const int columns = std::max(1, (width() - margin * 2) / 328);
    const int column = index % columns;
    const int row = index / columns;
    stream.preview->move(
        margin + column * (stream.preview->width() + gap),
        margin + row * (stream.preview->height() + gap));
    stream.preview->clampToParent();
}

void RobotViewport::tileCameraPreviews()
{
    int index = 0;
    for(const auto& stream : m_cameraStreams) {
        if(stream->preview != nullptr && stream->visible) {
            layoutNewCameraPreview(*stream, index++);
        }
    }
}

void RobotViewport::updateCameraOverlayGeometry()
{
    if(m_cameraStreamsButton != nullptr) {
        m_cameraStreamsButton->move(12, std::max(12, height() - m_cameraStreamsButton->height() - 12));
        m_cameraStreamsButton->raise();
    }
    for(const auto& stream : m_cameraStreams) {
        stream->preview->clampToParent();
    }
}

void RobotViewport::rebuildCameraStreamsMenu()
{
    if(m_cameraStreamsMenu == nullptr) {
        return;
    }
    m_cameraStreamsMenu->clear();
    const int visibleCount = static_cast<int>(std::count_if(
        m_cameraStreams.begin(), m_cameraStreams.end(),
        [](const std::unique_ptr<RobotViewportCameraStreamState>& stream) {
            return stream != nullptr && stream->visible;
        }));
    m_cameraStreamsButton->setText(QStringLiteral("Cameras %1/%2")
        .arg(visibleCount)
        .arg(m_cameraStreams.size()));
    m_cameraStreamsButton->setToolTip(QStringLiteral(
        "Manage camera previews. Hidden previews can always be reopened here."));
    QAction* runAll = m_cameraStreamsMenu->addAction(QStringLiteral("Run all"));
    runAll->setObjectName(QStringLiteral("cameraStreamsRunAllAction"));
    connect(runAll, &QAction::triggered, this, [this]() { setAllCameraStreamsRunning(true); });
    QAction* stopAll = m_cameraStreamsMenu->addAction(QStringLiteral("Stop all"));
    stopAll->setObjectName(QStringLiteral("cameraStreamsStopAllAction"));
    connect(stopAll, &QAction::triggered, this, [this]() { setAllCameraStreamsRunning(false); });
    QAction* showAll = m_cameraStreamsMenu->addAction(QStringLiteral("Show all previews"));
    showAll->setObjectName(QStringLiteral("cameraStreamsShowAllAction"));
    connect(showAll, &QAction::triggered, this, [this]() { setAllCameraPreviewsVisible(true); });
    QAction* hideAll = m_cameraStreamsMenu->addAction(QStringLiteral("Hide all previews"));
    hideAll->setObjectName(QStringLiteral("cameraStreamsHideAllAction"));
    connect(hideAll, &QAction::triggered, this, [this]() { setAllCameraPreviewsVisible(false); });
    QAction* tileAll = m_cameraStreamsMenu->addAction(QStringLiteral("Tile previews"));
    tileAll->setObjectName(QStringLiteral("cameraStreamsTileAction"));
    connect(tileAll, &QAction::triggered, this, &RobotViewport::tileCameraPreviews);
    m_cameraStreamsMenu->addSeparator();

    for(const auto& stream : m_cameraStreams) {
        const QString id = QString::fromStdString(stream->info.attachmentId);
        QMenu* cameraMenu = m_cameraStreamsMenu->addMenu(
            QString::fromStdString(stream->info.name.empty() ? stream->info.attachmentId : stream->info.name));
        QAction* run = cameraMenu->addAction(QStringLiteral("Run"));
        run->setCheckable(true);
        run->setChecked(stream->running);
        run->setEnabled(stream->info.enabled);
        connect(run, &QAction::toggled, this, [this, id](bool checked) {
            setCameraStreamRunning(id, checked);
        });
        QAction* show = cameraMenu->addAction(QStringLiteral("Show preview"));
        show->setObjectName(QStringLiteral("cameraPreviewVisibilityAction"));
        show->setData(id);
        show->setCheckable(true);
        show->setChecked(stream->visible);
        connect(show, &QAction::toggled, this, [this, id](bool checked) {
            setCameraPreviewVisible(id, checked);
        });
        QAction* lock = cameraMenu->addAction(QStringLiteral("Lock position"));
        lock->setCheckable(true);
        lock->setChecked(stream->preview->positionLocked());
        CameraPreviewWidget* preview = stream->preview;
        connect(lock, &QAction::toggled, preview, &CameraPreviewWidget::setPositionLocked);
        QAction* resetPosition = cameraMenu->addAction(QStringLiteral("Reset position"));
        resetPosition->setObjectName(QStringLiteral("cameraPreviewResetPositionAction"));
        resetPosition->setData(id);
        connect(resetPosition, &QAction::triggered, this, [this, id]() {
            RobotViewportCameraStreamState* target = cameraStream(id);
            if(target == nullptr) {
                return;
            }
            const auto it = std::find_if(m_cameraStreams.begin(), m_cameraStreams.end(),
                [&](const std::unique_ptr<RobotViewportCameraStreamState>& stream) {
                    return stream.get() == target;
                });
            if(it != m_cameraStreams.end()) {
                layoutNewCameraPreview(*target, static_cast<int>(std::distance(m_cameraStreams.begin(), it)));
            }
        });
        QAction* resetSize = cameraMenu->addAction(QStringLiteral("Reset size"));
        resetSize->setObjectName(QStringLiteral("cameraPreviewResetSizeAction"));
        resetSize->setData(id);
        connect(resetSize, &QAction::triggered, preview, &CameraPreviewWidget::resetPreviewSize);
        if(!stream->lastError.isEmpty()) {
            QAction* retry = cameraMenu->addAction(QStringLiteral("Retry stream"));
            retry->setObjectName(QStringLiteral("cameraPreviewRetryAction"));
            retry->setData(id);
            connect(retry, &QAction::triggered, this, [this, id]() {
                RobotViewportCameraStreamState* target = cameraStream(id);
                if(target != nullptr) {
                    target->lastError.clear();
                    target->nextFrame = Clock::time_point();
                    setCameraStreamRunning(id, true);
                }
            });
        }
    }
}

double RobotViewport::renderDueCameraStreams()
{
    if(m_scene == nullptr || m_cameraStreams.empty()) {
        return 0.0;
    }
    const auto start = Clock::now();
    const auto now = Clock::now();
    const std::size_t count = m_cameraStreams.size();
    int rendered = 0;
    for(const auto& stream : m_cameraStreams) {
        if(!stream->info.enabled) {
            stream->preview->setStatus(CameraPreviewStatus::Disabled);
        } else if(!stream->running) {
            stream->preview->setStatus(stream->lastError.isEmpty()
                ? CameraPreviewStatus::Paused
                : CameraPreviewStatus::Error, stream->lastError);
        } else if(!stream->visible) {
            stream->preview->setStatus(CameraPreviewStatus::Suspended,
                QStringLiteral("Preview is hidden; rendering is suspended"));
        } else if(stream->lastFrame != Clock::time_point() &&
            now - stream->lastFrame > std::chrono::seconds(2)) {
            stream->preview->setStatus(CameraPreviewStatus::Stale,
                QStringLiteral("No camera frame received for more than 2 seconds"));
        }
    }
    for(std::size_t offset = 0; offset < count && rendered < 2; ++offset) {
        const std::size_t index = (m_cameraRoundRobinIndex + offset) % count;
        RobotViewportCameraStreamState& stream = *m_cameraStreams[index];
        if(!stream.running || !stream.visible || !stream.info.enabled ||
            (stream.nextFrame != Clock::time_point() && now < stream.nextFrame)) {
            continue;
        }

        const QSize renderSize = stream.preview->requestedRenderSize(stream.info.width, stream.info.height);
        std::vector<unsigned char> pixels;
        std::string error;
        if(m_scene->renderCameraFrame(
            stream.info.attachmentId,
            renderSize.width(),
            renderSize.height(),
            pixels,
            &error)) {
            QImage image(
                pixels.data(),
                renderSize.width(),
                renderSize.height(),
                QImage::Format_RGBA8888);
            stream.preview->setFrameImage(image.mirrored(false, true).copy());
            stream.lastFrame = now;
            stream.lastError.clear();
        } else {
            stream.running = false;
            stream.lastError = QString::fromStdString(error);
            stream.preview->setStreamRunning(false);
            stream.preview->setStatus(CameraPreviewStatus::Error, stream.lastError);
            LOG_ERROR("rs2026") << "Camera stream stopped: id="
                << stream.info.attachmentId << ", error=" << error;
        }
        const int frameIntervalMs = std::max(1, 1000 / std::max(1, stream.targetFps));
        stream.nextFrame = now + std::chrono::milliseconds(frameIntervalMs);
        m_cameraRoundRobinIndex = (index + 1) % count;
        ++rendered;
    }
    return elapsedMilliseconds(start);
}

void RobotViewport::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
    m_mousePressPos = event->pos();
}

void RobotViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if(m_scene == nullptr || event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint delta = event->pos() - m_mousePressPos;
    if(delta.manhattanLength() > 3) {
        return;
    }

    const ProjectScenePickResult result = m_scene->pickScreenPoint(event->pos().x(), event->pos().y());
    if(!result.valid()) {
        return;
    }

    switch(result.kind) {
    case ProjectScenePickTargetKind::Robot:
        selectRobotLink(toQString(result.robotId), QString());
        break;
    case ProjectScenePickTargetKind::RobotLink:
        selectRobotLink(toQString(result.robotId), toQString(result.linkName));
        break;
    case ProjectScenePickTargetKind::RobotMount:
        selectRobotMount(
            toQString(result.robotId),
            toQString(result.linkName),
            toQString(result.robotMountId));
        setActivePreviewRobotMount(toQString(result.robotMountId));
        break;
    case ProjectScenePickTargetKind::MountedAttachment:
        selectMountedAttachment(toQString(result.mountedAttachmentId));
        setActiveMountedAttachment(toQString(result.mountedAttachmentId));
        break;
    case ProjectScenePickTargetKind::SceneObject:
        selectSceneObject(toQString(result.sceneObjectId));
        break;
    case ProjectScenePickTargetKind::PointCloud:
        selectSceneObject(toQString(result.sceneObjectId));
        break;
    case ProjectScenePickTargetKind::None:
        return;
    }

    emit scenePicked(
        pickKindName(result.kind),
        toQString(result.robotId),
        toQString(result.linkName),
        toQString(result.robotMountId),
        toQString(result.mountedAttachmentId),
        toQString(result.sceneObjectId));
    update();
}

void RobotViewport::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(m_scene != nullptr && event->button() == Qt::LeftButton) {
        const ProjectScenePickResult result =
            m_scene->pickScreenPoint(event->pos().x(), event->pos().y());
        if(!result.valid()) {
            emit backgroundDoubleClicked();
            event->accept();
            return;
        }
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void RobotViewport::mouseMoveEvent(QMouseEvent* event)
{
    if(m_scene == nullptr || m_lastMousePos.isNull()) {
        m_lastMousePos = event->pos();
        return;
    }

    const QPoint delta = event->pos() - m_lastMousePos;
    int button = -1;
    if(event->buttons() & Qt::LeftButton) {
        button = 0;
    } else if(event->buttons() & Qt::RightButton) {
        button = 1;
    }

    if(button >= 0) {
        m_cameraDragFramePending = true;
        if(m_surfaceScalarProbeEnabled) {
            emit surfaceScalarHovered(
                m_surfaceScalarProbeObjectId,
                0.0,
                event->pos(),
                false);
        }
        m_scene->onMouseMove(static_cast<float>(delta.x()) * 0.8f,
            static_cast<float>(-delta.y()) * 0.8f,
            button);
        update();
    } else if(m_surfaceScalarProbeEnabled && !m_surfaceScalarProbeObjectId.isEmpty()) {
        const Clock::time_point now = Clock::now();
        if(now - m_lastSurfaceScalarProbeTime >= std::chrono::milliseconds(33)) {
            m_lastSurfaceScalarProbeTime = now;
            const smrobot::visualization::SurfaceScalarProbeResult result =
                m_scene->probeSurfaceScalarAtScreenPoint(
                    m_surfaceScalarProbeObjectId.toStdString(),
                    event->pos().x(),
                    event->pos().y());
            emit surfaceScalarHovered(
                m_surfaceScalarProbeObjectId,
                result.value,
                event->pos(),
                result.hit);
        }
    }

    m_lastMousePos = event->pos();
}

void RobotViewport::leaveEvent(QEvent* event)
{
    if(m_surfaceScalarProbeEnabled) {
        emit surfaceScalarHovered(
            m_surfaceScalarProbeObjectId,
            0.0,
            QPoint(),
            false);
    }
    QOpenGLWidget::leaveEvent(event);
}

void RobotViewport::wheelEvent(QWheelEvent* event)
{
    if(m_scene == nullptr) {
        return;
    }

    const float delta = static_cast<float>(event->angleDelta().y()) / 120.0f;
    m_scene->onScroll(delta * 0.8f);
    update();
}

void RobotViewport::keyPressEvent(QKeyEvent* event)
{
    if(m_scene == nullptr) {
        QOpenGLWidget::keyPressEvent(event);
        return;
    }

    if(event->key() == Qt::Key_T && !event->isAutoRepeat()) {
        if(!modeAllowsMountedAttachmentSelection(m_interactionMode)) {
            QOpenGLWidget::keyPressEvent(event);
            return;
        }
        const bool reverse = (event->modifiers() & Qt::ShiftModifier) != 0;
        if(m_scene->cycleToolAttachment(reverse)) {
            update();
            return;
        }
    }

    QOpenGLWidget::keyPressEvent(event);
}
