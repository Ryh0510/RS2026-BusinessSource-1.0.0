#pragma once

#include <filesystem>
#include <functional>
#include <Eigen/Geometry>
#include <vector>

#include <QString>
#include <QStringList>

#include <SimulationProject/ProjectDocument.h>
#include <VisualizationSDK/CustomMesh.h>
#include <VisualizationSDK/SurfaceScalarOverlay.h>

#include "CollisionRuntimeViewModel.h"
#include "RobotQtViewerEvents.h"

namespace robot_qt_viewer
{
    struct SprayMeasurementResult
    {
        bool valid = false;
        double distanceMeters = 0.0;
        double angleDegrees = 0.0;
        QString errorMessage;
    };

    // Transient planning visualization and access to the actual model/TCP.
    // Trajectory and optimization state remain in the planning domain.
    class IRobotQtViewerMotionPlanningViewportPort
    {
    public:
        virtual ~IRobotQtViewerMotionPlanningViewportPort() = default;
        using RobotForwardKinematics = std::function<Eigen::Isometry3d(const std::vector<double>&)>;
        virtual RobotForwardKinematics robotForwardKinematics(const QString& robotId,
            const std::vector<std::string>& jointNames, bool includeTool) const = 0;
        virtual void setRobotJointValue(const QString& robotId, const QString& jointName, double value) = 0;
        virtual bool setRobotJointValues(const QString& robotId,
            const std::vector<std::string>& jointNames, const std::vector<double>& values) = 0;
        // A frame ticket acknowledges composition/swap, not merely a queued repaint.
        // Hidden/suspended viewports leave it pending until presentation resumes.
        virtual quint64 requestFramePresentation() = 0;
        virtual bool isFramePresented(quint64 ticket) const = 0;
        virtual double robotJointValue(const QString& robotId, const QString& jointName, bool* ok = nullptr) const = 0;
        virtual void setSprayRangeVisible(const QString& robotId, bool visible) = 0;
        virtual void setEndEffectorTraceVisible(const QString& robotId, bool visible) = 0;
        virtual void clearEndEffectorTrace() = 0;
        virtual void appendEndEffectorTraceSample() = 0;
        virtual SprayMeasurementResult sprayMeasurement(const QString& robotId) const = 0;
        virtual void setTrajectoryControlPointOverlay(const QString& trajectoryId,
            const std::vector<simulation_project::TransformDesc>& points, bool showPoints = true) = 0;
        virtual void clearTrajectoryControlPointOverlay(const QString& trajectoryId = QString()) = 0;
    };

    struct RobotQtViewerViewportLoadResult
    {
        bool success = false;
        QString errorMessage;
    };

    class IRobotQtViewerDocumentViewportPort
    {
    public:
        virtual ~IRobotQtViewerDocumentViewportPort() = default;

        virtual RobotQtViewerViewportLoadResult loadProjectDocument(
            const simulation_project::ProjectDocument& document,
            const std::filesystem::path& basePath) = 0;
    };

    class IRobotQtViewerSelectionViewportPort
    {
    public:
        virtual ~IRobotQtViewerSelectionViewportPort() = default;

        virtual void selectRobotMount(
            const QString& robotId,
            const QString& linkName,
            const QString& robotMountId) = 0;
        virtual void selectObjectFrame(const QString& objectId, const QString& frameId) = 0;
        virtual void selectRobotLink(const QString& robotId, const QString& linkName) = 0;
        virtual void selectRobotJointFrame(const QString& robotId, const QString& jointName) = 0;
        virtual void setActiveToolFrameRobot(const QString& robotId) = 0;
        virtual void selectSceneObject(const QString& objectId) = 0;
        virtual void selectMountedAttachment(const QString& attachmentId) = 0;
        virtual void focusMountFrameLink(const QString& robotId, const QString& linkName) = 0;
        virtual void clearMountFrameLinkFocus() = 0;
        virtual void focusObjectFrameObject(const QString& objectId) = 0;
        virtual void clearObjectFrameObjectFocus() = 0;
        virtual void focusMountedAttachment(const QString& attachmentId) = 0;
        virtual void clearMountedAttachmentFocus() = 0;
    };

    class IRobotQtViewerAssemblyViewportPort
    {
    public:
        virtual ~IRobotQtViewerAssemblyViewportPort() = default;

        virtual bool setActivePreviewRobotMount(const QString& robotMountId) = 0;
        virtual bool previewRobotMountTransform(
            const QString& robotMountId,
            const simulation_project::TransformDesc& transform) = 0;
        virtual bool previewMountedAttachmentTransform(
            const QString& attachmentId,
            const simulation_project::TransformDesc& transform) = 0;
        virtual bool previewAttachmentAsset(
            const simulation_project::AttachmentAssetDesc& asset) = 0;
        virtual bool previewAttachmentBinding(
            const simulation_project::BindFramesRequest& request) = 0;
        virtual void clearAttachmentBindingPreview() = 0;
        virtual bool previewRobotMountLink(
            const QString& robotMountId,
            const QString& linkName) = 0;
        virtual bool upsertPreviewRobotMount(const simulation_project::RobotMountDesc& mount) = 0;
        virtual bool removePreviewRobotMount(const QString& robotMountId) = 0;
        virtual void previewRobotBaseTransform(
            const QString& robotId,
            const simulation_project::TransformDesc& transform) = 0;
        virtual void previewSceneObjectTransform(
            const QString& objectId,
            const simulation_project::TransformDesc& transform) = 0;
        virtual bool commitSceneObjectTransform(
            const QString& objectId,
            const simulation_project::TransformDesc& transform) = 0;
        virtual bool removeSceneObject(const QString& objectId) = 0;
        virtual bool previewObjectFrameTransform(
            const QString& objectId,
            const QString& frameId,
            const simulation_project::TransformDesc& transform) = 0;
        virtual bool upsertPreviewObjectFrame(
            const QString& objectId,
            const simulation_project::ObjectFrameDesc& frame) = 0;
        virtual bool setActiveMountedAttachment(const QString& attachmentId) = 0;
        virtual void setToolFrameVisibility(
            const RobotQtViewerToolFrameVisibility& visibility) = 0;
        virtual void setRobotMountFrameVisibility(
            bool selectedLinkFrameVisible,
            bool mountFrameVisible) = 0;
        virtual void setPinnedRobotMountFrames(const QStringList& robotMountIds) = 0;
    };

    class IRobotQtViewerCollisionViewportPort
    {
    public:
        virtual ~IRobotQtViewerCollisionViewportPort() = default;

        virtual void previewObjectCollisionModelVariant(
            const QString& objectId,
            const QString& variantId) = 0;
        virtual void clearObjectCollisionModelVariantPreview() = 0;
        virtual void previewCollisionPairTargets(
            const QString& robotAId,
            const QString& linkAName,
            const QString& objectAId,
            const QString& attachmentAId,
            const QString& robotBId,
            const QString& linkBName,
            const QString& objectBId,
            const QString& attachmentBId) = 0;
        virtual std::filesystem::path projectBasePath() const = 0;
        virtual bool refreshCollisionConfiguration(
            const simulation_project::ProjectDocument& document,
            const std::filesystem::path& basePath) = 0;
        virtual void setCollisionGeometryVisible(bool visible) = 0;
        virtual bool collisionQueriesEnabled() const = 0;
        virtual bool setCollisionQueriesEnabled(bool enabled) = 0;
        virtual bool setActiveCollisionDetector(const QString& detectorId) = 0;
        virtual bool refreshCollisionDetectorNearest(const QString& detectorId) = 0;
        virtual bool setCollisionDetectorEnabled(const QString& detectorId, bool enabled) = 0;
        virtual bool setCollisionDetectorVisible(const QString& detectorId, bool visible) = 0;
        virtual bool updateCollisionDetectorRuntimeOptions(
            const simulation_project::CollisionDetectorDesc& detector) = 0;
        virtual bool rebuildCollisionDetectorsFromDocument(
            const simulation_project::ProjectDocument& document) = 0;
        virtual bool removeCollisionDetector(const QString& detectorId) = 0;
        virtual bool setVisibleRobotCollisionVariant(
            const QString& robotId,
            const QString& linkName,
            const QString& variantId) = 0;
        virtual QString visibleRobotCollisionVariant(
            const QString& robotId,
            const QString& linkName) const = 0;
        virtual CollisionRuntimeRobotSummary robotCollisionSummary(
            const QString& robotId) const = 0;
        virtual std::vector<CollisionRuntimeDetectorInfo> collisionRuntimeDetectors() const = 0;
        virtual bool generateRobotCollisionProxies(
            const QString& robotId,
            const QString& linkName,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const = 0;
        virtual bool generateRobotCollisionProxiesFromExistingCollision(
            const QString& robotId,
            const QString& linkName,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const = 0;
        virtual bool generateRobotCollisionProxiesFromExistingCollision(
            const QString& robotId,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const = 0;
        virtual bool generateRobotCollisionCoacdFromVisual(
            const QString& robotId,
            const QString& linkName,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const = 0;
        virtual bool generateRobotCollisionCoacdFromExistingCollision(
            const QString& robotId,
            const QString& linkName,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const = 0;
        virtual bool generateObjectCollisionCoacdFromVisual(
            const QString& objectId,
            std::vector<simulation_project::ObjectCollisionElementOverrideDesc>& elements) const = 0;
        virtual bool generateMissingRobotCollisionProxies(
            const QString& robotId,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const = 0;
        virtual bool evaluateRobotCollisionProxyQuality(
            const QString& robotId,
            const QString& linkName,
            const CollisionRuntimeProxyRequest& request,
            const std::vector<simulation_project::CollisionElementOverrideDesc>& elements,
            bool useExistingCollisionInput,
            CollisionRuntimeProxyQualitySummary& summary) const = 0;
    };

    class IRobotQtViewerVisualizationViewportPort :
        public smrobot::visualization::ICustomMeshScene
    {
    public:
        virtual ~IRobotQtViewerVisualizationViewportPort() = default;

        virtual bool applySurfaceScalarOverlay(
            const smrobot::visualization::SurfaceScalarOverlay& overlay,
            QString* errorMessage) = 0;
        virtual bool setSurfaceScalarOverlayVisible(const QString& objectId, bool visible) = 0;
        virtual bool clearSurfaceScalarOverlay(const QString& objectId) = 0;
        virtual void setSurfaceScalarProbeEnabled(bool enabled, const QString& objectId) = 0;
    };
}
