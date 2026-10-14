#include "ProjectTrajectoryOrientationOverlayPass.h"
#include <RenderCore/Shader.h>
#include <algorithm>

void ProjectTrajectoryOrientationOverlayPass::setPoints(const std::vector<Eigen::Isometry3d>& points)
{
    m_vertices.clear();
    m_indices.clear();
    m_dirty = true;
    if(points.empty()) { return; }
    Eigen::Vector3d minimum = points.front().translation(), maximum = minimum;
    for(const auto& pose : points) {
        if(!pose.matrix().allFinite()) { continue; }
        minimum = minimum.cwiseMin(pose.translation());
        maximum = maximum.cwiseMax(pose.translation());
    }
    const double length = std::clamp((maximum - minimum).norm() * 0.025, 0.015, 0.040);
    m_vertices.reserve(points.size() * 6);
    m_indices.reserve(points.size() * 6);
    for(const auto& pose : points) {
        if(!pose.matrix().allFinite()) { continue; }
        for(int axis = 0; axis < 3; ++axis) {
            const Eigen::Vector3d start = pose.translation();
            const Eigen::Vector3d end = start + length * pose.linear().col(axis);
            for(const auto& point : {start, end}) {
                rendercore::VertexRGBA vertex{};
                for(int j = 0; j < 3; ++j) {
                    vertex.position[j] = static_cast<float>(point[j]);
                    vertex.color[j] = j == axis ? 1.0f : 0.0f;
                }
                vertex.color[3] = 1.0f;
                m_indices.push_back(static_cast<unsigned int>(m_vertices.size()));
                m_vertices.push_back(vertex);
            }
        }
    }
}

void ProjectTrajectoryOrientationOverlayPass::render(scenecore::RenderQueue& queue)
{
    if(m_vertices.empty()) { return; }
    // Use the SDK queue to respect both the camera's global filter and this pass.
    scenecore::AxisInstance token;
    queue.axes.push_back(token);
    const auto visible = queue.filteredAxes(filter());
    const bool allowed = std::find(visible.begin(), visible.end(), &queue.axes.back()) != visible.end();
    queue.axes.pop_back();
    if(!allowed) { return; }
    GLint program = 0, vao = 0, arrayBuffer = 0;
    GLfloat width = 1;
    GLboolean depthMask = GL_TRUE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetFloatv(GL_LINE_WIDTH, &width);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    const bool depth = glIsEnabled(GL_DEPTH_TEST) != 0;
    const bool blend = glIsEnabled(GL_BLEND) != 0;
    if(!m_shader) {
        m_shader = rendercore::Shader::fromSources("TrajectoryOrientationAxes",
            "#version 330 core\n"
            "layout(location=0) in vec3 position;\n"
            "layout(location=3) in vec4 color;\n"
            "uniform mat4 viewProjection;\n"
            "out vec4 lineColor;\n"
            "void main(){gl_Position=viewProjection*vec4(position,1);lineColor=color;}\n",
            "#version 330 core\n"
            "in vec4 lineColor;out vec4 fragmentColor;\n"
            "void main(){fragmentColor=lineColor;}\n");
    }
    if(m_dirty) {
        m_geometry = std::make_shared<rendercore::Geometry>(m_vertices, m_indices, GL_LINES);
        m_dirty = false;
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glLineWidth(1.0f);
    m_shader->use();
    m_shader->setMat4("viewProjection", m_viewProjection);
    m_geometry->draw();
    glUseProgram(static_cast<GLuint>(program));
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
    glLineWidth(width);
    glDepthMask(depthMask);
    if(depth) { glEnable(GL_DEPTH_TEST); }
    if(blend) { glEnable(GL_BLEND); }
}
