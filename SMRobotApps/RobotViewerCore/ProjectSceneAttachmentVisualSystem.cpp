#include "ProjectSceneAttachmentVisualSystem.h"

#include <algorithm>

void ProjectSceneAttachmentVisualSystem::useMountedGraph(
    const simulation_runtime::RuntimeMountedAttachmentGraph& graph)
{
    m_mountedGraph = &graph;
}

void ProjectSceneAttachmentVisualSystem::usePreviewGraph()
{
    m_mountedGraph = &m_previewGraph;
}

simulation_runtime::RuntimeMountedAttachmentGraph&
ProjectSceneAttachmentVisualSystem::previewGraph()
{
    return m_previewGraph;
}

const simulation_runtime::RuntimeMountedAttachmentGraph&
ProjectSceneAttachmentVisualSystem::mountedGraph() const
{
    return m_mountedGraph != nullptr ? *m_mountedGraph : m_previewGraph;
}

std::vector<RuntimeToolAttachmentVisual>& ProjectSceneAttachmentVisualSystem::visuals()
{
    return m_visuals;
}

const std::vector<RuntimeToolAttachmentVisual>& ProjectSceneAttachmentVisualSystem::visuals() const
{
    return m_visuals;
}

void ProjectSceneAttachmentVisualSystem::resetVisuals()
{
    m_visuals.clear();
    m_activeIndex = static_cast<std::size_t>(-1);
}

void ProjectSceneAttachmentVisualSystem::selectInitialActive(
    std::size_t firstEnabled,
    std::size_t firstVisible)
{
    m_activeIndex = firstEnabled != static_cast<std::size_t>(-1)
        ? firstEnabled
        : firstVisible;
}

bool ProjectSceneAttachmentVisualSystem::cycleActive(bool reverse)
{
    if(m_visuals.empty()) {
        return false;
    }

    const std::size_t count = m_visuals.size();
    std::size_t index = m_activeIndex < count ? m_activeIndex : 0;
    for(std::size_t step = 0; step < count; ++step) {
        index = reverse
            ? (index == 0 ? count - 1 : index - 1)
            : (index + 1) % count;
        if(m_visuals[index].visible) {
            m_activeIndex = index;
            return true;
        }
    }
    return false;
}

bool ProjectSceneAttachmentVisualSystem::setActive(const std::string& id)
{
    if(id.empty()) {
        const bool changed = m_activeIndex < m_visuals.size();
        m_activeIndex = static_cast<std::size_t>(-1);
        return changed;
    }

    const auto it = std::find_if(
        m_visuals.begin(),
        m_visuals.end(),
        [&](const RuntimeToolAttachmentVisual& visual) {
            return visual.documentId == id && visual.visible;
        });
    if(it == m_visuals.end()) {
        return false;
    }
    m_activeIndex = static_cast<std::size_t>(std::distance(m_visuals.begin(), it));
    return true;
}

const RuntimeToolAttachmentVisual*
ProjectSceneAttachmentVisualSystem::activeToolFrameAttachment() const
{
    if(m_activeToolFrameRobotId.empty() || m_activeIndex >= m_visuals.size()) {
        return nullptr;
    }
    const RuntimeToolAttachmentVisual& attachment = m_visuals[m_activeIndex];
    if(attachment.robotId != m_activeToolFrameRobotId || !attachment.visible) {
        return nullptr;
    }
    return &attachment;
}

std::size_t ProjectSceneAttachmentVisualSystem::activeIndex() const
{
    return m_activeIndex;
}

void ProjectSceneAttachmentVisualSystem::setActiveToolFrameRobot(const std::string& robotId)
{
    m_activeToolFrameRobotId = robotId;
}

const std::string& ProjectSceneAttachmentVisualSystem::activeToolFrameRobot() const
{
    return m_activeToolFrameRobotId;
}

void ProjectSceneAttachmentVisualSystem::setToolFrameVisibility(
    const ProjectScene::ToolFrameVisibility& visibility)
{
    m_toolFrameVisibility = visibility;
}

const ProjectScene::ToolFrameVisibility&
ProjectSceneAttachmentVisualSystem::toolFrameVisibility() const
{
    return m_toolFrameVisibility;
}

void ProjectSceneAttachmentVisualSystem::applyVisibility(bool sceneFocusActive)
{
    for(RuntimeToolAttachmentVisual& attachment : m_visuals) {
        robot_render::MountedAttachmentVisualBridge::setVisible(
            attachment.visual,
            attachment.visible && !sceneFocusActive);
    }
}

void ProjectSceneAttachmentVisualSystem::setPreview(
    const simulation_project::AttachmentAssetDesc& asset,
    const std::filesystem::path& basePath)
{
    m_preview = RuntimeToolAssetPreview();
    m_preview.asset = asset;
    m_preview.basePath = basePath;
    m_hasPreview = true;
}

void ProjectSceneAttachmentVisualSystem::clearPreview()
{
    m_preview = RuntimeToolAssetPreview();
    m_hasPreview = false;
}

bool ProjectSceneAttachmentVisualSystem::hasPreview() const
{
    return m_hasPreview;
}

RuntimeToolAssetPreview& ProjectSceneAttachmentVisualSystem::preview()
{
    return m_preview;
}

const RuntimeToolAssetPreview& ProjectSceneAttachmentVisualSystem::preview() const
{
    return m_preview;
}
