#include "RobotQtViewerViewportServicesAdapter.h"

#include "ProjectScene.h"
#include "Telemetry/ExternalRobotTelemetry.h"
#include "RobotViewport.h"
#include <QPointer>
#include <QLabel>

#include <SimulationProject/ProjectAttachmentCommands.h>

namespace
{
    robot_qt_viewer::CollisionRuntimeDetectorInfo::Vec3Info toRuntimeVec3(
        const ProjectScene::CollisionDetectorInfo::Vec3Info& value)
    {
        robot_qt_viewer::CollisionRuntimeDetectorInfo::Vec3Info result;
        result.x = value.x;
        result.y = value.y;
        result.z = value.z;
        return result;
    }

    robot_qt_viewer::CollisionRuntimeDetectorInfo::ContactInfo toRuntimeContact(
        const ProjectScene::CollisionDetectorInfo::ContactInfo& value)
    {
        robot_qt_viewer::CollisionRuntimeDetectorInfo::ContactInfo result;
        result.bodyA = value.bodyA;
        result.bodyB = value.bodyB;
        result.position = toRuntimeVec3(value.position);
        result.normal = toRuntimeVec3(value.normal);
        result.penetrationDepth = value.penetrationDepth;
        return result;
    }

    robot_qt_viewer::CollisionRuntimeDetectorInfo::NearestInfo toRuntimeNearest(
        const ProjectScene::CollisionDetectorInfo::NearestInfo& value)
    {
        robot_qt_viewer::CollisionRuntimeDetectorInfo::NearestInfo result;
        result.valid = value.valid;
        result.state = value.state;
        result.reason = value.reason;
        result.bodyA = value.bodyA;
        result.bodyB = value.bodyB;
        result.pointA = toRuntimeVec3(value.pointA);
        result.pointB = toRuntimeVec3(value.pointB);
        result.distance = value.distance;
        return result;
    }

    robot_qt_viewer::CollisionRuntimeDetectorInfo toRuntimeDetector(
        const ProjectScene::CollisionDetectorInfo& value)
    {
        robot_qt_viewer::CollisionRuntimeDetectorInfo result;
        result.id = value.id;
        result.name = value.name;
        result.type = value.type;
        result.enabled = value.enabled;
        result.visible = value.visible;
        result.active = value.active;
        result.valid = value.valid;
        result.errorMessage = value.errorMessage;
        result.includePairCount = value.includePairCount;
        result.effectiveIncludePairCount = value.effectiveIncludePairCount;
        result.contactCount = value.contactCount;
        result.inCollision = value.inCollision;
        result.minDistance = value.minDistance;
        result.lastCheckMs = value.lastCheckMs;
        result.lastDistanceMs = value.lastDistanceMs;
        result.lastQueryMs = value.lastQueryMs;
        result.frameRobotPoseMs = value.frameRobotPoseMs;
        result.frameCollisionWorldUpdateMs = value.frameCollisionWorldUpdateMs;
        result.frameOverlayMs = value.frameOverlayMs;
        result.frameOverlayHighlightMs = value.frameOverlayHighlightMs;
        result.frameOverlayDebugBuildMs = value.frameOverlayDebugBuildMs;
        result.frameOverlayVariantFilterMs = value.frameOverlayVariantFilterMs;
        result.frameOverlayDebugSubmitMs = value.frameOverlayDebugSubmitMs;
        result.frameOverlayAuxFramesMs = value.frameOverlayAuxFramesMs;
        result.frameOverlayDetectorCount = value.frameOverlayDetectorCount;
        result.frameOverlayGeometryCount = value.frameOverlayGeometryCount;
        result.frameOverlayContactCount = value.frameOverlayContactCount;
        result.frameOverlayNearestCount = value.frameOverlayNearestCount;
        result.frameOverlayPrimitiveEstimate = value.frameOverlayPrimitiveEstimate;
        result.frameOverlayLineEstimate = value.frameOverlayLineEstimate;
        result.firstPairA = value.firstPairA;
        result.firstPairB = value.firstPairB;
        result.hasResult = value.hasResult;
        result.contacts.reserve(value.contacts.size());
        for(const ProjectScene::CollisionDetectorInfo::ContactInfo& contact : value.contacts) {
            result.contacts.push_back(toRuntimeContact(contact));
        }
        result.nearest = toRuntimeNearest(value.nearest);
        return result;
    }

    robot_qt_viewer::CollisionRuntimeStat toRuntimeStat(const RobotCollisionStat& value)
    {
        robot_qt_viewer::CollisionRuntimeStat result;
        result.name = value.name;
        result.count = value.count;
        return result;
    }

    std::vector<robot_qt_viewer::CollisionRuntimeStat> toRuntimeStats(
        const std::vector<RobotCollisionStat>& values)
    {
        std::vector<robot_qt_viewer::CollisionRuntimeStat> result;
        result.reserve(values.size());
        for(const RobotCollisionStat& value : values) {
            result.push_back(toRuntimeStat(value));
        }
        return result;
    }

    robot_qt_viewer::CollisionRuntimeModelVariantSummary toRuntimeVariantSummary(
        const RobotCollisionModelVariantSummary& value)
    {
        robot_qt_viewer::CollisionRuntimeModelVariantSummary result;
        result.variantId = value.variantId;
        result.label = value.label;
        result.linkName = value.linkName;
        result.role = value.role;
        result.source = value.source;
        result.enabled = value.enabled;
        result.replaceOriginal = value.replaceOriginal;
        result.visibleInViewport = value.visibleInViewport;
        result.selectedInViewport = value.selectedInViewport;
        result.elementCount = value.elementCount;
        result.meshElementCount = value.meshElementCount;
        result.meshVertexCount = value.meshVertexCount;
        result.meshTriangleCount = value.meshTriangleCount;
        result.unresolvedMeshCount = value.unresolvedMeshCount;
        result.primitiveCount = value.primitiveCount;
        result.elementNames = value.elementNames;
        result.geometryTypes = toRuntimeStats(value.geometryTypes);
        return result;
    }

    robot_qt_viewer::CollisionRuntimeLinkSummary toRuntimeLinkSummary(
        const RobotCollisionLinkSummary& value)
    {
        robot_qt_viewer::CollisionRuntimeLinkSummary result;
        result.linkName = value.linkName;
        result.hasVisual = value.hasVisual;
        result.hasOriginalCollision = value.hasOriginalCollision;
        result.hasOverrideCollision = value.hasOverrideCollision;
        result.hasMeshCollision = value.hasMeshCollision;
        result.hasSphereCover = value.hasSphereCover;
        result.visualCount = value.visualCount;
        result.originalCollisionCount = value.originalCollisionCount;
        result.overrideCollisionCount = value.overrideCollisionCount;
        result.effectiveCollisionCount = value.effectiveCollisionCount;
        result.geometryTypes = toRuntimeStats(value.geometryTypes);
        result.roles = toRuntimeStats(value.roles);
        result.sources = toRuntimeStats(value.sources);
        result.variants.reserve(value.variants.size());
        for(const RobotCollisionModelVariantSummary& variant : value.variants) {
            result.variants.push_back(toRuntimeVariantSummary(variant));
        }
        return result;
    }

    robot_qt_viewer::CollisionRuntimeRobotSummary toRuntimeRobotSummary(
        const RobotCollisionRobotSummary& value)
    {
        robot_qt_viewer::CollisionRuntimeRobotSummary result;
        result.robotId = value.robotId;
        result.robotName = value.robotName;
        result.hasVisualOnlyLinks = value.hasVisualOnlyLinks;
        result.hasCollisionReadyLinks = value.hasCollisionReadyLinks;
        result.hasOverrideAppliedLinks = value.hasOverrideAppliedLinks;
        result.hasMeshCollision = value.hasMeshCollision;
        result.hasSphereCover = value.hasSphereCover;
        result.linkCount = value.linkCount;
        result.visualLinkCount = value.visualLinkCount;
        result.visualOnlyLinkCount = value.visualOnlyLinkCount;
        result.collisionReadyLinkCount = value.collisionReadyLinkCount;
        result.overrideAppliedLinkCount = value.overrideAppliedLinkCount;
        result.visualCount = value.visualCount;
        result.originalCollisionCount = value.originalCollisionCount;
        result.overrideCollisionCount = value.overrideCollisionCount;
        result.effectiveCollisionCount = value.effectiveCollisionCount;
        result.geometryTypes = toRuntimeStats(value.geometryTypes);
        result.roles = toRuntimeStats(value.roles);
        result.sources = toRuntimeStats(value.sources);
        result.links.reserve(value.links.size());
        for(const RobotCollisionLinkSummary& link : value.links) {
            result.links.push_back(toRuntimeLinkSummary(link));
        }
        return result;
    }

    RobotCollisionProxyRequest toViewportProxyRequest(
        const robot_qt_viewer::CollisionRuntimeProxyRequest& value)
    {
        RobotCollisionProxyRequest result;
        result.proxyType = value.proxyType;
        result.role = value.role;
        result.inflationMargin = value.inflationMargin;
        result.maxSphereCount = value.maxSphereCount;
        result.useVisualWhenCollisionMissing = value.useVisualWhenCollisionMissing;
        result.useExistingCollisionAsInput = value.useExistingCollisionAsInput;
        return result;
    }

    robot_qt_viewer::CollisionRuntimeProxyQualitySummary toRuntimeProxyQualitySummary(
        const RobotCollisionProxyQualitySummary& value)
    {
        robot_qt_viewer::CollisionRuntimeProxyQualitySummary result;
        result.requestedMaxSpheres = value.requestedMaxSpheres;
        result.generatedSphereCount = value.generatedSphereCount;
        result.inputPointCount = value.inputPointCount;
        result.uncoveredPointCount = value.uncoveredPointCount;
        result.maxOutsideDistance = value.maxOutsideDistance;
        result.mainAxisLength = value.mainAxisLength;
        result.estimatedCrossSectionRadius = value.estimatedCrossSectionRadius;
        result.maxSphereRadius = value.maxSphereRadius;
        result.maxRadiusToLinkLength = value.maxRadiusToLinkLength;
        result.maxRadiusToCrossSectionRadius = value.maxRadiusToCrossSectionRadius;
        result.oversizedSphereCount = value.oversizedSphereCount;
        result.recommendedShape = value.recommendedShape;
        result.hasWarning = value.hasWarning;
        result.warning = value.warning;
        return result;
    }
}

namespace robot_qt_viewer
{
    RobotQtViewerDocumentViewportAdapter::RobotQtViewerDocumentViewportAdapter(
        RobotViewport& viewport,
        RobotQtViewerViewportProjectState& projectState)
        : m_viewport(viewport)
        , m_projectState(projectState)
    {
    }

    RobotQtViewerSelectionViewportAdapter::RobotQtViewerSelectionViewportAdapter(
        RobotViewport& viewport)
        : m_viewport(viewport)
    {
    }

    RobotQtViewerAssemblyViewportAdapter::RobotQtViewerAssemblyViewportAdapter(
        RobotViewport& viewport,
        RobotQtViewerViewportProjectState& projectState)
        : m_viewport(viewport)
        , m_projectState(projectState)
    {
    }

    RobotQtViewerCollisionViewportAdapter::RobotQtViewerCollisionViewportAdapter(
        RobotViewport& viewport,
        const RobotQtViewerViewportProjectState& projectState)
        : m_viewport(viewport)
        , m_projectState(projectState)
    {
    }

    RobotQtViewerVisualizationViewportAdapter::RobotQtViewerVisualizationViewportAdapter(
        RobotViewport& viewport)
        : m_viewport(viewport)
    {
    }

    RobotQtViewerRobotRunServiceAdapter::RobotQtViewerRobotRunServiceAdapter(
        RobotViewport& viewport)
        : m_viewport(viewport)
    {
    }

    void RobotQtViewerSelectionViewportAdapter::selectRobotMount(
        const QString& robotId,
        const QString& linkName,
        const QString& robotMountId)
    {
        m_viewport.selectRobotMount(robotId, linkName, robotMountId);
    }

    bool RobotQtViewerAssemblyViewportAdapter::setActivePreviewRobotMount(const QString& robotMountId)
    {
        return m_viewport.setActivePreviewRobotMount(robotMountId);
    }

    bool RobotQtViewerAssemblyViewportAdapter::previewRobotMountTransform(
        const QString& robotMountId,
        const simulation_project::TransformDesc& transform)
    {
        return m_viewport.setPreviewRobotMountTransform(robotMountId, transform);
    }

    bool RobotQtViewerAssemblyViewportAdapter::previewMountedAttachmentTransform(
        const QString& attachmentId,
        const simulation_project::TransformDesc& transform)
    {
        return m_viewport.setPreviewMountedAttachmentTransform(attachmentId, transform);
    }

    bool RobotQtViewerAssemblyViewportAdapter::previewAttachmentAsset(
        const simulation_project::AttachmentAssetDesc& asset)
    {
        return m_viewport.setPreviewAttachmentAsset(asset);
    }

    bool RobotQtViewerAssemblyViewportAdapter::previewAttachmentBinding(
        const simulation_project::BindFramesRequest& request)
    {
        if(!m_projectState.loaded) {
            return false;
        }

        simulation_project::ProjectDocument previewDocument = m_projectState.document;
        simulation_project::ProjectAttachmentCommands commands(previewDocument);
        const simulation_project::BindFramesResult result = commands.bindFrames(request);
        if(!result.success) { return false; }
        // Record the attempted preview so a failed partial load can also be restored.
        m_projectState.attachmentBindingPreviewActive = true;
        return m_viewport.loadProjectDocument(previewDocument, m_projectState.basePath);
    }

    void RobotQtViewerAssemblyViewportAdapter::clearAttachmentBindingPreview()
    {
        // Workbench switches clear all preview types, even when none was created.
        // A no-op clear must preserve the live runtime, camera and playback trace.
        if(m_projectState.loaded && m_projectState.attachmentBindingPreviewActive) {
            if(m_viewport.loadProjectDocument(m_projectState.document, m_projectState.basePath)) {
                m_projectState.attachmentBindingPreviewActive = false;
            }
        }
    }

    bool RobotQtViewerAssemblyViewportAdapter::previewRobotMountLink(
        const QString& robotMountId,
        const QString& linkName)
    {
        return m_viewport.setPreviewRobotMountLink(robotMountId, linkName);
    }

    bool RobotQtViewerAssemblyViewportAdapter::upsertPreviewRobotMount(
        const simulation_project::RobotMountDesc& mount)
    {
        return m_viewport.upsertPreviewRobotMount(mount);
    }

    bool RobotQtViewerAssemblyViewportAdapter::removePreviewRobotMount(const QString& robotMountId)
    {
        return m_viewport.removePreviewRobotMount(robotMountId);
    }

    void RobotQtViewerAssemblyViewportAdapter::previewRobotBaseTransform(
        const QString& robotId,
        const simulation_project::TransformDesc& transform)
    {
        m_viewport.previewRobotBaseTransform(robotId, transform);
    }

    void RobotQtViewerAssemblyViewportAdapter::previewSceneObjectTransform(
        const QString& objectId,
        const simulation_project::TransformDesc& transform)
    {
        m_viewport.previewSceneObjectTransform(objectId, transform);
    }

    bool RobotQtViewerAssemblyViewportAdapter::commitSceneObjectTransform(
        const QString& objectId,
        const simulation_project::TransformDesc& transform)
    {
        return m_viewport.setSceneObjectTransform(objectId, transform);
    }

    bool RobotQtViewerAssemblyViewportAdapter::removeSceneObject(const QString& objectId)
    {
        return m_viewport.removeSceneObject(objectId);
    }

    void RobotQtViewerSelectionViewportAdapter::selectObjectFrame(
        const QString& objectId,
        const QString& frameId)
    {
        m_viewport.selectObjectFrame(objectId, frameId);
    }

    bool RobotQtViewerAssemblyViewportAdapter::previewObjectFrameTransform(
        const QString& objectId,
        const QString& frameId,
        const simulation_project::TransformDesc& transform)
    {
        return m_viewport.previewObjectFrameTransform(objectId, frameId, transform);
    }

    bool RobotQtViewerAssemblyViewportAdapter::upsertPreviewObjectFrame(
        const QString& objectId,
        const simulation_project::ObjectFrameDesc& frame)
    {
        return m_viewport.upsertPreviewObjectFrame(objectId, frame);
    }

    void RobotQtViewerSelectionViewportAdapter::selectRobotLink(
        const QString& robotId,
        const QString& linkName)
    {
        m_viewport.selectRobotLink(robotId, linkName);
    }

    void RobotQtViewerSelectionViewportAdapter::selectRobotJointFrame(
        const QString& robotId,
        const QString& jointName)
    {
        m_viewport.selectRobotJointFrame(robotId, jointName);
    }

    void RobotQtViewerSelectionViewportAdapter::setActiveToolFrameRobot(const QString& robotId)
    {
        m_viewport.setActiveToolFrameRobot(robotId);
    }

    void RobotQtViewerSelectionViewportAdapter::selectSceneObject(const QString& objectId)
    {
        m_viewport.selectSceneObject(objectId);
    }

    void RobotQtViewerSelectionViewportAdapter::selectMountedAttachment(const QString& attachmentId)
    {
        m_viewport.selectMountedAttachment(attachmentId);
    }

    bool RobotQtViewerAssemblyViewportAdapter::setActiveMountedAttachment(const QString& attachmentId)
    {
        return m_viewport.setActiveMountedAttachment(attachmentId);
    }

    void RobotQtViewerAssemblyViewportAdapter::setToolFrameVisibility(
        const RobotQtViewerToolFrameVisibility& visibility)
    {
        ProjectScene::ToolFrameVisibility viewportVisibility;
        viewportVisibility.link = visibility.link;
        viewportVisibility.robotMount = visibility.robotMount;
        viewportVisibility.toolMount = visibility.toolMount;
        viewportVisibility.visual = visibility.visual;
        viewportVisibility.tcp = visibility.tcp;
        viewportVisibility.sensorPreview = visibility.sensorPreview;
        m_viewport.setToolFrameVisibility(viewportVisibility);
    }

    void RobotQtViewerAssemblyViewportAdapter::setRobotMountFrameVisibility(
        bool selectedLinkFrameVisible,
        bool mountFrameVisible)
    {
        m_viewport.setRobotMountFrameVisibility(selectedLinkFrameVisible, mountFrameVisible);
    }

    void RobotQtViewerAssemblyViewportAdapter::setPinnedRobotMountFrames(const QStringList& robotMountIds)
    {
        m_viewport.setPinnedRobotMountFrames(robotMountIds);
    }

    void RobotQtViewerSelectionViewportAdapter::focusMountFrameLink(
        const QString& robotId,
        const QString& linkName)
    {
        m_viewport.focusMountFrameLink(robotId, linkName);
    }

    void RobotQtViewerSelectionViewportAdapter::clearMountFrameLinkFocus()
    {
        m_viewport.clearMountFrameLinkFocus();
    }

    void RobotQtViewerSelectionViewportAdapter::focusObjectFrameObject(const QString& objectId)
    {
        m_viewport.focusObjectFrameObject(objectId);
    }

    void RobotQtViewerSelectionViewportAdapter::clearObjectFrameObjectFocus()
    {
        m_viewport.clearObjectFrameObjectFocus();
    }

    void RobotQtViewerSelectionViewportAdapter::focusMountedAttachment(const QString& attachmentId)
    {
        m_viewport.focusMountedAttachment(attachmentId);
    }

    void RobotQtViewerSelectionViewportAdapter::clearMountedAttachmentFocus()
    {
        m_viewport.clearMountedAttachmentFocus();
    }

    void RobotQtViewerCollisionViewportAdapter::previewObjectCollisionModelVariant(
        const QString& objectId,
        const QString& variantId)
    {
        m_viewport.previewObjectCollisionModelVariant(objectId, variantId);
    }

    void RobotQtViewerCollisionViewportAdapter::clearObjectCollisionModelVariantPreview()
    {
        m_viewport.clearObjectCollisionModelVariantPreview();
    }

    void RobotQtViewerCollisionViewportAdapter::previewCollisionPairTargets(
        const QString& robotAId,
        const QString& linkAName,
        const QString& objectAId,
        const QString& attachmentAId,
        const QString& robotBId,
        const QString& linkBName,
        const QString& objectBId,
        const QString& attachmentBId)
    {
        m_viewport.previewCollisionPairTargets(
            robotAId,
            linkAName,
            objectAId,
            attachmentAId,
            robotBId,
            linkBName,
            objectBId,
            attachmentBId);
    }

    RobotQtViewerViewportLoadResult RobotQtViewerDocumentViewportAdapter::loadProjectDocument(
        const simulation_project::ProjectDocument& document,
        const std::filesystem::path& basePath)
    {
        RobotQtViewerViewportLoadResult result;
        m_projectState.attachmentBindingPreviewActive = false;
        result.success = m_viewport.loadProjectDocument(document, basePath);
        if(!result.success) {
            result.errorMessage = m_viewport.lastError();
        } else {
            m_projectState.document = document;
            m_projectState.basePath = basePath;
            m_projectState.loaded = true;
        }
        return result;
    }

    std::filesystem::path RobotQtViewerCollisionViewportAdapter::projectBasePath() const
    {
        return m_projectState.loaded ? m_projectState.basePath : m_viewport.projectBasePath();
    }

    bool RobotQtViewerCollisionViewportAdapter::refreshCollisionConfiguration(
        const simulation_project::ProjectDocument& document,
        const std::filesystem::path& basePath)
    {
        return m_viewport.refreshCollisionConfiguration(document, basePath);
    }

    void RobotQtViewerCollisionViewportAdapter::setCollisionGeometryVisible(bool visible)
    {
        m_viewport.setCollisionGeometryVisible(visible);
    }

    bool RobotQtViewerCollisionViewportAdapter::collisionQueriesEnabled() const
    {
        return m_viewport.collisionQueriesEnabled();
    }

    bool RobotQtViewerCollisionViewportAdapter::setCollisionQueriesEnabled(bool enabled)
    {
        return m_viewport.setCollisionQueriesEnabled(enabled);
    }

    bool RobotQtViewerCollisionViewportAdapter::setActiveCollisionDetector(const QString& detectorId)
    {
        return m_viewport.setActiveCollisionDetector(detectorId);
    }

    bool RobotQtViewerCollisionViewportAdapter::refreshCollisionDetectorNearest(
        const QString& detectorId)
    {
        return m_viewport.refreshCollisionDetectorNearest(detectorId);
    }

    bool RobotQtViewerCollisionViewportAdapter::setCollisionDetectorEnabled(const QString& detectorId, bool enabled)
    {
        return m_viewport.setCollisionDetectorEnabled(detectorId, enabled);
    }

    bool RobotQtViewerCollisionViewportAdapter::setCollisionDetectorVisible(const QString& detectorId, bool visible)
    {
        return m_viewport.setCollisionDetectorVisible(detectorId, visible);
    }

    bool RobotQtViewerCollisionViewportAdapter::updateCollisionDetectorRuntimeOptions(
        const simulation_project::CollisionDetectorDesc& detector)
    {
        return m_viewport.updateCollisionDetectorRuntimeOptions(detector);
    }

    bool RobotQtViewerCollisionViewportAdapter::rebuildCollisionDetectorsFromDocument(
        const simulation_project::ProjectDocument& document)
    {
        return m_viewport.rebuildCollisionDetectorsFromDocument(document);
    }

    bool RobotQtViewerCollisionViewportAdapter::removeCollisionDetector(const QString& detectorId)
    {
        return m_viewport.removeCollisionDetector(detectorId);
    }

    bool RobotQtViewerCollisionViewportAdapter::setVisibleRobotCollisionVariant(
        const QString& robotId,
        const QString& linkName,
        const QString& variantId)
    {
        return m_viewport.setVisibleRobotCollisionVariant(robotId, linkName, variantId);
    }

    QString RobotQtViewerCollisionViewportAdapter::visibleRobotCollisionVariant(
        const QString& robotId,
        const QString& linkName) const
    {
        return m_viewport.visibleRobotCollisionVariant(robotId, linkName);
    }

    CollisionRuntimeRobotSummary RobotQtViewerCollisionViewportAdapter::robotCollisionSummary(
        const QString& robotId) const
    {
        return toRuntimeRobotSummary(m_viewport.robotCollisionSummary(robotId));
    }

    std::vector<CollisionRuntimeDetectorInfo> RobotQtViewerCollisionViewportAdapter::collisionRuntimeDetectors() const
    {
        const std::vector<ProjectScene::CollisionDetectorInfo> viewportDetectors = m_viewport.collisionDetectors();
        std::vector<CollisionRuntimeDetectorInfo> detectors;
        detectors.reserve(viewportDetectors.size());
        for(const ProjectScene::CollisionDetectorInfo& detector : viewportDetectors) {
            detectors.push_back(toRuntimeDetector(detector));
        }
        return detectors;
    }

    bool RobotQtViewerCollisionViewportAdapter::generateRobotCollisionProxies(
        const QString& robotId,
        const QString& linkName,
        const CollisionRuntimeProxyRequest& request,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateRobotCollisionProxies(
            robotId,
            linkName,
            toViewportProxyRequest(request),
            elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::generateRobotCollisionProxiesFromExistingCollision(
        const QString& robotId,
        const QString& linkName,
        const CollisionRuntimeProxyRequest& request,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateRobotCollisionProxiesFromExistingCollision(
            robotId,
            linkName,
            toViewportProxyRequest(request),
            elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::generateRobotCollisionProxiesFromExistingCollision(
        const QString& robotId,
        const CollisionRuntimeProxyRequest& request,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateRobotCollisionProxiesFromExistingCollision(
            robotId,
            toViewportProxyRequest(request),
            elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::generateRobotCollisionCoacdFromVisual(
        const QString& robotId,
        const QString& linkName,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateRobotCollisionCoacdFromVisual(robotId, linkName, elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::generateRobotCollisionCoacdFromExistingCollision(
        const QString& robotId,
        const QString& linkName,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateRobotCollisionCoacdFromExistingCollision(robotId, linkName, elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::generateObjectCollisionCoacdFromVisual(
        const QString& objectId,
        std::vector<simulation_project::ObjectCollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateObjectCollisionCoacdFromVisual(objectId, elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::generateMissingRobotCollisionProxies(
        const QString& robotId,
        const CollisionRuntimeProxyRequest& request,
        std::vector<simulation_project::CollisionElementOverrideDesc>& elements) const
    {
        return m_viewport.generateMissingRobotCollisionProxies(
            robotId,
            toViewportProxyRequest(request),
            elements);
    }

    bool RobotQtViewerCollisionViewportAdapter::evaluateRobotCollisionProxyQuality(
        const QString& robotId,
        const QString& linkName,
        const CollisionRuntimeProxyRequest& request,
        const std::vector<simulation_project::CollisionElementOverrideDesc>& elements,
        bool useExistingCollisionInput,
        CollisionRuntimeProxyQualitySummary& summary) const
    {
        RobotCollisionProxyQualitySummary viewportSummary;
        if(!m_viewport.evaluateRobotCollisionProxyQuality(
               robotId,
               linkName,
               toViewportProxyRequest(request),
               elements,
               useExistingCollisionInput,
               viewportSummary)) {
            return false;
        }
        summary = toRuntimeProxyQualitySummary(viewportSummary);
        return true;
    }

    bool RobotQtViewerVisualizationViewportAdapter::applySurfaceScalarOverlay(
        const smrobot::visualization::SurfaceScalarOverlay& overlay,
        QString* errorMessage,
        bool blackOutsideRange)
    {
        return m_viewport.applySurfaceScalarOverlay(
            overlay, errorMessage, blackOutsideRange);
    }

    bool RobotQtViewerVisualizationViewportAdapter::setSurfaceScalarOverlayVisible(
        const QString& objectId,
        bool visible)
    {
        return m_viewport.setSurfaceScalarOverlayVisible(objectId, visible);
    }

    bool RobotQtViewerVisualizationViewportAdapter::clearSurfaceScalarOverlay(const QString& objectId)
    {
        return m_viewport.clearSurfaceScalarOverlay(objectId);
    }

    void RobotQtViewerVisualizationViewportAdapter::setSurfaceScalarProbeEnabled(
        bool enabled,
        const QString& objectId)
    {
        m_viewport.setSurfaceScalarProbeEnabled(enabled, objectId);
    }

    bool RobotQtViewerVisualizationViewportAdapter::setSceneObjectVisibilityOverride(
        const QString& objectId,
        bool visible)
    {
        return m_viewport.setSceneObjectVisibilityOverride(objectId, visible);
    }

    bool RobotQtViewerVisualizationViewportAdapter::clearSceneObjectVisibilityOverride(
        const QString& objectId)
    {
        return m_viewport.clearSceneObjectVisibilityOverride(objectId);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::upsertCustomMesh(
        const smrobot::visualization::CustomMeshDesc& desc,
        smrobot::visualization::CustomMeshHandle& handle)
    {
        return m_viewport.upsertCustomMesh(desc, handle);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::updateCustomMeshGeometry(
        smrobot::visualization::CustomMeshHandle handle,
        const smrobot::visualization::MeshData& mesh)
    {
        return m_viewport.updateCustomMeshGeometry(handle, mesh);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::updateCustomMeshColors(
        smrobot::visualization::CustomMeshHandle handle,
        const std::vector<smrobot::visualization::Color4f>& colors)
    {
        return m_viewport.updateCustomMeshColors(handle, colors);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::setCustomMeshTransform(
        smrobot::visualization::CustomMeshHandle handle,
        const smrobot::visualization::TransformMatrix& transform)
    {
        return m_viewport.setCustomMeshTransform(handle, transform);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::setCustomMeshAppearance(
        smrobot::visualization::CustomMeshHandle handle,
        const smrobot::visualization::MeshAppearance& appearance)
    {
        return m_viewport.setCustomMeshAppearance(handle, appearance);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::setCustomMeshVisible(
        smrobot::visualization::CustomMeshHandle handle,
        bool visible)
    {
        return m_viewport.setCustomMeshVisible(handle, visible);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::removeCustomMesh(
        smrobot::visualization::CustomMeshHandle handle)
    {
        return m_viewport.removeCustomMesh(handle);
    }

    smrobot::visualization::CustomMeshResult
    RobotQtViewerVisualizationViewportAdapter::clearCustomMeshes(const std::string& ownerId)
    {
        return m_viewport.clearCustomMeshes(ownerId);
    }

    bool RobotQtViewerRobotRunServiceAdapter::jointValue(
        const std::string& robotId,
        const std::string& jointName,
        double& value) const
    {
        bool ok = false;
        value = m_viewport.robotJointValue(
            QString::fromStdString(robotId),
            QString::fromStdString(jointName),
            &ok);
        return ok;
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::setJointValue(
        const std::string& robotId,
        const std::string& jointName,
        double value)
    {
        m_viewport.setRobotJointValue(
            QString::fromStdString(robotId),
            QString::fromStdString(jointName),
            value);
        return { true, "Robot joint value updated." };
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::setAutoMotion(
        const std::string& robotId,
        bool enabled,
        double amplitude,
        double speed)
    {
        m_viewport.setRobotAutoMotion(
            QString::fromStdString(robotId),
            enabled,
            amplitude,
            speed);
        return { true, enabled ? "Robot auto motion enabled." : "Robot auto motion disabled." };
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::loadTrajectory(
        const std::string& robotId,
        const std::string& trajectoryId,
        const std::vector<std::string>& jointNames,
        const robottrajectory::JointTrajectory& trajectory)
    {
        if(trajectory.empty() || jointNames.size() != trajectory.points.front().q.size()) {
            return { false, "Trajectory joint names must match its joint values." };
        }
        const robotruntime::RobotRunCommandResult result =
            m_trajectorySession.load(robotId, trajectoryId, trajectory);
        if(result.success) {
            m_trajectoryJointNames = jointNames;
        }
        return result;
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::startTrajectory()
    {
        const robotruntime::RobotRunCommandResult result = m_trajectorySession.start();
        if(result.success) {
            const robotruntime::RobotRunExecutionSnapshot snapshot = m_trajectorySession.snapshot();
            for(std::size_t index = 0;
                index < m_trajectoryJointNames.size() && index < snapshot.jointValues.size();
                ++index) {
                m_viewport.setRobotJointValue(
                    QString::fromStdString(snapshot.robotId),
                    QString::fromStdString(m_trajectoryJointNames[index]),
                    snapshot.jointValues[index]);
            }
        }
        return result;
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::pauseTrajectory()
    {
        return m_trajectorySession.pause();
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::stopTrajectory()
    {
        return m_trajectorySession.stop();
    }

    robotruntime::RobotRunCommandResult RobotQtViewerRobotRunServiceAdapter::stepTrajectory(
        double timeStep)
    {
        const robotruntime::RobotRunCommandResult result = m_trajectorySession.step(timeStep);
        if(!result.success) {
            return result;
        }
        const robotruntime::RobotRunExecutionSnapshot snapshot = m_trajectorySession.snapshot();
        for(std::size_t index = 0;
            index < m_trajectoryJointNames.size() && index < snapshot.jointValues.size();
            ++index) {
            m_viewport.setRobotJointValue(
                QString::fromStdString(snapshot.robotId),
                QString::fromStdString(m_trajectoryJointNames[index]),
                snapshot.jointValues[index]);
        }
        return result;
    }

    robotruntime::RobotRunExecutionSnapshot
    RobotQtViewerRobotRunServiceAdapter::trajectorySnapshot() const
    {
        return m_trajectorySession.snapshot();
    }
    bool RobotQtViewerVisualizationViewportAdapter::applySurfaceScalarOverlayModel(
        const smrobot::visualization::SurfaceScalarOverlay& overlay,
        const assetcore::ModelDesc& model,
        QString* errorMessage,
        bool blackOutsideRange)
    {
        return m_viewport.applySurfaceScalarOverlayModel(
            overlay, model, errorMessage, blackOutsideRange);
    }
    bool RobotQtViewerVisualizationViewportAdapter::setCoatingPredictionModel(
        const QString& objectId,
        const assetcore::ModelDesc& model,
        QString* errorMessage)
    {
        return m_viewport.setCoatingPredictionModel(objectId, model, errorMessage);
    }
    bool RobotQtViewerVisualizationViewportAdapter::clearCoatingPredictionModel(
        const QString& objectId)
    {
        return m_viewport.clearCoatingPredictionModel(objectId);
    }
    void RobotQtViewerVisualizationViewportAdapter::setCoatingTrajectoryPreview(
        const std::vector<CoatingTrajectoryPreviewPoint>& points,
        bool visible,
        bool showTrajectoryLines,
        bool showSprayPointFrames)
    {
        std::vector<ProjectScene::CoatingTrajectoryPreviewPoint> converted;
        converted.reserve(points.size());
        for(const CoatingTrajectoryPreviewPoint& point : points) {
            ProjectScene::CoatingTrajectoryPreviewPoint target;
            target.positionX = point.positionX;
            target.positionY = point.positionY;
            target.positionZ = point.positionZ;
            target.directionX = point.directionX;
            target.directionY = point.directionY;
            target.directionZ = point.directionZ;
            target.frameXAxisX = point.frameXAxisX;
            target.frameXAxisY = point.frameXAxisY;
            target.frameXAxisZ = point.frameXAxisZ;
            target.frameYAxisX = point.frameYAxisX;
            target.frameYAxisY = point.frameYAxisY;
            target.frameYAxisZ = point.frameYAxisZ;
            target.frameZAxisX = point.frameZAxisX;
            target.frameZAxisY = point.frameZAxisY;
            target.frameZAxisZ = point.frameZAxisZ;
            target.sprayEnabled = point.sprayEnabled;
            target.startsNewSegment = point.startsNewSegment;
            converted.push_back(target);
        }
        m_viewport.setCoatingTrajectoryPreview(
            converted,
            visible,
            showTrajectoryLines,
            showSprayPointFrames);
    }
    bool RobotQtViewerVisualizationViewportAdapter::setCoatingTrajectoryPreviewVisible(
        bool visible,
        bool showTrajectoryLines,
        bool showSprayPointFrames)
    {
        return m_viewport.setCoatingTrajectoryPreviewVisible(
            visible,
            showTrajectoryLines,
            showSprayPointFrames);
    }
    void RobotQtViewerVisualizationViewportAdapter::setCoatingPredictionDebugState(
        const CoatingPredictionDebugState& state)
    {
        ProjectScene::CoatingPredictionDebugState converted;
        converted.visible = state.visible;
        converted.objectId = state.objectId.toStdString();
        converted.axisOriginX = state.axisOriginX;
        converted.axisOriginY = state.axisOriginY;
        converted.axisOriginZ = state.axisOriginZ;
        converted.axisDirectionX = state.axisDirectionX;
        converted.axisDirectionY = state.axisDirectionY;
        converted.axisDirectionZ = state.axisDirectionZ;
        converted.axisLength = state.axisLength;
        converted.markerRadius = state.markerRadius;
        const auto convertTriangle = [](const CoatingPredictionDebugTriangle& source) {
            ProjectScene::CoatingPredictionDebugTriangle target;
            target.ax = source.ax;
            target.ay = source.ay;
            target.az = source.az;
            target.bx = source.bx;
            target.by = source.by;
            target.bz = source.bz;
            target.cx = source.cx;
            target.cy = source.cy;
            target.cz = source.cz;
            return target;
        };
        const auto convertTriangles = [&](const std::vector<CoatingPredictionDebugTriangle>& source,
                                          std::vector<ProjectScene::CoatingPredictionDebugTriangle>& target) {
            target.reserve(source.size());
            for(const auto& triangle : source) {
                target.push_back(convertTriangle(triangle));
            }
        };
        const auto convertPoint = [](const CoatingPredictionDebugPoint& source) {
            ProjectScene::CoatingPredictionDebugPoint target;
            target.positionX = source.positionX;
            target.positionY = source.positionY;
            target.positionZ = source.positionZ;
            target.directionX = source.directionX;
            target.directionY = source.directionY;
            target.directionZ = source.directionZ;
            return target;
        };
        const auto convertPoints = [&](const std::vector<CoatingPredictionDebugPoint>& source,
                                       std::vector<ProjectScene::CoatingPredictionDebugPoint>& target) {
            target.reserve(source.size());
            for(const auto& point : source) {
                target.push_back(convertPoint(point));
            }
        };
        convertTriangles(state.seedTriangles, converted.seedTriangles);
        convertTriangles(state.cylindricalTriangles, converted.cylindricalTriangles);
        convertTriangles(state.localSectorTriangles, converted.localSectorTriangles);
        converted.localSectorVertexIndices = state.localSectorVertexIndices;
        convertPoints(state.profileLinePoints, converted.profileLinePoints);
        convertPoints(state.selectedProfileLinePoints, converted.selectedProfileLinePoints);
        convertPoints(state.sprayPoints, converted.sprayPoints);
        m_viewport.setCoatingPredictionDebugState(converted);
    }
    void RobotQtViewerVisualizationViewportAdapter::setCoatingPredictionDebugVisibility(
        const CoatingPredictionDebugVisibility& visibility)
    {
        ProjectScene::CoatingPredictionDebugVisibility converted;
        converted.cylindricalSurface = visibility.cylindricalSurface;
        converted.rotationAxis = visibility.rotationAxis;
        converted.localSector = visibility.localSector;
        converted.sprayPoints = visibility.sprayPoints;
        m_viewport.setCoatingPredictionDebugVisibility(converted);
    }
    void RobotQtViewerVisualizationViewportAdapter::clearCoatingPredictionDebugState()
    {
        m_viewport.clearCoatingPredictionDebugState();
    }
    bool RobotQtViewerVisualizationViewportAdapter::setCoatingModelVisible(
        const QString& objectId,
        bool visible)
    {
        return m_viewport.setCoatingModelVisible(objectId, visible);
    }
    void RobotQtViewerVisualizationViewportAdapter::setCoatingModelVisibilities(
        const QHash<QString, bool>& visibility)
    {
        std::unordered_map<std::string, bool> converted;
        converted.reserve(static_cast<std::size_t>(visibility.size()));
        for(auto iterator = visibility.constBegin(); iterator != visibility.constEnd(); ++iterator) {
            converted[iterator.key().toStdString()] = iterator.value();
        }
        m_viewport.setCoatingModelVisibilities(converted);
    }
    void RobotQtViewerVisualizationViewportAdapter::clearCoatingModelVisibility(
        const QString& objectId)
    {
        m_viewport.clearCoatingModelVisibility(objectId);
    }
    void RobotQtViewerVisualizationViewportAdapter::setCoatingAnalysisView(bool active)
    {
        m_viewport.setCoatingAnalysisView(active);
    }
    void RobotQtViewerVisualizationViewportAdapter::setGpuPredictionBusy(bool busy)
    {
        m_viewport.setUpdatesEnabled(!busy);
        if(!busy) m_viewport.update();
    }
    void RobotQtViewerVisualizationViewportAdapter::focusCoatingObject(
        const QString& objectId,
        double duration)
    {
        m_viewport.focusCoatingObject(objectId, duration);
    }
    void RobotQtViewerVisualizationViewportAdapter::beginRotationSurfacePick()
    {
        m_viewport.beginCoatingSurfacePick();
    }

    IRobotQtViewerMotionPlanningViewportPort::RobotForwardKinematics
    RobotQtViewerMotionPlanningViewportAdapter::robotForwardKinematics(const QString& robotId,
        const std::vector<std::string>& jointNames, bool includeTool) const
    {
        return m_viewport.robotForwardKinematics(robotId, jointNames, includeTool);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setRobotJointValue(
        const QString& robotId,
        const QString& jointName,
        double value)
    {
        m_viewport.setRobotJointValue(robotId, jointName, value);
    }

    bool RobotQtViewerMotionPlanningViewportAdapter::setRobotJointValues(
        const QString& robotId, const std::vector<std::string>& jointNames,
        const std::vector<double>& values)
    {
        return m_viewport.setRobotJointValues(robotId, jointNames, values);
    }

    bool RobotQtViewerMotionPlanningViewportAdapter::setRobotJointValuesByDof(
        const std::vector<double>& values, QString* matchedRobotId)
    {
        return m_viewport.setRobotJointValuesByDof(values, matchedRobotId);
    }

    quint64 RobotQtViewerMotionPlanningViewportAdapter::requestFramePresentation()
    {
        return m_viewport.requestFramePresentation();
    }

    bool RobotQtViewerMotionPlanningViewportAdapter::isFramePresented(quint64 ticket) const
    {
        return m_viewport.isFramePresented(ticket);
    }

    double RobotQtViewerMotionPlanningViewportAdapter::robotJointValue(
        const QString& robotId,
        const QString& jointName,
        bool* ok) const
    {
        return m_viewport.robotJointValue(robotId, jointName, ok);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setSprayDirectionNegativeZ(bool negativeZ)
    {
        m_viewport.setSprayDirectionNegativeZ(negativeZ);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setSprayRangeVisible(
        const QString& robotId,
        bool visible)
    {
        m_viewport.setSprayRangeVisible(robotId, visible);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setEndEffectorTraceVisible(const QString& robotId, bool visible)
    {
        m_viewport.setEndEffectorTraceVisible(robotId, visible);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::clearEndEffectorTrace()
    {
        m_viewport.clearEndEffectorTrace();
    }

    void RobotQtViewerMotionPlanningViewportAdapter::appendEndEffectorTraceSample()
    {
        m_viewport.appendEndEffectorTraceSample();
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setTelemetryEndEffectorTraceVisible(bool visible)
    {
        m_viewport.setTelemetryEndEffectorTraceVisible(visible);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::clearTelemetryEndEffectorTrace()
    {
        m_viewport.clearTelemetryEndEffectorTrace();
    }

    void RobotQtViewerMotionPlanningViewportAdapter::appendTelemetryEndEffectorTraceSample(const QString& robotId)
    {
        m_viewport.appendTelemetryEndEffectorTraceSample(robotId);
    }

    SprayInspectionView RobotQtViewerMotionPlanningViewportAdapter::createSprayInspectionView(
        QWidget* parent, const QString& robotId, const std::vector<std::string>& jointNames,
        const std::vector<simulation_project::TransformDesc>& trajectory) const
    {
        auto* view = new RobotViewport(parent);
        view->setObjectName(QStringLiteral("sprayPlotGeometryView"));
        view->setMinimumSize(300, 320);
        // Keep actual robot/tool/workpiece meshes; decorative ground can occlude a close camera.
        view->setEnvironmentPreset(ProjectSceneEnvironmentPreset::None);
        auto inspectionDocument = m_viewport.runtimeDocumentSnapshot();
        if(jointNames.empty()) {
            // A TCP-only sample does not imply a robot configuration. The tool
            // remains visible and follows the measured full TCP pose in the inspection scene.
            for(auto& robot : inspectionDocument.robots) if(robot.id == robotId.toStdString()) robot.visible = false;
        }
        view->loadProjectDocument(inspectionDocument, m_viewport.projectBasePath());
        QObject::connect(view, &RobotViewport::environmentSceneReady, view, [view, trajectory]() {
            view->setTrajectoryControlPointOverlay(QStringLiteral("spray_inspection"), trajectory, false, false);
            view->setCollisionQueriesEnabled(false);
        });
        auto* metrics = new QLabel(view);
        metrics->setObjectName(QStringLiteral("sprayInspectionMetrics"));
        metrics->setAttribute(Qt::WA_TransparentForMouseEvents);
        metrics->setStyleSheet(QStringLiteral("QLabel { color: #172b3a; background: rgba(255,255,255,220); padding: 5px; border-radius: 3px; }"));
        metrics->move(8,8);
        const QPointer<RobotViewport> guarded(view);
        SprayInspectionView out; out.widget = view;
        out.select = [guarded, metrics, robotId, jointNames](const SprayMeasurementResult& value, const std::vector<double>& joints) {
            if(!guarded) return;
            ProjectScene::SprayMeasurement geometry;
            geometry.valid=value.valid; geometry.poseValid=value.poseValid;
            geometry.tcpPosition=value.tcpPosition; geometry.sprayDirection=value.sprayDirection;
            geometry.hitPosition=value.hitPosition; geometry.surfaceNormal=value.surfaceNormal;
            geometry.distanceMeters=value.distanceMeters; geometry.angleDegrees=value.angleDegrees;
            geometry.tcpPoseAvailable=value.tcpPoseAvailable; geometry.tcpPose=value.tcpPose;
            guarded->setSprayInspectionSample(robotId, jointNames, joints, geometry);
            metrics->setText(value.valid
                ? QStringLiteral("TCP -> hit: %1 mm\nIncidence: %2 deg").arg(value.distanceMeters*1000.,0,'f',3).arg(value.angleDegrees,0,'f',3)
                : QStringLiteral("No surface intersection"));
            metrics->adjustSize(); metrics->raise();
        };
        out.focus = [guarded]() { if(guarded) guarded->focusSprayInspection(); };
        return out;
    }

    SprayMeasurementResult RobotQtViewerMotionPlanningViewportAdapter::sprayMeasurementAtPose(
        const Eigen::Isometry3d& tcp, const QString& targetId, bool negativeZ) const
    {
        const auto measurement = m_viewport.sprayMeasurementAtPose(tcp, targetId, negativeZ);
        return {measurement.valid, measurement.distanceMeters, measurement.angleDegrees,
            QString::fromStdString(measurement.errorMessage), measurement.poseValid,
            measurement.tcpPosition, measurement.sprayDirection, measurement.hitPosition, measurement.surfaceNormal, measurement.tcpPoseAvailable, measurement.tcpPose};
    }

    SprayMeasurementResult RobotQtViewerMotionPlanningViewportAdapter::sprayMeasurement(const QString& robotId) const
    {
        const auto measurement = m_viewport.sprayMeasurement(robotId);
        return { measurement.valid, measurement.distanceMeters, measurement.angleDegrees,
            QString::fromStdString(measurement.errorMessage), measurement.poseValid,
            measurement.tcpPosition, measurement.sprayDirection, measurement.hitPosition, measurement.surfaceNormal, measurement.tcpPoseAvailable, measurement.tcpPose };
    }

    SprayMeasurementResult RobotQtViewerMotionPlanningViewportAdapter::sprayMeasurement(
        const QString& robotId, const QString& targetId) const
    {
        const auto measurement = m_viewport.sprayMeasurement(robotId, targetId);
        return { measurement.valid, measurement.distanceMeters, measurement.angleDegrees,
            QString::fromStdString(measurement.errorMessage), measurement.poseValid,
            measurement.tcpPosition, measurement.sprayDirection, measurement.hitPosition, measurement.surfaceNormal, measurement.tcpPoseAvailable, measurement.tcpPose };
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setTrajectoryControlPointOverlay(
        const QString& trajectoryId,
        const std::vector<simulation_project::TransformDesc>& controlPoints, bool showPoints, bool showOrientations)
    {
        m_viewport.setTrajectoryControlPointOverlay(trajectoryId, controlPoints, showPoints, showOrientations);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setTrajectoryControlPointLineVisible(bool visible)
    {
        m_viewport.setTrajectoryControlPointLineVisible(visible);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::setTrajectoryDerivedPointOverlays(
        const QString& trajectoryId,
        const std::vector<simulation_project::TransformDesc>& surfacePoints, bool surfaceVisible,
        bool surfaceLineVisible,
        const std::vector<simulation_project::TransformDesc>& offsetPoints, bool offsetVisible,
        bool offsetLineVisible)
    {
        m_viewport.setTrajectoryDerivedPointOverlays(trajectoryId, surfacePoints, surfaceVisible,
            surfaceLineVisible, offsetPoints, offsetVisible, offsetLineVisible);
    }

    void RobotQtViewerMotionPlanningViewportAdapter::clearTrajectoryControlPointOverlay(
        const QString& trajectoryId)
    {
        m_viewport.clearTrajectoryControlPointOverlay(trajectoryId);
    }
}

void robot_qt_viewer::RobotQtViewerMotionPlanningViewportAdapter::setTrajectoryHighlightRange(int first,int last) { m_viewport.setTrajectoryHighlightRange(first,last); }

namespace robot_qt_viewer
{
    TelemetrySpeedStatistics RobotQtViewerTelemetryAdapter::speedStatistics(double startSeconds, double endSeconds,
        TelemetrySpeedCalculationMethod method, double minimumIntervalSeconds) const
    {
        const auto source = m_session->speedStatistics(startSeconds, endSeconds,
            method == TelemetrySpeedCalculationMethod::WindowAverage
                ? robot_telemetry::SpeedCalculationMethod::WindowAverage
                : robot_telemetry::SpeedCalculationMethod::InstantaneousDifference,
            minimumIntervalSeconds);
        TelemetrySpeedStatistics result;
        result.valid = source.valid; result.startSeconds = source.startSeconds; result.endSeconds = source.endSeconds;
        result.method = method; result.minimumIntervalSeconds = source.minimumIntervalSeconds;
        result.sampleCount = static_cast<quint64>(source.sampleCount);
        result.tcpLinearMaxMmPerSecond = source.tcpLinearMaxMmPerSecond;
        result.tcpLinearAverageMmPerSecond = source.tcpLinearAverageMmPerSecond;
        result.tcpAngularMaxDegPerSecond = source.tcpAngularMaxDegPerSecond;
        result.tcpAngularAverageDegPerSecond = source.tcpAngularAverageDegPerSecond;
        result.diagnostic = QString::fromUtf8(source.diagnostic.c_str());
        for(double value : source.jointMaxSpeed) result.jointMaxSpeed.push_back(value);
        for(double value : source.jointAverageSpeed) result.jointAverageSpeed.push_back(value);
        for(const auto& point : source.points) {
            TelemetrySpeedPoint mapped;
            mapped.elapsedSeconds = point.elapsedSeconds;
            mapped.tcpLinearMmPerSecond = point.tcpLinearMmPerSecond;
            mapped.tcpAngularDegPerSecond = point.tcpAngularDegPerSecond;
            for(double value : point.jointAbsSpeed) mapped.jointAbsSpeed.push_back(value);
            result.points.push_back(std::move(mapped));
        }
        return result;
    }
}

namespace robot_qt_viewer
{
    RobotQtViewerTelemetryAdapter::RobotQtViewerTelemetryAdapter() : m_session(std::make_unique<robot_telemetry::Session>()) {}
    RobotQtViewerTelemetryAdapter::~RobotQtViewerTelemetryAdapter() = default;
    bool RobotQtViewerTelemetryAdapter::discover() { return m_session->discover(); }
    bool RobotQtViewerTelemetryAdapter::start(const QString& endpointId, int group, int intervalMs, const QString& csvPath) {
        robot_telemetry::Options options;
        options.endpointId = endpointId.toUtf8().toStdString();
        options.group = group; options.intervalMs = intervalMs;
        options.csvPath = std::filesystem::u8path(csvPath.toUtf8().toStdString());
        return m_session->start(options);
    }
    void RobotQtViewerTelemetryAdapter::stop() { m_session->stop(); }
    TelemetrySnapshot RobotQtViewerTelemetryAdapter::snapshot() const {
        const auto source = m_session->snapshot();
        TelemetrySnapshot result;
        using State = robot_telemetry::State;
        result.busy = source.state == State::Discovering || source.state == State::Connecting || source.state == State::Streaming;
        result.streaming = source.state == State::Streaming;
        result.fault = source.state == State::Fault;
        for(const auto& endpoint : source.endpoints) result.endpoints.push_back({QString::fromUtf8(endpoint.id.c_str()), QString::fromUtf8(endpoint.name.c_str())});
        result.source = QString::fromUtf8(source.source.c_str());
        result.diagnostic = QString::fromUtf8(source.diagnostic.c_str());
        result.csvPath = QString::fromStdWString(source.csvPath.wstring());
        result.sequence = source.latest.sequence;
        result.receivedUnixNs = source.latest.receivedUnixNs;
        result.elapsedNs = source.latest.elapsedNs;
        result.readLatencyNs = source.latest.readLatencyNs;
        result.joints = source.latest.joints;
        result.tcpPoseValid = source.latest.tcpPose.valid;
        result.tcpXyzwpr = source.latest.tcpPose.xyzwpr;
        result.tcpUserFrame = source.latest.tcpPose.userFrame;
        result.tcpUserTool = source.latest.tcpPose.userTool;
        result.tcpDiagnostic = QString::fromUtf8(source.latest.tcpPose.diagnostic.c_str());
        return result;
    }
}
