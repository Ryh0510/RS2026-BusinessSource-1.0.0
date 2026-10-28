#include "../../RobotQtViewerEnvironmentController.h"
#include <ProjectSceneModelEnvironment.h>
#include <ProjectSceneCameraSystem.h>
#include <SimulationRuntime/RuntimeMountedAttachment.h>
#include <SceneCore/Renderer.h>
#include <SceneCore/RenderPass/MeshPass.h>
#include <QJsonArray>
#include <RobotViewport.h>
#include <RobotQtViewerLocalization.h>
#include <RobotQtViewerOperationStatus.h>
#include <RobotQtViewerEventHub.h>
#include <QAction>
#include <QMenu>
#include <QSettings>
#include <QFileDialog>
#include <QMouseEvent>
#include <QWheelEvent>
#include <SimulationProject/ProjectIo.h>
#include <AssetCore/AssimpMeshLoader.h>
#include <AssetCore/ModelDesc.h>
#include <RenderCore/Material.h>
#include <RenderCore/Shader.h>
#include <QOpenGLFramebufferObject>
#include <QImage>
#include <cmath>
#include <array>
#include <RenderCore/TextureManager.h>
#include <SceneCore/SceneGraph.h>
#include <SceneCore/RenderQueue.h>
#include <SimulationProject/ProjectDocument.h>
#include <glad/glad.h>
#include <QApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTimer>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool ok, const char* message) {
    std::cout << (ok ? "[OK] " : "[FAIL] ") << message << '\n';
    if(!ok) ++failures;
}
void events() {
    QEventLoop loop;
    QTimer::singleShot(60, &loop, &QEventLoop::quit);
    loop.exec();
}
scenecore::RenderQueue collect(scenecore::SceneGraph& graph) {
    graph.update();
    scenecore::RenderQueue queue;
    graph.root()->collect(queue);
    return queue;
}
void verifyShader(const QString& shaderRoot, const QString& fixtureRoot) {
    QFile fragmentFile(shaderRoot + "/MLRobotUBO.frag");
    QFile commonFile(shaderRoot + "/common/scene_ubo.glsl");
    check(fragmentFile.open(QIODevice::ReadOnly) && commonFile.open(QIODevice::ReadOnly), "production shader source available");
    QByteArray fragment = fragmentFile.readAll();
    fragment.replace("#include \"common/scene_ubo.glsl\"", commonFile.readAll());
    const int mainIndex = fragment.lastIndexOf("void main()");
    check(mainIndex > 0, "production material helpers found");
    if(mainIndex < 0) return;
    // Measure production helpers directly; do not duplicate the channel math under test.
    fragment = fragment.left(mainIndex) + "void main(){ FragColor=vec4(getMetallic(),getRoughness(),getNormal().z,1); }";
    const std::string vertex = R"(#version 420 core
out vec3 vWorldPos; out vec3 vNormal; out vec2 vTexCoord; out vec4 vVertexColor;
void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
vTexCoord=p; vWorldPos=vec3(p*2-1,0); vNormal=vec3(0,0,1); vVertexColor=vec4(1);
gl_Position=vec4(p*2-1,0,1); })";
    auto shader = rendercore::Shader::fromSources("environment-material-contract", vertex, fragment.toStdString());
    GLint linked = 0;
    glGetProgramiv(shader->id(), GL_LINK_STATUS, &linked);
    check(linked != 0, "production material helpers compile and link");
    QImage mr(2, 2, QImage::Format_RGBA8888);
    mr.fill(QColor(25, 160, 224));
    QImage normal(2, 2, QImage::Format_RGBA8888);
    normal.fill(QColor(255, 128, 128));
    const QString mrPath = fixtureRoot + "/known-mr.png", normalPath = fixtureRoot + "/known-normal.png";
    check(mr.save(mrPath) && normal.save(normalPath), "known-pixel material fixtures saved");
    rendercore::Material material;
    material.type = rendercore::MaterialType::PBR;
    material.metallic = 0.4f;
    material.roughness = 0.75f;
    auto& textures = rendercore::TextureManager::instance();
    material.textures[rendercore::MaterialTextureSlot::MetallicRoughness] = textures.load(mrPath.toUtf8().toStdString());
    material.textures[rendercore::MaterialTextureSlot::Normal] = textures.load(normalPath.toUtf8().toStdString());
    QOpenGLFramebufferObject framebuffer(8, 8);
    framebuffer.bind();
    glViewport(0, 0, 8, 8);
    glDisable(GL_DEPTH_TEST);
    unsigned int vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    auto pixel = [&]() {
        shader->use();
        material.apply(*shader);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::array<float, 4> result{};
        glReadPixels(4, 4, 1, 1, GL_RGBA, GL_FLOAT, result.data());
        return result;
    };
    auto result = pixel();
    check(std::abs(result[0] - 224.0f/255*0.4f) < 0.01f &&
        std::abs(result[1] - 160.0f/255*0.75f) < 0.01f, "real shader samples B metallic and G roughness with factors");
    check(result[2] < 0.02f, "normal texture changes derivative TBN result");
    material.textures.erase(rendercore::MaterialTextureSlot::Normal);
    material.textures.erase(rendercore::MaterialTextureSlot::MetallicRoughness);
    result = pixel();
    check(std::abs(result[0] - 0.4f) < 0.01f && std::abs(result[1] - 0.75f) < 0.01f && result[2] > 0.99f,
        "missing texture flags reset to factors and geometry normal");
    auto gray = textures.load(mrPath.toUtf8().toStdString());
    material.textures[rendercore::MaterialTextureSlot::Metallic] = gray;
    material.textures[rendercore::MaterialTextureSlot::Roughness] = gray;
    result = pixel();
    check(std::abs(result[0] - 25.0f/255) < 0.01f && std::abs(result[1] - 25.0f/255) < 0.01f,
        "legacy independent gray maps retain R-channel behavior");
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    framebuffer.release();
}
void verifyMountedCamera(scenecore::SceneGraph& graph, const QString& shaderRoot) {
    class CountingPass final : public scenecore::MeshPass {
    public:
        using MeshPass::MeshPass;
        std::size_t count = 0;
        void render(scenecore::RenderQueue& queue) override {
            count = queue.filteredItems(filter()).size();
            MeshPass::render(queue);
        }
    };
    ProjectSceneCameraSystem cameras;
    check(cameras.initialize(graph), "camera system initializes");
    scenecore::Renderer renderer;
    renderer.initialize();
    auto shader = std::make_shared<rendercore::Shader>((shaderRoot + "/MLRobotUBO.vert").toStdString(),
        (shaderRoot + "/MLRobotUBO.frag").toStdString());
    auto pass = std::make_shared<CountingPass>(shader);
    renderer.addPass(pass);
    simulation_runtime::RuntimeMountedCamera camera;
    camera.attachmentId = "environment-contract-camera";
    camera.worldOptical.translation() = Eigen::Vector3d(0, 0, 1.7);
    camera.worldOptical.linear() << 1,0,0, 0,0,1, 0,-1,0;
    camera.intrinsics.fovY = 60;
    camera.intrinsics.nearPlane = 0.05;
    camera.intrinsics.farPlane = 100;
    std::vector<unsigned char> pixels;
    std::string detail;
    graph.update();
    check(cameras.renderMountedCamera(camera, 160, 120, Eigen::Vector4f(0,0,0,1), graph, renderer, pixels, &detail),
        "actual mounted-camera render succeeds with grid disabled");
    check(pass->count == 39 && pixels.size() == 160*120*4, "mounted-camera filter submits all environment meshes once");
    std::size_t colored = 0;
    for(std::size_t i = 0; i + 3 < pixels.size(); i += 4)
        if(pixels[i] > 10 || pixels[i+1] > 10 || pixels[i+2] > 10) ++colored;
    check(colored > 10000, "mounted camera sees indoor background pixels");
}
int captureOrbit(const ProjectSceneEnvironmentSelection& selection, const fs::path& projectPath, const fs::path& output) {
    fs::create_directories(output);
    simulation_project::ProjectDocument document;
    std::string detail;
    check(simulation_project::loadProjectDocument(projectPath, document, &detail), "orbit loads real project document");
    RobotViewport viewport;
    viewport.resize(868, 719);
    check(viewport.loadProjectDocument(document, projectPath.parent_path()), "orbit queues real project before showing");
    viewport.show();
    events();
    check(viewport.environmentSceneInitialized(), "orbit builds real robot viewport");
    check(viewport.setEnvironmentSelection(selection, &detail), "orbit commits model environment");
    viewport.setCameraView(ProjectSceneCameraView::Isometric);
    const QPoint center = viewport.rect().center();
    QWheelEvent wheel(center, viewport.mapToGlobal(center), QPoint(), QPoint(0,-120*70),
        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&viewport, &wheel);
    QMouseEvent press(QEvent::MouseButtonPress, center, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &press);
    QPoint end = center;
    for(int i = 0; i <= 48; ++i) {
        end = center + QPoint(static_cast<int>(std::lround(i * 6.28318530718 / 48 / 0.004)), 0);
        QMouseEvent move(QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&viewport, &move);
        events();
        const auto frame = viewport.grabFramebuffer();
        const QString path = QString::fromUtf8((output / ("orbit-" + std::to_string(i) + ".png")).u8string().c_str());
        check(!frame.isNull() && frame.save(path), "continuous orbit frame renders and saves");
    }
    QMouseEvent release(QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &release);
    return failures == 0 ? 0 : 1;
}
void writeJson(const QString& path, const QJsonObject& object) {
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly)) throw std::runtime_error("fixture write failed");
    file.write(QJsonDocument(object).toJson());
}
}

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(4, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    if(argc < 3) return 2;
    QTemporaryDir settingsRoot;
    QCoreApplication::setOrganizationName("RS2026CandidateTests");
    QCoreApplication::setApplicationName("ModelEnvironment");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsRoot.path());
    const QString descriptor = QString::fromLocal8Bit(argv[1]);
    const fs::path assetRoot = fs::u8path(argv[2]);
    ProjectSceneEnvironmentSelection selection;
    QString error;
    check(robot_qt_viewer::readEnvironmentSelection(descriptor, selection, error), "real descriptor parses");
    if(!error.isEmpty()) std::cerr << error.toStdString() << '\n';
    check(selection.parts.size() == 7 && selection.scale == 1 && selection.hasGround, "seven meter-scale parts and ground");
    // Resolve fixtures explicitly so this test also works outside the source cwd.
    for(auto& part : selection.parts) part.source = (assetRoot / (part.id + ".gltf")).u8string();
    std::string detail;
    if(argc == 5) return captureOrbit(selection, fs::u8path(argv[3]), fs::u8path(argv[4]));
    auto invalid = selection;
    invalid.parts[1].id = invalid.parts[0].id;
    check(!validateProjectSceneEnvironment(invalid, detail), "duplicate ID rejected");
    invalid = selection;
    invalid.scale = std::numeric_limits<float>::quiet_NaN();
    check(!validateProjectSceneEnvironment(invalid, detail), "non-finite scale rejected");
    invalid = selection;
    invalid.parts[2].wallNormal = glm::vec3(2, 0, 0);
    check(!validateProjectSceneEnvironment(invalid, detail), "non-unit wall plane rejected");
    for(const auto* id : {"factory", "studio", "workshop", "home", "none"}) {
        ProjectSceneEnvironmentSelection old;
        check(robot_qt_viewer::readEnvironmentSelection(id, old, error) && !old.isModel() && old.source == id,
            "legacy preset remains a procedural source");
    }
    QTemporaryDir temporary;
    check(temporary.isValid(), "temporary fixtures available");
    QFile source(descriptor);
    source.open(QIODevice::ReadOnly);
    const auto original = QJsonDocument::fromJson(source.readAll()).object();
    auto object = original;
    object["version"] = 1.5;
    const QString bad = temporary.path() + "/bad.environment.json";
    writeJson(bad, object);
    check(!robot_qt_viewer::readEnvironmentSelection(bad, invalid, error), "fractional schema version rejected");
    object = original;
    object["showGrid"] = "false";
    writeJson(bad, object);
    check(!robot_qt_viewer::readEnvironmentSelection(bad, invalid, error), "string boolean rejected");
    object = original;
    object["units"] = "millimeters";
    writeJson(bad, object);
    check(!robot_qt_viewer::readEnvironmentSelection(bad, invalid, error), "wrong units rejected");

    QOpenGLContext context;
    context.setFormat(format);
    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    check(context.create() && context.makeCurrent(&surface), "real OpenGL context available");
    if(!QOpenGLContext::currentContext()) return 1;
    check(gladLoadGLLoader([](const char* name) -> void* {
        return reinterpret_cast<void*>(QOpenGLContext::currentContext()->getProcAddress(name));
    }) != 0, "GL functions loaded");
    {
        verifyShader(QString::fromUtf8((assetRoot / "../../../shader_gen2").u8string().c_str()), temporary.path());
        if(failures) return 1;
        // Unicode parent directory plus URI-escaped filenames exercise both external bin and PNG IO.
        const fs::path unicode = fs::u8path(temporary.path().toUtf8().toStdString()) / fs::u8path(u8"\u5de5\u5382 assets");
        fs::copy(assetRoot, unicode, fs::copy_options::recursive);
        assetcore::AssimpMeshLoader loader;
        auto cpu = loader.load((unicode / "interior.gltf").u8string(), 1);
        check(cpu && cpu->subMeshCount() == 10, "Unicode glTF directory loads external bin");
        bool normal = false, packed = false, escaped = false;
        if(cpu) for(const auto& material : cpu->materials()) {
            normal |= material.textures.count(assetcore::TextureSlot::Normal) != 0;
            packed |= material.textures.count(assetcore::TextureSlot::MetallicRoughness) != 0;
            for(const auto& texture : material.textures) {
                check(fs::is_regular_file(fs::u8path(texture.second)), "decoded texture URI resolves");
                if(texture.second.find("Ocean-blue enamel") != std::string::npos) escaped = true;
            }
        }
        check(normal && packed && escaped, "normal and packed MR preserved with decoded space URI");
        auto floor = loader.load((unicode / "floor.gltf").u8string(), 1);
        Eigen::Vector3f minimum = Eigen::Vector3f::Constant(1e6f), maximum = -minimum;
        if(floor) for(const auto& mesh : floor->subMeshes()) for(const auto& point : mesh.geometry.positions) {
            minimum = minimum.cwiseMin(point); maximum = maximum.cwiseMax(point);
        }
        check(floor && std::abs(minimum.x()+8) < 0.001f && std::abs(maximum.y()-8) < 0.001f &&
            std::abs(minimum.z()+0.24f) < 0.001f && maximum.z() < 0.03f,
            "real Blender floor loads 16m XY and Z-up once without rescaling");
        scenecore::SceneGraph graph;
        ProjectSceneModelEnvironment environment;
        check(environment.load(selection, graph, detail), "seven-part environment uploads");
        if(!detail.empty()) std::cerr << detail << '\n';
        auto queue = collect(graph);
        check(environment.partCount() == 7 && queue.items().size() == 39, "complete environment submits 39 meshes once");
        bool gpuNormal = false, gpuMR = false;
        for(const auto& item : queue.items()) {
            check(item.tag.category == scenecore::RenderCategory::Environment, "mesh carries environment category");
            if(!item.material) continue;
            for(const auto& texture : item.material->textures) {
                check(texture.second && texture.second->isValid(), "material texture uploaded");
                gpuNormal |= texture.first == rendercore::MaterialTextureSlot::Normal;
                gpuMR |= texture.first == rendercore::MaterialTextureSlot::MetallicRoughness;
            }
        }
        check(gpuNormal && gpuMR, "GPU normal and MR slots reachable");
        std::size_t cut = 0;
        environment.withView({20, 20, 8}, true, [&] {
            cut = collect(graph).items().size();
            environment.withView({20, 20, 8}, false, [&] {
                check(collect(graph).items().size() == 39, "secondary complete view sees every mesh");
            });
            check(collect(graph).items().size() == cut, "nested secondary view restores main cutaway");
        });
        check(cut > 0 && cut < 35 && collect(graph).items().size() == 39, "roof and facing walls cut only in view scope");
        verifyMountedCamera(graph, QString::fromUtf8((assetRoot / "../../../shader_gen2").u8string().c_str()));
        graph.removeNode(graph.getNode("/root/mainCameraNode"));
        graph.traverse([&](scenecore::SceneNode* node) { check(node->visible(), "view scopes preserve node visibility"); });
        auto previous = graph.getNode("/root/__presentation_environment");
        invalid = selection;
        invalid.parts.back().source = (assetRoot / "missing.gltf").u8string();
        check(!environment.load(invalid, graph, detail) && !detail.empty(), "missing final part fails");
        check(graph.root()->children().size() == 1 && graph.root()->children().front() == previous && collect(graph).items().size() == 39,
            "failed load preserves old scene");
        QFile floorFile(QString::fromUtf8((unicode / "floor.gltf").u8string().c_str()));
        floorFile.open(QIODevice::ReadOnly);
        const auto floorObject = QJsonDocument::fromJson(floorFile.readAll()).object();
        const QString rejectedPath = QString::fromUtf8((unicode / "rejected.gltf").u8string().c_str());
        auto broken = floorObject;
        auto images = broken["images"].toArray();
        auto image = images[0].toObject();
        QFile png(QString::fromUtf8((unicode / "Epoxy-base.png").u8string().c_str()));
        png.open(QIODevice::ReadOnly);
        image["uri"] = "data:image/png;base64," + QString::fromLatin1(png.readAll().toBase64());
        images[0] = image;
        broken["images"] = images;
        writeJson(rejectedPath, broken);
        invalid = selection;
        invalid.parts[0].source = rejectedPath.toUtf8().toStdString();
        check(!environment.load(invalid, graph, detail) && detail.find("Embedded") != std::string::npos,
            "embedded texture is explicitly rejected");
        broken = floorObject;
        auto materials = broken["materials"].toArray();
        for(int i = 0; i < materials.size(); ++i) {
            auto material = materials[i].toObject();
            auto pbr = material["pbrMetallicRoughness"].toObject();
            pbr["baseColorFactor"] = QJsonArray{1,1,1,0.5};
            material["pbrMetallicRoughness"] = pbr;
            material["alphaMode"] = "BLEND";
            materials[i] = material;
        }
        broken["materials"] = materials;
        writeJson(rejectedPath, broken);
        check(!environment.load(invalid, graph, detail) && detail.find("opaque") != std::string::npos,
            "transparent environment rejected without old scene loss");
        writeJson(rejectedPath, QJsonObject{{"asset", QJsonObject{{"version", "2.0"}}}, {"scenes", QJsonArray{QJsonObject{}}}, {"scene", 0}});
        check(!environment.load(invalid, graph, detail) && !detail.empty() && collect(graph).items().size() == 39,
            "empty glTF rejected without old scene loss");
        previous.reset();
        queue.clear();
        environment.clear(graph);
        check(graph.root()->children().empty(), "unload removes root");
        auto unicodeSelection = selection;
        for(auto& part : unicodeSelection.parts) part.source = (unicode / (part.id + ".gltf")).u8string();
        check(environment.load(unicodeSelection, graph, detail), "Unicode PNGs upload through real material chain");
        fs::remove(unicode / "Ocean-blue enamel-base.png");
        check(!environment.load(unicodeSelection, graph, detail) && collect(graph).items().size() == 39,
            "missing texture rejects replacement and preserves old GPU scene");
        environment.clear(graph);
        for(int cycle = 0; cycle < 50; ++cycle) {
            check(environment.load(selection, graph, detail), "switch cycle upload succeeds");
            std::weak_ptr<rendercore::Texture> weak;
            unsigned int textureId = 0;
            auto current = collect(graph);
            for(const auto& item : current.items()) if(!item.material->textures.empty()) {
                auto texture = item.material->textures.begin()->second;
                weak = texture;
                textureId = texture->id();
                break;
            }
            current.clear();
            environment.clear(graph);
            check(weak.expired() && !glIsTexture(textureId) && graph.root()->children().empty(), "switch cycle releases GPU texture and scene nodes");
        }
        auto held = rendercore::TextureManager::instance().load((assetRoot / "Epoxy-base.png").u8string());
        // Use a known material texture if exporter naming changes: fixture inventory is asserted below.
        check(held && held->isValid(), "independent texture owner loads");
        if(held) {
            check(environment.load(selection, graph, detail), "environment loads beside independent owner");
            environment.clear(graph);
            check(held->isValid() && glIsTexture(held->id()), "unload preserves another texture owner");
        }
    }
    context.doneCurrent();
    {
        RobotViewport pending;
        int committed = 0;
        QObject::connect(&pending, &RobotViewport::environmentCommitted, [&]() { ++committed; });
        check(pending.setEnvironmentSelection(selection, &detail) && committed == 0 && !pending.environmentSceneInitialized(),
            "pre-initialization request stays pending without committed signal");
        pending.resize(320, 240);
        pending.show();
        events();
        simulation_project::ProjectDocument empty;
        check(pending.loadProjectDocument(empty, fs::current_path()) && committed > 0 && pending.environmentSceneInitialized(),
            "pending model commits only after actual scene initialization");
    }
    {
        RobotViewport viewport;
        viewport.resize(640, 480);
        viewport.show();
        events();
        simulation_project::ProjectDocument document;
        check(viewport.loadProjectDocument(document, fs::current_path()), "empty project initializes");
        events();
        check(viewport.setEnvironmentSelection(selection, &detail), "viewport commits model environment");
        const auto frame = viewport.grabFramebuffer();
        check(!frame.isNull(), "viewport renders actual framebuffer");
        check(document.robots.empty() && document.objects.empty(), "presentation leaves document entity lists untouched");
        check(viewport.loadProjectDocument(document, fs::current_path()), "project rebuild succeeds");
        events();
        check(viewport.environmentSelection().id == selection.id && viewport.environmentSelection().isModel(), "model selection replays after scene rebuild");
        invalid = selection;
        invalid.parts[0].source = (assetRoot / "missing.gltf").u8string();
        check(!viewport.setEnvironmentSelection(invalid, &detail) && viewport.environmentSelection().isModel(), "viewport failed load retains committed selection");
        {
            using namespace robot_qt_viewer;
            QWidget window;
            QMenu menu(&window);
            RobotQtViewerLocalizationService localization(QFileInfo(descriptor).absolutePath() + "/../translations");
            check(localization.reloadCatalogs(&error), "real language catalogs load");
            RobotQtViewerEventHub hub;
            RobotQtViewerOperationStatusStore operations(hub);
            QSettings().setValue("view/environmentPreset", "workshop");
            RobotQtViewerEnvironmentController controller(viewport, menu, localization, operations, window);
            check(viewport.environmentPreset() == ProjectSceneEnvironmentPreset::Workshop, "legacy preference restored");
            check(controller.selectSource("factory-detailed", true, &error), "menu controller loads built-in model");
            const auto saved = QSettings().value("view/environmentSource").toString();
            check(saved == "factory-detailed", "successful load persists source");
            check(!controller.selectSource(bad, true, &error), "controller reports malformed descriptor failure");
            check(QSettings().value("view/environmentSource").toString() == saved &&
                viewport.environmentSelection().id == "factory-detailed", "failed load preserves preferences and effective model");
            auto* modelAction = menu.findChild<QAction*>("environment.factory-detailed");
            auto* cutawayAction = menu.findChild<QAction*>("environment.cutaway");
            check(modelAction && modelAction->isChecked() && cutawayAction && cutawayAction->isEnabled(), "menu projects committed selection");
            cutawayAction->trigger();
            check(!viewport.environmentSelection().cutaway && !QSettings().value("view/environmentCutaway").toBool(),
                "complete mode updates effective state and preference");
            check(localization.setLanguage("en-US", false, &error), "English activates");
            controller.retranslateUi();
            check(modelAction->text() == "Detailed factory (Blender)", "English menu translated");
            check(localization.setLanguage("zh-CN", false, &error), "Chinese activates");
            controller.retranslateUi();
            check(modelAction->text().contains(QChar(0x7cbe)), "Chinese menu translated");
            const int beforeCancel = operations.history().size();
            qputenv("SMROBOT_FILE_DIALOG_BACKEND", "qt");
            QTimer::singleShot(100, []() {
                for(auto* widget : QApplication::topLevelWidgets())
                    if(auto* dialog = qobject_cast<QFileDialog*>(widget)) dialog->reject();
            });
            auto* loadAction = menu.findChild<QAction*>("environment.load");
            check(loadAction != nullptr, "file action registered");
            loadAction->trigger();
            check(operations.history().size() == beforeCancel &&
                QSettings().value("view/environmentSource").toString() == saved, "file cancel leaves operation history and preference untouched");
            for(const auto& operation : operations.history())
                check(operation.state != RobotQtViewerOperationState::Running, "every operation has a terminal state");
            check(viewport.loadProjectDocument(document, fs::current_path()), "rebuild preserves controller environment");
            check(viewport.environmentSelection().id == "factory-detailed" && !viewport.environmentSelection().cutaway,
                "rebuild retains complete mode");
        }
        {
            using namespace robot_qt_viewer;
            QWidget window;
            QMenu menu(&window);
            RobotQtViewerLocalizationService localization(QFileInfo(descriptor).absolutePath() + "/../translations");
            RobotQtViewerEventHub hub;
            RobotQtViewerOperationStatusStore operations(hub);
            RobotQtViewerEnvironmentController restored(viewport, menu, localization, operations, window);
            check(viewport.loadProjectDocument(document, fs::current_path()), "new controller receives ready on rebuild");
            check(viewport.environmentSelection().id == "factory-detailed" && !viewport.environmentSelection().cutaway,
                "saved model source and complete mode restore after ready");
            const auto preference = QSettings().value("view/environmentSource");
            check(restored.selectSource("home", false, &error) && QSettings().value("view/environmentSource") == preference,
                "profile selection does not persist preference");
        }
        for(const auto preset : {ProjectSceneEnvironmentPreset::Factory, ProjectSceneEnvironmentPreset::Studio,
            ProjectSceneEnvironmentPreset::Workshop, ProjectSceneEnvironmentPreset::Home, ProjectSceneEnvironmentPreset::None}) {
            viewport.setEnvironmentPreset(preset);
            check(viewport.environmentPreset() == preset && !viewport.environmentSelection().isModel(), "legacy viewport environment remains usable");
        }
    }
    std::cout << "Model environment failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
