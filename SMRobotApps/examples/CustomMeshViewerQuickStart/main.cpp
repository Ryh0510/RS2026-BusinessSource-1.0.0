#include <RobotViewport.h>

#include <SimulationProject/ProjectDocument.h>
#include <VisualizationSDK/CustomMesh.h>

#include <QApplication>
#include <QEventLoop>
#include <QSurfaceFormat>
#include <QTimer>

#include <filesystem>
#include <iostream>
#include <string>

namespace
{
    void processEvents()
    {
        QEventLoop loop;
        QTimer::singleShot(80, &loop, &QEventLoop::quit);
        loop.exec();
    }

    bool require(
        const smrobot::visualization::CustomMeshResult& result,
        const char* operation)
    {
        if(result.success) {
            return true;
        }
        std::cerr << operation << " failed: " << result.message << '\n';
        return false;
    }
}

int main(int argc, char** argv)
{
    bool interactive = false;
    for(int index = 1; index < argc; ++index) {
        interactive = interactive || std::string(argv[index]) == "--interactive";
    }

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argc, argv);
    RobotViewport viewport;
    viewport.setWindowTitle(QStringLiteral("SMRobot Custom Mesh Demo"));
    viewport.resize(720, 520);
    viewport.show();
    processEvents();

    simulation_project::ProjectDocument document;
    document.view.showGrid = true;
    if(!viewport.loadProjectDocument(document, std::filesystem::current_path())) {
        std::cerr << "Viewport initialization failed.\n";
        return 1;
    }
    processEvents();

    using namespace smrobot::visualization;
    CustomMeshDesc plane;
    plane.ownerId = "quickstart";
    plane.meshId = "plane";
    PlaneMeshSpec planeSpec;
    planeSpec.width = 2.0f;
    planeSpec.height = 1.4f;
    planeSpec.segmentsX = 3;
    planeSpec.segmentsY = 2;
    if(!require(makePlaneMesh(planeSpec, plane.mesh), "makePlaneMesh")) {
        return 1;
    }
    plane.mesh.colors.assign(
        plane.mesh.positions.size(),
        Color4f{ 0.15f, 0.65f, 0.95f, 0.55f });
    plane.appearance.shading = MeshShadingMode::Unlit;
    plane.appearance.blend = MeshBlendMode::AlphaBlend;
    plane.appearance.cull = MeshCullMode::None;
    plane.appearance.renderOrder = 10;

    CustomMeshHandle planeHandle;
    if(!require(viewport.upsertCustomMesh(plane, planeHandle), "upsert plane")) {
        return 1;
    }

    HeightFieldMeshSpec heightField;
    heightField.columns = 4;
    heightField.rows = 4;
    heightField.spacingX = 0.35f;
    heightField.spacingY = 0.35f;
    heightField.heights = {
        0.0f, 0.05f, 0.05f, 0.0f,
        0.05f, 0.25f, 0.25f, 0.05f,
        0.05f, 0.25f, 0.25f, 0.05f,
        0.0f, 0.05f, 0.05f, 0.0f };
    heightField.colors.assign(16, Color4f{ 0.95f, 0.35f, 0.1f, 0.7f });
    CustomMeshDesc surface;
    surface.ownerId = plane.ownerId;
    surface.meshId = "surface";
    surface.appearance = plane.appearance;
    surface.appearance.renderOrder = 20;
    surface.transform.columnMajor[13] = 1.1f;
    if(!require(makeHeightFieldMesh(heightField, surface.mesh), "makeHeightFieldMesh")) {
        return 1;
    }
    CustomMeshHandle surfaceHandle;
    if(!require(viewport.upsertCustomMesh(surface, surfaceHandle), "upsert surface")) {
        return 1;
    }

    TransformMatrix moved;
    moved.columnMajor[12] = -0.4f;
    moved.columnMajor[14] = 0.35f;
    if(!require(viewport.setCustomMeshTransform(planeHandle, moved), "move plane") ||
        !require(viewport.setCustomMeshVisible(surfaceHandle, false), "hide surface") ||
        !require(viewport.setCustomMeshVisible(surfaceHandle, true), "show surface")) {
        return 1;
    }
    processEvents();

    if(interactive) {
        return application.exec();
    }

    if(!require(viewport.removeCustomMesh(planeHandle), "remove plane") ||
        !require(viewport.clearCustomMeshes(surface.ownerId), "clear owner")) {
        return 1;
    }

    std::cout << "Custom mesh viewer quick start passed.\n";
    return 0;
}
