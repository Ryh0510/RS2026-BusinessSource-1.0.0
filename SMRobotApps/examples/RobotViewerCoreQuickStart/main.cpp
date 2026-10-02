#include <ProjectScene.h>
#include <ProjectScenePickingService.h>

#include <iostream>
#include <vector>

int main()
{
    ProjectScene scene;
    ProjectScenePickRay ray;
    const auto result = ProjectScenePickingService::pick(
        ray,
        std::vector<ProjectScenePickCandidate>{},
        ProjectSceneInteractionMode::Browse);
    if(result.valid() || scene.isInitialized()) {
        std::cerr << "Unexpected initialized RobotViewerCore state." << std::endl;
        return 1;
    }

    std::cout << "RobotViewerCore quick start passed." << std::endl;
    return 0;
}
