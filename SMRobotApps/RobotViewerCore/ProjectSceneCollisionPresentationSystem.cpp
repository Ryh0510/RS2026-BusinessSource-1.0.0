#include "ProjectSceneCollisionPresentationSystem.h"

namespace
{
    std::string collisionVariantSelectionKey(
        const std::string& robotId,
        const std::string& linkName)
    {
        return robotId + "|" + linkName;
    }
}

void ProjectSceneCollisionPresentationSystem::GeometryOverlayCache::clear()
{
    valid = false;
    key.clear();
    data.clear();
}

void ProjectSceneCollisionPresentationSystem::FrameMetrics::resetOverlay()
{
    overlayMs = 0.0;
    overlayHighlightMs = 0.0;
    overlayDebugBuildMs = 0.0;
    overlayVariantFilterMs = 0.0;
    overlayDebugSubmitMs = 0.0;
    overlayAuxFramesMs = 0.0;
    overlayDetectorCount = 0;
    overlayGeometryCount = 0;
    overlayContactCount = 0;
    overlayNearestCount = 0;
    overlayPrimitiveEstimate = 0;
    overlayLineEstimate = 0;
}

ProjectSceneCollisionPresentationSystem::State&
ProjectSceneCollisionPresentationSystem::state()
{
    return m_state;
}

const ProjectSceneCollisionPresentationSystem::State&
ProjectSceneCollisionPresentationSystem::state() const
{
    return m_state;
}

std::string ProjectSceneCollisionPresentationSystem::visibleVariantId(
    const std::string& robotId,
    const std::string& linkName) const
{
    const auto it =
        m_state.visibleVariantIds.find(collisionVariantSelectionKey(robotId, linkName));
    return it != m_state.visibleVariantIds.end() ? it->second : std::string();
}

void ProjectSceneCollisionPresentationSystem::invalidateGeometryOverlayCache()
{
    m_state.geometryOverlayCache.clear();
    ++m_state.geometryOverlayCacheVersion;
}

void ProjectSceneCollisionPresentationSystem::resetForProjectDocument()
{
    m_state.visibleVariantIds.clear();
    m_state.visibleVariantFilters.clear();
    m_state.visibleVariantFiltersByRuntimeLink.clear();
    m_state.queriesEnabled = false;
    m_state.showGeometry = false;
    m_state.runtimeBuilt = false;
    m_state.runtimeBuildInProgress = false;
    m_state.scene = collision::CollisionScene();
    invalidateGeometryOverlayCache();
}

void ProjectSceneCollisionPresentationSystem::beginObjectPreview(
    const std::string& objectId,
    const std::string& variantId)
{
    m_state.objectPreviewActive = true;
    m_state.objectPreviewObjectId = objectId;
    m_state.objectPreviewVariantId = variantId;
}

void ProjectSceneCollisionPresentationSystem::clearObjectPreview()
{
    m_state.objectPreviewActive = false;
    m_state.objectPreviewObjectId.clear();
    m_state.objectPreviewVariantId.clear();
}

