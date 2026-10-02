#pragma once

#include <string>

enum class ProjectSceneEnvironmentPreset
{
    None,
    Studio,
    Factory,
    Workshop,
    Home
};

const char* projectSceneEnvironmentPresetId(ProjectSceneEnvironmentPreset preset);
bool parseProjectSceneEnvironmentPreset(
    const std::string& id,
    ProjectSceneEnvironmentPreset& preset);
