#include <ProjectSimulationSDK/ProjectSimulationSdk.h>

#include <iostream>
#include <memory>

namespace
{
    const char* entityKindName(project_simulation_sdk::ProjectEntityKind kind)
    {
        using project_simulation_sdk::ProjectEntityKind;

        switch(kind) {
        case ProjectEntityKind::Robot:
            return "Robot";
        case ProjectEntityKind::SceneObject:
            return "SceneObject";
        case ProjectEntityKind::PointCloud:
            return "PointCloud";
        case ProjectEntityKind::RobotMount:
            return "RobotMount";
        case ProjectEntityKind::AttachmentAsset:
            return "AttachmentAsset";
        case ProjectEntityKind::MountedAttachment:
            return "MountedAttachment";
        }

        return "Unknown";
    }

    void printEntities(
        const project_simulation_sdk::IProjectSimulationSession& session,
        project_simulation_sdk::ProjectEntityKind kind)
    {
        const std::vector<project_simulation_sdk::ProjectEntityInfo> entities = session.entities(kind);
        std::cout << entityKindName(kind) << " count: " << entities.size() << '\n';
        for(const project_simulation_sdk::ProjectEntityInfo& entity : entities) {
            std::cout << "  " << entity.id << " | " << entity.name << " | "
                      << entity.type << " | " << entity.sourcePath << '\n';
        }
    }
}

int main(int argc, char** argv)
{
    std::cout << "ProjectSimulationSDK ABI: "
              << project_simulation_sdk::getProjectSimulationSdkAbiVersion() << '\n';

    std::unique_ptr<project_simulation_sdk::IProjectSimulationSession> session =
        project_simulation_sdk::createProjectSimulationSession();
    if(!session) {
        std::cerr << "Failed to create ProjectSimulationSDK session.\n";
        return 1;
    }

    if(argc < 2) {
        std::cout << "No project path provided; session creation smoke test passed.\n";
        return 0;
    }

    const project_simulation_sdk::Result loadResult = session->loadProjectFile(argv[1]);
    if(!loadResult.success) {
        std::cerr << "loadProjectFile failed: " << loadResult.message << '\n';
        return 2;
    }

    const project_simulation_sdk::Result validateResult = session->validate();
    if(!validateResult.success) {
        std::cerr << "validate failed: " << validateResult.message << '\n';
        return 3;
    }

    const project_simulation_sdk::ProjectSummary& summary = session->summary();
    std::cout << "Project: " << summary.projectPath << '\n';
    std::cout << "Schema: " << summary.schemaVersion << '\n';
    std::cout << "Robots: " << summary.robotCount << '\n';
    std::cout << "Objects: " << summary.objectCount << '\n';
    std::cout << "Point clouds: " << summary.pointCloudCount << '\n';
    std::cout << "Collision detectors: " << summary.collisionDetectorCount << '\n';

    printEntities(*session, project_simulation_sdk::ProjectEntityKind::Robot);
    printEntities(*session, project_simulation_sdk::ProjectEntityKind::SceneObject);

    return 0;
}
