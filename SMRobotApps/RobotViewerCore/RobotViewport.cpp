#include <QMenu>
#include <QContextMenuEvent>
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <cmath>

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

    connect(this, &QOpenGLWidget::frameSwapped, this, [this]() {
        m_presentedFrameTicket = m_renderedFrameTicket;
    });
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
    ProjectSceneEnvironmentSelection selection;
    selection.preset = preset;
    setEnvironmentSelection(selection);
}

bool RobotViewport::setEnvironmentSelection(const ProjectSceneEnvironmentSelection& selection, std::string* error)
{
    std::string detail;
    if(!validateProjectSceneEnvironment(selection, detail)) {
        if(error) *error = detail;
        return false;
    }
    if(m_scene && m_scene->isInitialized() && context() && isValid()) {
        makeCurrent();
        CurrentContextGuard guard(*this);
        if(!m_scene->setEnvironmentSelection(selection, &detail)) {
            if(error) *error = detail;
            return false;
        }
    }
    m_environmentSelection = selection;
    m_environmentPreset = selection.preset;
    if(environmentSceneInitialized()) emit environmentCommitted();
    if(error) error->clear();
    update();
    return true;
}

bool RobotViewport::environmentSceneInitialized() const
{
    return m_scene && m_scene->isInitialized() && context() && isValid();
}

ProjectSceneEnvironmentSelection RobotViewport::environmentSelection() const
{
    return m_environmentSelection;
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
    m_environmentSelection = ProjectSceneEnvironmentSelection{};
    m_environmentSelection.preset = ProjectSceneEnvironmentPreset::Studio;
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

bool RobotViewport::setRobotJointValues(const QString& robotId,
    const std::vector<std::string>& jointNames, const std::vector<double>& values)
{
    if(m_scene == nullptr || !m_scene->setRobotJointValues(robotId.toStdString(), jointNames, values)) {
        return false;
    }
    // Explicit joint playback owns the displayed robot. A pending agent preview
    // or demonstration motion must not paint another pose over its joint updates.
    m_scene->setRobotAutoMotion(robotId.toStdString(),false,0.0,1.0);
    if(m_sdkPreviewRobot==robotId.toStdString())m_sdkPreviewRobot.clear();
    m_manualJointControlExpiry=Clock::now()+std::chrono::seconds(1);
    m_manualJointControlRobot=robotId.toStdString();
    update();
    return true;
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

simulation_project::ProjectDocument RobotViewport::runtimeDocumentSnapshot() const
{
    return m_scene ? m_scene->runtimeDocumentSnapshot() : m_pendingProjectDocument;
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
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
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

bool RobotViewport::applyUserCoordinateFrameDocument(const simulation_project::ProjectDocument& document)
{
    bool success=false;
    if(m_scene) {
        makeCurrent();
        CurrentContextGuard contextGuard(*this);
        success=m_scene->applyUserCoordinateFrameDocument(document);
    }
    update();
    repaint();
    return success;
}

bool RobotViewport::previewSprayDirection(const QJsonObject& trajectory, QString& error)
{
    if(!m_scene){error="Scene unavailable";return false;}
    const auto points=trajectory.value("points").toArray();
    if(points.isEmpty()){m_scene->setSprayDirectionArrow({}, {},false);update();return true;}
    const auto point=points.first().toObject(),spray=point.value("spray").toObject();
    if(spray.isEmpty()){m_scene->setSprayDirectionArrow({}, {},false);update();return true;}
    const auto xyz=point.value("position_m").toArray(),target=spray.value("target_mm").toArray();
    if(xyz.size()!=3 || target.size()!=3){error="Invalid spray direction preview";return false;}
    Eigen::Vector3d a,b;
    for(int i=0;i<3;++i){
        if(!xyz[i].isDouble() || !target[i].isDouble() || !std::isfinite(xyz[i].toDouble()) || !std::isfinite(target[i].toDouble())){error="Invalid spray arrow";return false;}
        a[i]=xyz[i].toDouble();b[i]=target[i].toDouble()/1000;
    }
    if((b-a).norm()<1e-9) {
        const auto q=point.value("quaternion_xyzw").toArray(),axis=spray.value("axis_tool").toArray();
        if(q.size()!=4 || axis.size()!=3){error="Zero-distance preview requires quaternion and tool axis";return false;}
        Eigen::Quaterniond rotation(q[3].toDouble(),q[0].toDouble(),q[1].toDouble(),q[2].toDouble());
        Eigen::Vector3d direction(axis[0].toDouble(),axis[1].toDouble(),axis[2].toDouble());
        if(!rotation.coeffs().allFinite() || std::abs(rotation.norm()-1)>1e-5 || !direction.allFinite() || std::abs(direction.norm()-1)>1e-5){error="Invalid zero-distance direction";return false;}
        // A 150 mm direction glyph; never used as a physical spray distance.
        b=a+0.15*(rotation*direction);
    }
    const auto frame=trajectory.value("frame").toString();
    if(frame!="user" && frame!="world"){error="Invalid spray preview frame";return false;}
    if(frame=="user") {
        Eigen::Isometry3d world=Eigen::Isometry3d::Identity();
        for(const auto& ext:runtimeDocumentSnapshot().extensions)if(ext.key=="rws.user_frame") {
            const auto o=QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object();
            world.translation()=Eigen::Vector3d(o.value("x").toDouble(),o.value("y").toDouble(),o.value("z").toDouble());
            world.linear()=(Eigen::AngleAxisd(o.value("yaw").toDouble(),Eigen::Vector3d::UnitZ())*
                Eigen::AngleAxisd(o.value("pitch").toDouble(),Eigen::Vector3d::UnitY())*
                Eigen::AngleAxisd(o.value("roll").toDouble(),Eigen::Vector3d::UnitX())).toRotationMatrix();
        }
        a=world*a;b=world*b;
    }
    m_scene->setSprayDirectionArrow(a,b,true);update();return true;
}

bool RobotViewport::renderTrajectoryRecord(const QJsonObject& trajectory, QString& error)
{
    if(!m_scene) { error="Scene unavailable"; return false; }
    const auto points=trajectory.value("points").toArray();
    const auto frame=trajectory.value("frame").toString();
    if(points.isEmpty() || (frame!="world" && frame!="user")) {error="Invalid trajectory frame or points";return false;}
    Eigen::Isometry3d world=Eigen::Isometry3d::Identity();
    if(frame=="user") for(const auto& ext:runtimeDocumentSnapshot().extensions) if(ext.key=="rws.user_frame") {
        const auto o=QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object();
        world.translation()=Eigen::Vector3d(o.value("x").toDouble(),o.value("y").toDouble(),o.value("z").toDouble());
        world.linear()=(Eigen::AngleAxisd(o.value("yaw").toDouble(),Eigen::Vector3d::UnitZ())*
            Eigen::AngleAxisd(o.value("pitch").toDouble(),Eigen::Vector3d::UnitY())*
            Eigen::AngleAxisd(o.value("roll").toDouble(),Eigen::Vector3d::UnitX())).toRotationMatrix();
    }
    std::vector<ProjectScene::CoatingTrajectoryPreviewPoint> preview;
    std::vector<Eigen::Vector3d> inputPoints,surfaceTargets;
    for(const auto& value:points) {
        const auto p=value.toObject(); const auto xyz=p.value("position_m").toArray(),q=p.value("quaternion_xyzw").toArray();
        if(xyz.size()!=3 || q.size()!=4) {error="Invalid trajectory pose";return false;}
        for(const auto& v:xyz) if(!v.isDouble() || !std::isfinite(v.toDouble())) {error="Invalid position";return false;}
        for(const auto& v:q) if(!v.isDouble() || !std::isfinite(v.toDouble())) {error="Invalid rotation";return false;}
        Eigen::Quaterniond rotation(q[3].toDouble(),q[0].toDouble(),q[1].toDouble(),q[2].toDouble());
        if(std::abs(rotation.norm()-1)>1e-5) {error="Non-unit trajectory quaternion";return false;}
        const Eigen::Vector3d position=world*Eigen::Vector3d(xyz[0].toDouble(),xyz[1].toDouble(),xyz[2].toDouble());
        const Eigen::Matrix3d axes=world.linear()*rotation.toRotationMatrix();
        const auto spray=p.value("spray").toObject();
        if(!spray.isEmpty()) {
            auto input=spray.value("source_position_mm").toArray();
            if(input.isEmpty())input=spray.value("original_tcp_position_mm").toArray();
            const auto target=spray.value("target_mm").toArray();
            if(input.size()!=3 || target.size()!=3){error="Invalid spray reference points";return false;}
            Eigen::Vector3d inputPoint,targetPoint;
            for(int j=0;j<3;++j){
                if(!input[j].isDouble() || !target[j].isDouble() || !std::isfinite(input[j].toDouble()) || !std::isfinite(target[j].toDouble())){error="Invalid spray reference values";return false;}
                inputPoint[j]=input[j].toDouble()/1000;targetPoint[j]=target[j].toDouble()/1000;
            }
            inputPoints.push_back(world*inputPoint);
            if(p.value("point_type").toInt()!=2)surfaceTargets.push_back(world*targetPoint);
        }
        ProjectScene::CoatingTrajectoryPreviewPoint point;
        point.positionX=position.x();point.positionY=position.y();point.positionZ=position.z();
        point.frameXAxisX=axes(0,0);point.frameXAxisY=axes(1,0);point.frameXAxisZ=axes(2,0);
        point.frameYAxisX=axes(0,1);point.frameYAxisY=axes(1,1);point.frameYAxisZ=axes(2,1);
        point.frameZAxisX=axes(0,2);point.frameZAxisY=axes(1,2);point.frameZAxisZ=axes(2,2);
        point.directionX=axes(0,2);point.directionY=axes(1,2);point.directionZ=axes(2,2);preview.push_back(point);
    }
    m_scene->setCoatingTrajectoryPreview(preview,true,true,false);
    m_scene->setSprayReferencePaths(inputPoints,surfaceTargets);
    return previewSprayDirection(trajectory,error);
}

bool RobotViewport::loadTrajectoryRecord(const QJsonObject& input, QString& error)
{
    // Validate before replacing the current preview; records are embedded in the project.
    if(!renderTrajectoryRecord(input,error)) return false;
    auto trajectory=input;
    QString id=trajectory.value("library_id").toString();
    if(id.isEmpty()) id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    trajectory["library_id"]=id;
    auto document=runtimeDocumentSnapshot();
    const auto upsert=[&](const std::string& key,const QJsonObject& value) {
        simulation_project::ProjectExtensionDesc ext;ext.key=key;ext.version=1;
        ext.serializedPayload=QJsonDocument(value).toJson(QJsonDocument::Compact).toStdString();
        const auto it=std::find_if(document.extensions.begin(),document.extensions.end(),[&](const auto& e){return e.key==key;});
        if(it==document.extensions.end()) document.extensions.push_back(ext);else *it=ext;
    };
    upsert("rws.trajectory."+id.toStdString(),trajectory);
    upsert("rws.active_trajectory",QJsonObject{{"id",id}});
    if(!applyUserCoordinateFrameDocument(document)) {error="Unable to retain trajectory";return false;}
    return true;
}

bool RobotViewport::showStoredTrajectory(const QString& id, QString& error)
{
    for(const auto& ext:runtimeDocumentSnapshot().extensions) if(ext.key=="rws.trajectory."+id.toStdString())
        return loadTrajectoryRecord(QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object(),error);
    error="Trajectory not found";return false;
}

bool RobotViewport::deleteStoredTrajectory(const QString& id, QString& error)
{
    if(id.isEmpty()) {error="Trajectory ID required";return false;}
    auto document=runtimeDocumentSnapshot();
    const auto key="rws.trajectory."+id.toStdString();
    const auto record=std::find_if(document.extensions.begin(),document.extensions.end(),
        [&](const auto& ext){return ext.key==key;});
    if(record==document.extensions.end()) {error="Trajectory not found";return false;}
    document.extensions.erase(record);
    bool active=false;
    document.extensions.erase(std::remove_if(document.extensions.begin(),document.extensions.end(),[&](const auto& ext){
        if(ext.key!="rws.active_trajectory") return false;
        const bool matches=QJsonDocument::fromJson(QByteArray::fromStdString(ext.serializedPayload)).object().value("id").toString()==id;
        active=active || matches;return matches;
    }),document.extensions.end());
    if(!applyUserCoordinateFrameDocument(document)) {error="Unable to delete trajectory";return false;}
    if(active && m_scene) {m_scene->setCoatingTrajectoryPreview({},false,false,false);m_scene->setSprayDirectionArrow({}, {},false);}
    update();return true;
}

bool RobotViewport::rebuildCollisionDetectorsFromDocument(const simulation_project::ProjectDocument& document)
{
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
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

bool RobotViewport::robotLinkTransform(const std::string& robotId, const std::string& linkName,
    simulation_project::TransformDesc& transform) const
{
    return m_scene && m_scene->robotLinkTransform(robotId, linkName, transform);
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
    QString* errorMessage,
    bool blackOutsideRange)
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
        ok = m_scene->applySurfaceScalarOverlay(
            overlay, &error, blackOutsideRange);
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
    const std::vector<simulation_project::TransformDesc>& controlPoints, bool showPoints, bool showOrientations)
{
    if(m_scene != nullptr) {
        m_scene->setTrajectoryControlPointOverlay(trajectoryId.toStdString(), controlPoints, showPoints, showOrientations);
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

bool RobotViewport::setSceneObjectVisibilityOverride(const QString& objectId, bool visible)
{
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return false;
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    const bool changed = m_scene->setSceneObjectVisibilityOverride(objectId.toStdString(), visible);
    if(changed) {
        update();
    }
    return changed;
}

bool RobotViewport::clearSceneObjectVisibilityOverride(const QString& objectId)
{
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return false;
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    const bool changed = m_scene->clearSceneObjectVisibilityOverride(objectId.toStdString());
    if(changed) {
        update();
    }
    return changed;
}

void RobotViewport::clearSceneObjectVisibilityOverrides()
{
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        return;
    }
    makeCurrent();
    CurrentContextGuard contextGuard(*this);
    m_scene->clearSceneObjectVisibilityOverrides();
    update();
}

bool RobotViewport::sceneObjectVisible(const QString& objectId, bool& visible) const
{
    return m_scene != nullptr && m_scene->sceneObjectVisible(objectId.toStdString(), visible);
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
        if(ok) {
            std::string environmentError;
            if(!m_scene->setEnvironmentSelection(m_environmentSelection, &environmentError)) {
                m_lastError = QString::fromUtf8(environmentError.c_str());
                m_environmentSelection = ProjectSceneEnvironmentSelection{};
                m_environmentPreset = m_environmentSelection.preset;
                m_scene->setEnvironmentSelection(m_environmentSelection);
                LOG_WARNING("rs2026") << "Environment restore failed: " << environmentError;
                emit environmentRestoreFailed(m_lastError);
            }
        }
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
        const auto document=runtimeDocumentSnapshot();
        QString active;
        for(const auto& e:document.extensions) if(e.key=="rws.active_trajectory")
            active=QJsonDocument::fromJson(QByteArray::fromStdString(e.serializedPayload)).object().value("id").toString();
        for(const auto& e:document.extensions) if(!active.isEmpty() && e.key=="rws.trajectory."+active.toStdString()) {
            QString error;renderTrajectoryRecord(QJsonDocument::fromJson(QByteArray::fromStdString(e.serializedPayload)).object(),error);
        }
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
    if(ok) {
        emit environmentCommitted();
        emit environmentSceneReady();
    }
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

quint64 RobotViewport::requestFramePresentation()
{
    const quint64 ticket = ++m_requestedFrameTicket;
    update();
    return ticket;
}

bool RobotViewport::isFramePresented(quint64 ticket) const
{
    return ticket <= m_presentedFrameTicket;
}

void RobotViewport::paintGL()
{
    if(m_scene == nullptr || !m_scene->isInitialized()) {
        return;
    }

    // Capture before any callbacks: a repaint requested during this paint must
    // not be acknowledged by the frame that was already being rendered.
    const quint64 frameTicket = m_requestedFrameTicket;
    const auto frameStart = Clock::now();
    const auto now = Clock::now();
    const std::chrono::duration<double> elapsed = now - m_startTime;
    const auto updateStart = Clock::now();
    const bool previewRequested=!m_sdkPreviewRobot.empty() && Clock::now()<m_sdkPreviewExpiry;
    m_scene->update(elapsed.count(),!previewRequested);
    emit robotStateUpdated();
    const double updateMs = elapsedMilliseconds(updateStart);
    const auto renderStart = Clock::now();
    // Preview exists only inside this synchronous GUI paint. SDK snapshots,
    // saved projects and planner seeds always observe the original joint state.
    std::vector<double> previewOriginal;
    bool previewValid=previewRequested;
    if(previewValid) for(const auto& name:m_sdkPreviewNames) {
        double value=0;
        if(!m_scene->robotJointValue(m_sdkPreviewRobot,name,value)) {previewValid=false;break;}
        previewOriginal.push_back(value);
    }
    if(previewValid) previewValid=m_scene->setRobotJointValues(m_sdkPreviewRobot,m_sdkPreviewNames,m_sdkPreviewValues);
    try {
        if(previewRequested) m_scene->refreshCollisionPresentation();
        m_scene->render();
    }
    catch(...) {
        if(previewValid) {
            m_scene->setRobotJointValues(m_sdkPreviewRobot,m_sdkPreviewNames,previewOriginal);
            m_scene->refreshCollisionPresentation(false);
        }
        throw;
    }
    if(previewValid) {
        m_scene->setRobotJointValues(m_sdkPreviewRobot,m_sdkPreviewNames,previewOriginal);
        m_scene->refreshCollisionPresentation(false);
    }
    m_renderedFrameTicket = frameTicket;
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
    m_cameraDraggedSincePress = false;
}

void RobotViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if(m_scene == nullptr || event->button() != Qt::LeftButton) {
        return;
    }

    const QPoint delta = event->pos() - m_mousePressPos;
    if(m_cameraDraggedSincePress || delta.manhattanLength() > 3) {
        return;
    }

    if(m_coatingSurfacePick) {
        const auto hit=m_scene->pickTriangleScreenPoint(event->pos().x(),event->pos().y());
        if(hit.valid()) {
            m_coatingSurfacePick=false; unsetCursor();
            emit coatingSurfacePicked(QString::fromStdString(hit.sceneObjectId),hit.triangleIndex,hit.hitPosition.x(),hit.hitPosition.y(),hit.hitPosition.z(),hit.normal.x(),hit.normal.y(),hit.normal.z());
        }
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
        // A move that changes the camera is navigation, even if the pointer
        // returns to the press position before release. Do not pick a workpiece.
        if(!delta.isNull()) {
            m_cameraDraggedSincePress = true;
        }
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
            emit coatingSurfaceScalarHovered(m_surfaceScalarProbeObjectId,result.value,result.worldPosition.x(),result.worldPosition.y(),result.worldPosition.z(),event->pos(),result.hit);
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

bool RobotViewport::applySurfaceScalarOverlayModel(
    const smrobot::visualization::SurfaceScalarOverlay& overlay,
    const assetcore::ModelDesc& model,
    QString* errorMessage,
    bool blackOutsideRange)
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
        ok = m_scene->applySurfaceScalarOverlayModel(
            overlay, model, &error, blackOutsideRange);
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

bool RobotViewport::setCoatingPredictionModel(
    const QString& objectId,
    const assetcore::ModelDesc& model,
    QString* errorMessage)
{
    if(m_scene == nullptr || context() == nullptr || !isValid()) {
        if(errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Viewport OpenGL context is not available.");
        }
        return false;
    }
    std::string error;
    makeCurrent();
    const bool ok = m_scene->setCoatingPredictionModel(objectId.toStdString(), model, &error);
    doneCurrent();
    if(errorMessage != nullptr) {
        *errorMessage = QString::fromStdString(error);
    }
    if(ok) {
        update();
    }
    return ok;
}

bool RobotViewport::clearCoatingPredictionModel(const QString& objectId)
{
    if(m_scene == nullptr) {
        return false;
    }
    const bool ok = m_scene->clearCoatingPredictionModel(objectId.toStdString());
    if(ok) {
        update();
    }
    return ok;
}

void RobotViewport::setCoatingTrajectoryPreview(
    const std::vector<ProjectScene::CoatingTrajectoryPreviewPoint>& points,
    bool visible,
    bool showTrajectoryLines,
    bool showSprayPointFrames)
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->setCoatingTrajectoryPreview(
        points,
        visible,
        showTrajectoryLines,
        showSprayPointFrames);
    update();
}

bool RobotViewport::setCoatingTrajectoryPreviewVisible(
    bool visible,
    bool showTrajectoryLines,
    bool showSprayPointFrames)
{
    if(m_scene == nullptr) {
        return false;
    }
    const bool ok = m_scene->setCoatingTrajectoryPreviewVisible(
        visible,
        showTrajectoryLines,
        showSprayPointFrames);
    update();
    return ok;
}

void RobotViewport::setCoatingPredictionDebugState(
    const ProjectScene::CoatingPredictionDebugState& state)
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->setCoatingPredictionDebugState(state);
    update();
}

void RobotViewport::setCoatingPredictionDebugVisibility(
    const ProjectScene::CoatingPredictionDebugVisibility& visibility)
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->setCoatingPredictionDebugVisibility(visibility);
    update();
}

void RobotViewport::clearCoatingPredictionDebugState()
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->clearCoatingPredictionDebugState();
    update();
}

bool RobotViewport::setCoatingModelVisible(const QString& objectId, bool visible)
{
    if(m_scene == nullptr) {
        return false;
    }
    const bool ok = m_scene->setCoatingModelVisible(objectId.toStdString(), visible);
    if(ok) {
        update();
    }
    return ok;
}

void RobotViewport::setCoatingModelVisibilities(
    const std::unordered_map<std::string, bool>& visibility)
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->setCoatingModelVisibilities(visibility);
    update();
}

void RobotViewport::clearCoatingModelVisibility(const QString& objectId)
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->clearCoatingModelVisibility(objectId.toStdString());
    update();
}

void RobotViewport::setCoatingAnalysisView(bool active)
{
    if(!active) { m_coatingSurfacePick=false; unsetCursor(); }


    if(m_scene == nullptr) {
        return;
    }
    m_scene->setCoatingAnalysisView(active);
    update();
}

void RobotViewport::focusCoatingObject(const QString& objectId, double duration)
{
    if(m_scene == nullptr) {
        return;
    }
    m_scene->focusCoatingObject(objectId.toStdString(), duration);
    update();
}

void RobotViewport::beginCoatingSurfacePick() { m_coatingSurfacePick=true; setCursor(Qt::CrossCursor); }
void RobotViewport::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this); emit coatingContextMenuRequested(&menu);
    if(!menu.isEmpty()) menu.exec(event->globalPos());
}

void RobotViewport::setTrajectoryHighlightRange(int first,int last) { if(m_scene) m_scene->setTrajectoryHighlightRange(first,last); update(); }
