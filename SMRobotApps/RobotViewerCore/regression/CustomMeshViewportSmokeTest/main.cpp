#include <RobotViewport.h>

#include <SimulationProject/ProjectDocument.h>
#include <VisualizationSDK/CustomMesh.h>

#include <QApplication>
#include <QEventLoop>
#include <QImage>
#include <QSurfaceFormat>
#include <QTimer>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
    void processEvents(int milliseconds = 80)
    {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    }

    int fail(const smrobot::visualization::CustomMeshResult& result, const char* operation)
    {
        std::cerr << operation << " failed: " << result.message << '\n';
        return 1;
    }
}

int main(int argc, char** argv)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application(argc, argv);
    RobotViewport viewport;
    viewport.resize(640, 480);
    viewport.show();
    processEvents();

    simulation_project::ProjectDocument document;
    document.view.showGrid = true;
    document.view.showAxis = true;
    const std::filesystem::path objectPath =
        std::filesystem::temp_directory_path() / "robot_viewer_visibility_smoke.obj";
    {
        std::ofstream objectFile(objectPath);
        objectFile << "v -0.5 -0.5 0\n";
        objectFile << "v 0.5 -0.5 0\n";
        objectFile << "v 0.0 0.5 0\n";
        objectFile << "f 1 2 3\n";
    }
    simulation_project::SceneObjectDesc object;
    object.id = "visibility-smoke-object";
    object.name = "Visibility smoke object";
    object.sourcePath = objectPath.string();
    document.objects.push_back(object);
    if(!viewport.loadProjectDocument(document, std::filesystem::current_path())) {
        std::cerr << "Viewport scene initialization failed.\n";
        return 1;
    }
    processEvents();

    bool objectVisible = false;
    const QString objectId = QString::fromStdString(object.id);
    if(!viewport.sceneObjectVisible(objectId, objectVisible) || !objectVisible ||
        !viewport.setSceneObjectVisibilityOverride(objectId, false)) {
        std::cerr << "Scene object visibility override setup failed.\n";
        std::filesystem::remove(objectPath);
        return 1;
    }
    // resetCamera reapplies the complete focus/document visibility calculation.
    // The runtime override must still win after that calculation.
    viewport.resetCamera();
    processEvents();
    if(!viewport.sceneObjectVisible(objectId, objectVisible) || objectVisible ||
        !viewport.clearSceneObjectVisibilityOverride(objectId) ||
        !viewport.sceneObjectVisible(objectId, objectVisible) || !objectVisible) {
        std::cerr << "Scene object visibility override did not persist or restore.\n";
        std::filesystem::remove(objectPath);
        return 1;
    }

    using namespace smrobot::visualization;
    CustomMeshDesc plane;
    plane.ownerId = "custom-mesh-smoke";
    plane.meshId = "plane";
    plane.name = "Transparent plane";
    PlaneMeshSpec planeSpec;
    planeSpec.width = 1.8f;
    planeSpec.height = 1.2f;
    planeSpec.segmentsX = 2;
    planeSpec.segmentsY = 2;
    CustomMeshResult result = makePlaneMesh(planeSpec, plane.mesh);
    if(!result.success) {
        return fail(result, "makePlaneMesh");
    }
    plane.mesh.colors.assign(plane.mesh.positions.size(), Color4f{ 0.1f, 0.65f, 0.95f, 0.55f });
    plane.appearance.baseColor = { 1.0f, 1.0f, 1.0f, 0.8f };
    plane.appearance.blend = MeshBlendMode::AlphaBlend;
    plane.appearance.cull = MeshCullMode::None;
    plane.appearance.shading = MeshShadingMode::Unlit;
    plane.appearance.renderOrder = 10;

    CustomMeshHandle planeHandle;
    result = viewport.upsertCustomMesh(plane, planeHandle);
    if(!result.success || !planeHandle.valid()) {
        return fail(result, "upsertCustomMesh(plane)");
    }
    CustomMeshHandle repeatedHandle;
    result = viewport.upsertCustomMesh(plane, repeatedHandle);
    if(!result.success || repeatedHandle.value != planeHandle.value) {
        std::cerr << "Custom mesh upsert did not preserve its handle.\n";
        return 1;
    }

    HeightFieldMeshSpec heightField;
    heightField.columns = 3;
    heightField.rows = 3;
    heightField.spacingX = 0.45f;
    heightField.spacingY = 0.45f;
    heightField.heights = {
        0.0f, 0.1f, 0.0f,
        0.1f, 0.35f, 0.1f,
        0.0f, 0.1f, 0.0f };
    heightField.colors.assign(9, Color4f{ 0.95f, 0.3f, 0.15f, 0.7f });
    CustomMeshDesc surface;
    surface.ownerId = plane.ownerId;
    surface.meshId = "height-field";
    surface.appearance = plane.appearance;
    surface.appearance.renderOrder = 20;
    surface.transform.columnMajor[13] = 1.0f;
    result = makeHeightFieldMesh(heightField, surface.mesh);
    if(!result.success) {
        return fail(result, "makeHeightFieldMesh");
    }
    CustomMeshHandle surfaceHandle;
    result = viewport.upsertCustomMesh(surface, surfaceHandle);
    if(!result.success) {
        return fail(result, "upsertCustomMesh(height-field)");
    }

    std::vector<Color4f> colors(plane.mesh.positions.size(), Color4f{ 0.2f, 0.9f, 0.35f, 0.45f });
    result = viewport.updateCustomMeshColors(planeHandle, colors);
    if(!result.success) {
        return fail(result, "updateCustomMeshColors");
    }
    TransformMatrix transform;
    transform.columnMajor[12] = -0.5f;
    transform.columnMajor[14] = 0.4f;
    result = viewport.setCustomMeshTransform(planeHandle, transform);
    if(!result.success) {
        return fail(result, "setCustomMeshTransform");
    }
    result = viewport.setCustomMeshVisible(surfaceHandle, false);
    if(!result.success) {
        return fail(result, "setCustomMeshVisible(false)");
    }
    result = viewport.setCustomMeshVisible(surfaceHandle, true);
    if(!result.success) {
        return fail(result, "setCustomMeshVisible(true)");
    }

    processEvents();
    const QImage framebuffer = viewport.grabFramebuffer();
    if(framebuffer.isNull() || framebuffer.width() != viewport.width() ||
        framebuffer.height() != viewport.height()) {
        std::cerr << "Custom mesh viewport did not produce a framebuffer.\n";
        return 1;
    }

    result = viewport.removeCustomMesh(planeHandle);
    if(!result.success) {
        return fail(result, "removeCustomMesh");
    }
    result = viewport.clearCustomMeshes(surface.ownerId);
    if(!result.success) {
        return fail(result, "clearCustomMeshes");
    }
    if(viewport.removeCustomMesh(surfaceHandle).error != CustomMeshError::NotFound) {
        std::cerr << "Removed custom mesh handle did not report NotFound.\n";
        return 1;
    }

    std::filesystem::remove(objectPath);

    std::cout << "Custom mesh viewport smoke passed.\n";
    return 0;
}
