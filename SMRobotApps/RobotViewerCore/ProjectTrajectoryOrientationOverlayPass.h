#pragma once

#include <SceneCore/RenderPass/IRenderPass.h>
#include <RenderCore/Geometry.h>
#include <Eigen/Geometry>
#include <memory>
#include <vector>

namespace rendercore { class Shader; }

// Session-only Cartesian pose preview; all axes share one cached GL_LINES mesh.
class ProjectTrajectoryOrientationOverlayPass final : public scenecore::IRenderPass
{
public:
    void setPoints(const std::vector<Eigen::Isometry3d>& points);
    void setViewProjection(const Eigen::Matrix4f& matrix) { m_viewProjection = matrix; }
    void render(scenecore::RenderQueue& queue) override;

private:
    std::vector<rendercore::VertexRGBA> m_vertices;
    std::vector<unsigned int> m_indices;
    std::shared_ptr<rendercore::Geometry> m_geometry;
    std::shared_ptr<rendercore::Shader> m_shader;
    Eigen::Matrix4f m_viewProjection = Eigen::Matrix4f::Identity();
    bool m_dirty = true;
};
