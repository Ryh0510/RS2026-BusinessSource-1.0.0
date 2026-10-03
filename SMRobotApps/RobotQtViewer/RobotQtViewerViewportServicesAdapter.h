#pragma once

#include "RobotQtViewerViewportPorts.h"

#include <RobotRuntime/RobotRunService.h>
#include <RobotRuntime/RobotTrajectoryExecutionSession.h>

class RobotViewport;

namespace robot_qt_viewer
{
    struct RobotQtViewerViewportProjectState
    {
        simulation_project::ProjectDocument document;
        std::filesystem::path basePath;
        bool loaded = false;
        bool attachmentBindingPreviewActive = false;
    };

    class RobotQtViewerMotionPlanningViewportAdapter : public IRobotQtViewerMotionPlanningViewportPort
    {
    public:
        explicit RobotQtViewerMotionPlanningViewportAdapter(RobotViewport& viewport) : m_viewport(viewport) {}
        using RobotForwardKinematics = std::function<Eigen::Isometry3d(const std::vector<double>&)>;
        RobotForwardKinematics robotForwardKinematics(const QString& robotId,
            const std::vector<std::string>& jointNames, bool includeTool) const override;
        void setRobotJointValue(const QString& robotId, const QString& jointName, double value) override;
        bool setRobotJointValues(const QString& robotId,
            const std::vector<std::string>& jointNames, const std::vector<double>& values) override;
        quint64 requestFramePresentation() override;
        bool isFramePresented(quint64 ticket) const override;
        double robotJointValue(const QString& robotId, const QString& jointName, bool* ok = nullptr) const override;
        void setSprayRangeVisible(const QString& robotId, bool visible) override;
        void setEndEffectorTraceVisible(const QString& robotId, bool visible) override;
        void clearEndEffectorTrace() override;
        void appendEndEffectorTraceSample() override;
        SprayMeasurementResult sprayMeasurement(const QString& robotId) const override;
        void setTrajectoryControlPointOverlay(const QString& trajectoryId,
            const std::vector<simulation_project::TransformDesc>& points, bool showPoints = true) override;
        void clearTrajectoryControlPointOverlay(const QString& trajectoryId = QString()) override;
    private:
        RobotViewport& m_viewport;
    };

    class RobotQtViewerDocumentViewportAdapter : public IRobotQtViewerDocumentViewportPort
    {
    public:
        RobotQtViewerDocumentViewportAdapter(
            RobotViewport& viewport,
            RobotQtViewerViewportProjectState& projectState);

        RobotQtViewerViewportLoadResult loadProjectDocument(
            const simulation_project::ProjectDocument& document,
            const std::filesystem::path& basePath) override;

    private:
        RobotViewport& m_viewport;
        RobotQtViewerViewportProjectState& m_projectState;
    };

    class RobotQtViewerSelectionViewportAdapter : public IRobotQtViewerSelectionViewportPort
    {
    public:
        explicit RobotQtViewerSelectionViewportAdapter(RobotViewport& viewport);

        void selectRobotMount(
            const QString& robotId,
            const QString& linkName,
            const QString& robotMountId) override;
        void selectObjectFrame(const QString& objectId, const QString& frameId) override;
        void selectRobotLink(const QString& robotId, const QString& linkName) override;
        void selectRobotJointFrame(const QString& robotId, const QString& jointName) override;
        void setActiveToolFrameRobot(const QString& robotId) override;
        void selectSceneObject(const QString& objectId) override;
        void selectMountedAttachment(const QString& attachmentId) override;
        void focusMountFrameLink(const QString& robotId, const QString& linkName) override;
        void clearMountFrameLinkFocus() override;
        void focusObjectFrameObject(const QString& objectId) override;
        void clearObjectFrameObjectFocus() override;
        void focusMountedAttachment(const QString& attachmentId) override;
        void clearMountedAttachmentFocus() override;

    private:
        RobotViewport& m_viewport;
    };

    class RobotQtViewerAssemblyViewportAdapter : public IRobotQtViewerAssemblyViewportPort
    {
    public:
        RobotQtViewerAssemblyViewportAdapter(
            RobotViewport& viewport,
            RobotQtViewerViewportProjectState& projectState);

        bool setActivePreviewRobotMount(const QString& robotMountId) override;
        bool previewRobotMountTransform(
            const QString& robotMountId,
            const simulation_project::TransformDesc& transform) override;
        bool previewMountedAttachmentTransform(
            const QString& attachmentId,
            const simulation_project::TransformDesc& transform) override;
        bool previewAttachmentAsset(
            const simulation_project::AttachmentAssetDesc& asset) override;
        bool previewAttachmentBinding(
            const simulation_project::BindFramesRequest& request) override;
        void clearAttachmentBindingPreview() override;
        bool previewRobotMountLink(
            const QString& robotMountId,
            const QString& linkName) override;
        bool upsertPreviewRobotMount(const simulation_project::RobotMountDesc& mount) override;
        bool removePreviewRobotMount(const QString& robotMountId) override;
        void previewRobotBaseTransform(
            const QString& robotId,
            const simulation_project::TransformDesc& transform) override;
        void previewSceneObjectTransform(
            const QString& objectId,
            const simulation_project::TransformDesc& transform) override;
        bool commitSceneObjectTransform(
            const QString& objectId,
            const simulation_project::TransformDesc& transform) override;
        bool removeSceneObject(const QString& objectId) override;
        bool previewObjectFrameTransform(
            const QString& objectId,
            const QString& frameId,
            const simulation_project::TransformDesc& transform) override;
        bool upsertPreviewObjectFrame(
            const QString& objectId,
            const simulation_project::ObjectFrameDesc& frame) override;
        bool setActiveMountedAttachment(const QString& attachmentId) override;
        void setToolFrameVisibility(
            const RobotQtViewerToolFrameVisibility& visibility) override;
        void setRobotMountFrameVisibility(
            bool selectedLinkFrameVisible,
            bool mountFrameVisible) override;
        void setPinnedRobotMountFrames(const QStringList& robotMountIds) override;

    private:
        RobotViewport& m_viewport;
        RobotQtViewerViewportProjectState& m_projectState;
    };

    class RobotQtViewerCollisionViewportAdapter : public IRobotQtViewerCollisionViewportPort
    {
    public:
        RobotQtViewerCollisionViewportAdapter(
            RobotViewport& viewport,
            const RobotQtViewerViewportProjectState& projectState);

        void previewObjectCollisionModelVariant(
            const QString& objectId,
            const QString& variantId) override;
        void clearObjectCollisionModelVariantPreview() override;
        void previewCollisionPairTargets(
            const QString& robotAId,
            const QString& linkAName,
            const QString& objectAId,
            const QString& attachmentAId,
            const QString& robotBId,
            const QString& linkBName,
            const QString& objectBId,
            const QString& attachmentBId) override;
        std::filesystem::path projectBasePath() const override;
        bool refreshCollisionConfiguration(
            const simulation_project::ProjectDocument& document,
            const std::filesystem::path& basePath) override;
        void setCollisionGeometryVisible(bool visible) override;
        bool collisionQueriesEnabled() const override;
        bool setCollisionQueriesEnabled(bool enabled) override;
        bool setActiveCollisionDetector(const QString& detectorId) override;
        bool refreshCollisionDetectorNearest(const QString& detectorId) override;
        bool setCollisionDetectorEnabled(const QString& detectorId, bool enabled) override;
        bool setCollisionDetectorVisible(const QString& detectorId, bool visible) override;
        bool updateCollisionDetectorRuntimeOptions(
            const simulation_project::CollisionDetectorDesc& detector) override;
        bool rebuildCollisionDetectorsFromDocument(
            const simulation_project::ProjectDocument& document) override;
        bool removeCollisionDetector(const QString& detectorId) override;
        bool setVisibleRobotCollisionVariant(
            const QString& robotId,
            const QString& linkName,
            const QString& variantId) override;
        QString visibleRobotCollisionVariant(
            const QString& robotId,
            const QString& linkName) const override;
        CollisionRuntimeRobotSummary robotCollisionSummary(
            const QString& robotId) const override;
        std::vector<CollisionRuntimeDetectorInfo> collisionRuntimeDetectors() const override;
        bool generateRobotCollisionProxies(
            const QString& robotId,
            const QString& linkName,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const override;
        bool generateRobotCollisionProxiesFromExistingCollision(
            const QString& robotId,
            const QString& linkName,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const override;
        bool generateRobotCollisionProxiesFromExistingCollision(
            const QString& robotId,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const override;
        bool generateRobotCollisionCoacdFromVisual(
            const QString& robotId,
            const QString& linkName,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const override;
        bool generateRobotCollisionCoacdFromExistingCollision(
            const QString& robotId,
            const QString& linkName,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const override;
        bool generateObjectCollisionCoacdFromVisual(
            const QString& objectId,
            std::vector<simulation_project::ObjectCollisionElementOverrideDesc>& elements) const override;
        bool generateMissingRobotCollisionProxies(
            const QString& robotId,
            const CollisionRuntimeProxyRequest& request,
            std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const override;
        bool evaluateRobotCollisionProxyQuality(
            const QString& robotId,
            const QString& linkName,
            const CollisionRuntimeProxyRequest& request,
            const std::vector<simulation_project::CollisionElementOverrideDesc>& elements,
            bool useExistingCollisionInput,
            CollisionRuntimeProxyQualitySummary& summary) const override;

    private:
        RobotViewport& m_viewport;
        const RobotQtViewerViewportProjectState& m_projectState;
    };

    class RobotQtViewerVisualizationViewportAdapter :
        public IRobotQtViewerVisualizationViewportPort
    {
    public:
        explicit RobotQtViewerVisualizationViewportAdapter(RobotViewport& viewport);

        bool applySurfaceScalarOverlay(
            const smrobot::visualization::SurfaceScalarOverlay& overlay,
            QString* errorMessage) override;
        bool setSurfaceScalarOverlayVisible(const QString& objectId, bool visible) override;
        bool clearSurfaceScalarOverlay(const QString& objectId) override;
        void setSurfaceScalarProbeEnabled(bool enabled, const QString& objectId) override;
        smrobot::visualization::CustomMeshResult upsertCustomMesh(
            const smrobot::visualization::CustomMeshDesc& desc,
            smrobot::visualization::CustomMeshHandle& handle) override;
        smrobot::visualization::CustomMeshResult updateCustomMeshGeometry(
            smrobot::visualization::CustomMeshHandle handle,
            const smrobot::visualization::MeshData& mesh) override;
        smrobot::visualization::CustomMeshResult updateCustomMeshColors(
            smrobot::visualization::CustomMeshHandle handle,
            const std::vector<smrobot::visualization::Color4f>& colors) override;
        smrobot::visualization::CustomMeshResult setCustomMeshTransform(
            smrobot::visualization::CustomMeshHandle handle,
            const smrobot::visualization::TransformMatrix& transform) override;
        smrobot::visualization::CustomMeshResult setCustomMeshAppearance(
            smrobot::visualization::CustomMeshHandle handle,
            const smrobot::visualization::MeshAppearance& appearance) override;
        smrobot::visualization::CustomMeshResult setCustomMeshVisible(
            smrobot::visualization::CustomMeshHandle handle,
            bool visible) override;
        smrobot::visualization::CustomMeshResult removeCustomMesh(
            smrobot::visualization::CustomMeshHandle handle) override;
        smrobot::visualization::CustomMeshResult clearCustomMeshes(
            const std::string& ownerId) override;

    private:
        RobotViewport& m_viewport;
    };

    class RobotQtViewerRobotRunServiceAdapter : public robotruntime::IRobotRunService
    {
    public:
        explicit RobotQtViewerRobotRunServiceAdapter(RobotViewport& viewport);

        bool jointValue(
            const std::string& robotId,
            const std::string& jointName,
            double& value) const override;
        robotruntime::RobotRunCommandResult setJointValue(
            const std::string& robotId,
            const std::string& jointName,
            double value) override;
        robotruntime::RobotRunCommandResult setAutoMotion(
            const std::string& robotId,
            bool enabled,
            double amplitude,
            double speed) override;
        robotruntime::RobotRunCommandResult loadTrajectory(
            const std::string& robotId,
            const std::string& trajectoryId,
            const std::vector<std::string>& jointNames,
            const robottrajectory::JointTrajectory& trajectory) override;
        robotruntime::RobotRunCommandResult startTrajectory() override;
        robotruntime::RobotRunCommandResult pauseTrajectory() override;
        robotruntime::RobotRunCommandResult stopTrajectory() override;
        robotruntime::RobotRunCommandResult stepTrajectory(double timeStep) override;
        robotruntime::RobotRunExecutionSnapshot trajectorySnapshot() const override;

    private:
        RobotViewport& m_viewport;
        robotruntime::RobotTrajectoryExecutionSession m_trajectorySession;
        std::vector<std::string> m_trajectoryJointNames;
    };
}
