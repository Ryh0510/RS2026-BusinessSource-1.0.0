#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

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

enum class ProjectSceneEnvironmentPartRole { Solid, Floor, Wall, Roof };

struct ProjectSceneEnvironmentPart
{
    std::string id;
    std::string source;
    ProjectSceneEnvironmentPartRole role = ProjectSceneEnvironmentPartRole::Solid;
    glm::vec3 wallNormal{0.0f};
    float wallOffset = 0.0f;
};

struct ProjectSceneEnvironmentSelection
{
    ProjectSceneEnvironmentPreset preset = ProjectSceneEnvironmentPreset::Factory;
    std::string id;
    std::string source;
    std::vector<ProjectSceneEnvironmentPart> parts;
    float scale = 1.0f;
    float groundHeight = 0.0f;
    bool hasGround = false;
    bool showGrid = false;
    bool cutaway = true;
    bool isModel() const { return !parts.empty(); }
};

bool validateProjectSceneEnvironment(const ProjectSceneEnvironmentSelection& selection, std::string& error);
