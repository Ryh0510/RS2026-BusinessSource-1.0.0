#pragma once

#include "ProjectCollisionRuntimeProjection.h"
#include "ProjectRuntimeTypes.h"

#include <Collision/CollisionDebugDrawBuilder.h>
#include <Collision/CollisionScene.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scenecore {
class MeshPass;
class PrimitivePass;
class TrajectoryPass;
}

class ProjectSceneCollisionPresentationSystem
{
public:
    struct VisibleVariantFilter
    {
        std::string robotId;
        std::string linkName;
        std::string variantId;
        std::unordered_set<std::string> elementNames;
    };

    struct GeometryOverlayCache
    {
        bool valid = false;
        std::uint64_t version = 0;
        std::string key;
        collision::CollisionDebugDrawData data;

        void clear();
    };

    struct ObjectVariantMeshPreview
    {
        std::shared_ptr<VisibleModelNode> node;
        glm::mat4 targetLocal = glm::mat4(1.0f);
        bool configured = false;
    };

    struct ObjectVariantMeshPreviewCache
    {
        std::vector<ObjectVariantMeshPreview> previews;
        std::uint64_t revision = 0;
    };

    struct FrameMetrics
    {
        double worldUpdateMs = 0.0;
        double overlayMs = 0.0;
        double overlayHighlightMs = 0.0;
        double overlayDebugBuildMs = 0.0;
        double overlayVariantFilterMs = 0.0;
        double overlayDebugSubmitMs = 0.0;
        double overlayAuxFramesMs = 0.0;
        std::size_t overlayDetectorCount = 0;
        std::size_t overlayGeometryCount = 0;
        std::size_t overlayContactCount = 0;
        std::size_t overlayNearestCount = 0;
        std::size_t overlayPrimitiveEstimate = 0;
        std::size_t overlayLineEstimate = 0;

        void resetOverlay();
    };

    struct State
    {
        bool showGeometry = false;
        bool reportedCollision = false;
        std::size_t lastContactCount = static_cast<std::size_t>(-1);
        std::size_t lastIncludePairCount = static_cast<std::size_t>(-1);
        std::uint64_t queryFrame = 0;
        FrameMetrics frameMetrics;
        std::shared_ptr<scenecore::MeshPass> meshPass;
        std::shared_ptr<scenecore::PrimitivePass> primitivePass;
        std::shared_ptr<scenecore::TrajectoryPass> linePass;
        collision::CollisionScene scene;
        std::vector<ProjectCollisionDetectorViewRuntime> detectors;
        bool queriesEnabled = false;
        bool runtimeBuilt = false;
        bool runtimeBuildInProgress = false;
        std::string activeDetectorId;
        std::unordered_map<std::string, std::string> visibleVariantIds;
        std::unordered_map<std::string, VisibleVariantFilter> visibleVariantFilters;
        std::unordered_map<std::string, VisibleVariantFilter> visibleVariantFiltersByRuntimeLink;
        GeometryOverlayCache geometryOverlayCache;
        std::uint64_t geometryOverlayCacheVersion = 1;
        bool objectPreviewActive = false;
        std::string objectPreviewObjectId;
        std::string objectPreviewVariantId;
        std::unordered_map<std::string, ObjectVariantMeshPreviewCache> objectVariantMeshPreviews;
        std::uint64_t objectVariantMeshPreviewRevision = 1;
    };

    State& state();
    const State& state() const;

    std::string visibleVariantId(
        const std::string& robotId,
        const std::string& linkName) const;
    void invalidateGeometryOverlayCache();
    void resetForProjectDocument();
    void beginObjectPreview(
        const std::string& objectId,
        const std::string& variantId);
    void clearObjectPreview();

private:
    State m_state;
};

