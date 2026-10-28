#pragma once

#include "ProjectSceneEnvironment.h"
#include <functional>
#include <memory>
#include <glm/glm.hpp>

namespace scenecore { class SceneGraph; }

// Presentation-only model ownership; callers provide a current GL context.
class ProjectSceneModelEnvironment
{
public:
    ProjectSceneModelEnvironment();
    ~ProjectSceneModelEnvironment();
    bool load(const ProjectSceneEnvironmentSelection& selection,
        scenecore::SceneGraph& graph, std::string& error);
    void clear(scenecore::SceneGraph& graph);
    void withView(const glm::vec3& camera, bool cutaway,
        const std::function<void()>& render);
    std::size_t partCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
