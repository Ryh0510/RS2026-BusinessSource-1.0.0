#include "ProjectSceneEnvironment.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>

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

bool validateProjectSceneEnvironment(const ProjectSceneEnvironmentSelection& selection, std::string& error)
{
    error.clear();
    if(!std::isfinite(selection.scale) || selection.scale <= 0.0f || selection.scale > 1000.0f ||
        !std::isfinite(selection.groundHeight) || selection.parts.size() > 128) {
        error = "Invalid environment scale, ground height or part count.";
        return false;
    }
    std::unordered_set<std::string> ids;
    for(const auto& part : selection.parts) {
        if(part.id.empty() || part.source.empty() || !ids.insert(part.id).second) {
            error = "Environment part requires a unique ID and model source.";
            return false;
        }
        if(part.role == ProjectSceneEnvironmentPartRole::Wall &&
            (!std::isfinite(part.wallOffset) || !std::isfinite(glm::length(part.wallNormal)) ||
             std::abs(glm::length(part.wallNormal) - 1.0f) > 0.001f)) {
            error = "Environment wall requires a finite plane and unit normal.";
            return false;
        }
    }
    return true;
}
