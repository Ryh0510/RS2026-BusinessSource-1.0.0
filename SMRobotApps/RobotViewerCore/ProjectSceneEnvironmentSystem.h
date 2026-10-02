#pragma once

#include "ProjectSceneEnvironment.h"

#include <cstddef>

namespace scenecore
{
    class DebugDraw;
}

class ProjectSceneEnvironmentSystem
{
public:
    void setPreset(ProjectSceneEnvironmentPreset preset) { m_preset = preset; }
    ProjectSceneEnvironmentPreset preset() const { return m_preset; }
    bool hasGround() const { return m_preset != ProjectSceneEnvironmentPreset::None; }
    bool showsGrid() const { return m_preset != ProjectSceneEnvironmentPreset::Home; }
    void setSceneScale(float scale);
    float sceneScale() const { return m_sceneScale; }

    void submit(scenecore::DebugDraw& draw) const;
    std::size_t primitiveCount() const;

private:
    ProjectSceneEnvironmentPreset m_preset = ProjectSceneEnvironmentPreset::Factory;
    float m_sceneScale = 1.0f;
};
