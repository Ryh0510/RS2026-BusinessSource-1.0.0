#include "ProjectSceneModelEnvironment.h"

#include <AssetCore/ModelAssetLeaseCache.h>
#include <AssetCore/ModelDesc.h>
#include <RenderCore/GeometryResourceCache.h>
#include <RenderCore/Material.h>
#include <RenderCore/Model.h>
#include <RenderCore/ModelManager.h>
#include <SceneCore/RenderItem.h>
#include <SceneCore/RenderQueue.h>
#include <SceneCore/SceneGraph.h>
#include <SceneCore/SceneNode.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <stdexcept>

namespace
{
    struct ViewState { glm::vec3 camera{0.0f}; bool cutaway = false; };

    class EnvironmentNode final : public scenecore::SceneNode
    {
    public:
        std::shared_ptr<rendercore::Model> model;
        ProjectSceneEnvironmentPart part;
        ViewState* view = nullptr;
        bool wallVisible = true;

        void collect(scenecore::RenderQueue& queue) override
        {
            if(!visible() || !model) return;
            if(view->cutaway) {
                if(part.role == ProjectSceneEnvironmentPartRole::Roof) return;
                if(part.role == ProjectSceneEnvironmentPartRole::Wall) {
                    const float side = glm::dot(view->camera, part.wallNormal) - part.wallOffset;
                    if(side > 0.15f) wallVisible = false;
                    if(side < -0.15f) wallVisible = true;
                    if(!wallVisible) return;
                }
            }
            for(std::size_t i = 0; i < model->subMeshCount(); ++i) {
                auto& sub = model->subMesh(static_cast<unsigned int>(i));
                scenecore::RenderItem item;
                item.geometry = sub.geometry.get();
                item.material = sub.material.get();
                item.transform = world();
                item.tag = {scenecore::RenderLayer::VisualMesh,
                    scenecore::RenderCategory::Environment, scenecore::RenderFeature::Visual};
                queue.add(item);
            }
        }
    };
}

struct ProjectSceneModelEnvironment::Impl
{
    ViewState view;
    std::shared_ptr<scenecore::SceneNode> root;
    std::size_t count = 0;
};

ProjectSceneModelEnvironment::ProjectSceneModelEnvironment() : m_impl(std::make_unique<Impl>()) {}
ProjectSceneModelEnvironment::~ProjectSceneModelEnvironment() = default;

bool ProjectSceneModelEnvironment::load(const ProjectSceneEnvironmentSelection& selection,
    scenecore::SceneGraph& graph, std::string& error)
{
    error.clear();
    if(!validateProjectSceneEnvironment(selection, error)) return false;
    if(!selection.isModel()) { clear(graph); return true; }
    try {
        auto root = std::make_shared<scenecore::SceneNode>("__presentation_environment");
        // Per-load caches share within this asset, without retaining an unloaded environment.
        assetcore::ModelAssetLeaseCache assets;
        rendercore::GeometryResourceCache geometry;
        for(const auto& part : selection.parts) {
            assetcore::ModelAssetLeaseRequest request;
            request.resolveContext = assetcore::AssetResolver::defaultContext();
            request.source = part.source;
            request.scale = selection.scale;
            request.consumer = "presentation-environment:" + part.id;
            auto lease = assets.acquire(request);
            if(!lease.success()) throw std::runtime_error(lease.errorMessage);
            if(lease.model->subMeshCount() == 0) throw std::runtime_error("Environment model has no meshes: " + part.source);
            for(const auto& mesh : lease.model->subMeshes()) {
                if(mesh.materialIndex < 0 || static_cast<std::size_t>(mesh.materialIndex) >= lease.model->materialCount())
                    throw std::runtime_error("Environment mesh has invalid material index: " + part.source);
                if(mesh.geometry.positions.empty() || mesh.geometry.indices.empty())
                    throw std::runtime_error("Environment model contains empty geometry: " + part.source);
            }
            for(const auto& material : lease.model->materials()) {
                if(material.baseColor.w() < 0.999f)
                    throw std::runtime_error("Environment requires opaque materials: " + part.source);
                for(const auto& texture : material.textures) {
                    if(texture.second.find('*') != std::string::npos ||
                        texture.second.rfind("data:", 0) == 0)
                        throw std::runtime_error("Embedded textures are not supported; export glTF with external images.");
                    if(!std::filesystem::is_regular_file(std::filesystem::u8path(texture.second)))
                        throw std::runtime_error("Missing environment texture: " + texture.second);
                }
            }
            auto model = rendercore::ModelManager::instance().buildModelFromDesc(*lease.model, geometry, lease.assetKey);
            if(!model) throw std::runtime_error("Environment GPU model build failed.");
            for(std::size_t i = 0; i < model->subMeshCount(); ++i) {
                const auto& mesh = model->subMesh(static_cast<unsigned int>(i));
                const auto& desc = lease.model->material(lease.model->subMesh(i).materialIndex);
                if(!mesh.material || mesh.material->textures.size() != desc.textures.size())
                    throw std::runtime_error("Environment material or texture upload failed: " + part.source);
            }
            auto node = std::make_shared<EnvironmentNode>();
            node->setName(part.id);
            node->part = part;
            node->part.wallOffset *= selection.scale;
            node->model = std::move(model);
            node->view = &m_impl->view;
            node->setLocal(lease.model->get_local());
            root->addChild(node);
        }
        // Register the new graph only after every dependency and GPU upload succeeds.
        clear(graph);
        graph.root()->addChild(root);
        graph.registerNode(root);
        m_impl->root = std::move(root);
        m_impl->count = selection.parts.size();
        return true;
    } catch(const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

void ProjectSceneModelEnvironment::clear(scenecore::SceneGraph& graph)
{
    if(m_impl->root) graph.removeNode(m_impl->root);
    m_impl->root.reset();
    m_impl->count = 0;
}

void ProjectSceneModelEnvironment::withView(const glm::vec3& camera, bool cutaway,
    const std::function<void()>& render)
{
    const ViewState previous = m_impl->view;
    struct Restore { ViewState& target; ViewState saved; ~Restore() { target = saved; } } restore{m_impl->view, previous};
    m_impl->view = {camera, cutaway};
    render();
}

std::size_t ProjectSceneModelEnvironment::partCount() const { return m_impl->count; }
