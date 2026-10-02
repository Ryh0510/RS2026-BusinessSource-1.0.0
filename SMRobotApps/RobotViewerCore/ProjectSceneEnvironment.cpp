#include "ProjectSceneEnvironment.h"

#include <algorithm>
#include <cctype>

const char* projectSceneEnvironmentPresetId(ProjectSceneEnvironmentPreset preset)
{
    switch(preset) {
    case ProjectSceneEnvironmentPreset::None: return "none";
    case ProjectSceneEnvironmentPreset::Studio: return "studio";
    case ProjectSceneEnvironmentPreset::Factory: return "factory";
    case ProjectSceneEnvironmentPreset::Workshop: return "workshop";
    case ProjectSceneEnvironmentPreset::Home: return "home";
    default: return "factory";
    }
}

bool parseProjectSceneEnvironmentPreset(
    const std::string& id,
    ProjectSceneEnvironmentPreset& preset)
{
    std::string normalized = id;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });

    for(ProjectSceneEnvironmentPreset candidate : {
        ProjectSceneEnvironmentPreset::None,
        ProjectSceneEnvironmentPreset::Studio,
        ProjectSceneEnvironmentPreset::Factory,
        ProjectSceneEnvironmentPreset::Workshop,
        ProjectSceneEnvironmentPreset::Home }) {
        if(normalized == projectSceneEnvironmentPresetId(candidate)) {
            preset = candidate;
            return true;
        }
    }
    return false;
}
