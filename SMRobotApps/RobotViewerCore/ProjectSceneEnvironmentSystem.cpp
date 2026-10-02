#include "ProjectSceneEnvironmentSystem.h"

#include <SceneCore/DebugDraw.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <vector>

namespace
{
    enum class Shape
    {
        Box,
        Cylinder
    };

    struct Primitive
    {
        Shape shape = Shape::Box;
        glm::vec3 position{ 0.0f };
        glm::vec3 size{ 1.0f };
        glm::vec4 color{ 1.0f };
        float yawRadians = 0.0f;
    };

    using Primitives = std::vector<Primitive>;

    const scenecore::RenderTag kEnvironmentTag{
        scenecore::RenderLayer::VisualMesh,
        scenecore::RenderCategory::Environment,
        scenecore::RenderFeature::Visual
    };

    void addBox(
        Primitives& values,
        const glm::vec3& position,
        const glm::vec3& size,
        const glm::vec4& color,
        float yawRadians = 0.0f)
    {
        values.push_back({ Shape::Box, position, size, color, yawRadians });
    }

    void addCylinder(
        Primitives& values,
        const glm::vec3& position,
        float radius,
        float height,
        const glm::vec4& color)
    {
        values.push_back({ Shape::Cylinder, position, glm::vec3(radius, radius, height), color, 0.0f });
    }

    void addGround(Primitives& values, const glm::vec4& color)
    {
        addBox(values, { 0.0f, 0.0f, -0.08f }, { 14.0f, 14.0f, 0.12f }, color);
    }

    Primitives studioPrimitives()
    {
        Primitives values;
        addGround(values, { 0.34f, 0.37f, 0.39f, 1.0f });
        addBox(values, { 0.0f, 5.8f, 2.4f }, { 13.0f, 0.18f, 4.8f }, { 0.28f, 0.31f, 0.34f, 1.0f });
        addBox(values, { -5.8f, 0.0f, 2.4f }, { 0.18f, 11.5f, 4.8f }, { 0.25f, 0.28f, 0.31f, 1.0f });
        addBox(values, { 0.0f, 5.68f, 2.7f }, { 4.5f, 0.05f, 0.06f }, { 0.75f, 0.82f, 0.86f, 1.0f });
        return values;
    }

    Primitives factoryPrimitives()
    {
        Primitives values;
        const glm::vec4 floor{ 0.27f, 0.31f, 0.33f, 1.0f };
        const glm::vec4 wall{ 0.36f, 0.40f, 0.41f, 1.0f };
        const glm::vec4 safety{ 0.95f, 0.66f, 0.08f, 1.0f };
        const glm::vec4 machine{ 0.16f, 0.27f, 0.32f, 1.0f };
        const glm::vec4 steel{ 0.22f, 0.25f, 0.27f, 1.0f };

        addGround(values, floor);
        addBox(values, { 0.0f, 5.8f, 3.0f }, { 14.0f, 0.20f, 6.0f }, wall);
        addBox(values, { -5.8f, 0.0f, 3.0f }, { 0.20f, 11.5f, 6.0f }, wall);
        for(float x : { -4.5f, -1.5f, 1.5f, 4.5f }) {
            addBox(values, { x, 5.64f, 3.0f }, { 0.12f, 0.08f, 5.6f }, { 0.60f, 0.64f, 0.64f, 1.0f });
        }
        addBox(values, { 0.0f, -3.0f, -0.005f }, { 9.5f, 0.08f, 0.025f }, safety);
        addBox(values, { 0.0f, 3.0f, -0.005f }, { 9.5f, 0.08f, 0.025f }, safety);
        addBox(values, { -4.75f, 0.0f, -0.005f }, { 0.08f, 6.0f, 0.025f }, safety);
        addBox(values, { 4.75f, 0.0f, -0.005f }, { 0.08f, 6.0f, 0.025f }, safety);

        addBox(values, { -3.8f, 4.2f, 0.55f }, { 1.35f, 0.85f, 1.1f }, machine);
        addBox(values, { -3.8f, 4.2f, 1.24f }, { 1.05f, 0.58f, 0.28f }, { 0.22f, 0.36f, 0.41f, 1.0f });
        addBox(values, { 3.5f, 4.0f, 1.10f }, { 1.2f, 0.75f, 2.2f }, { 0.52f, 0.56f, 0.56f, 1.0f });
        addBox(values, { 3.5f, 3.59f, 1.45f }, { 0.75f, 0.04f, 0.50f }, { 0.08f, 0.12f, 0.14f, 1.0f });

        for(int i = 0; i < 7; ++i) {
            const float x = -3.6f + static_cast<float>(i) * 1.2f;
            addCylinder(values, { x, 4.8f, 0.42f }, 0.15f, 0.75f, steel);
            addBox(values, { x, 4.8f, 0.81f }, { 1.05f, 0.44f, 0.07f }, { 0.42f, 0.47f, 0.48f, 1.0f });
        }

        for(float x : { -3.2f, -1.6f, 0.0f, 1.6f, 3.2f }) {
            addCylinder(values, { x, -3.0f, 0.55f }, 0.05f, 1.1f, safety);
        }
        addBox(values, { 0.0f, -3.0f, 1.02f }, { 6.5f, 0.06f, 0.06f }, safety);
        return values;
    }

    Primitives workshopPrimitives()
    {
        Primitives values;
        const glm::vec4 wall{ 0.47f, 0.50f, 0.49f, 1.0f };
        const glm::vec4 steel{ 0.24f, 0.27f, 0.28f, 1.0f };
        const glm::vec4 wood{ 0.48f, 0.30f, 0.15f, 1.0f };
        const glm::vec4 cabinet{ 0.18f, 0.38f, 0.36f, 1.0f };

        addGround(values, { 0.32f, 0.33f, 0.32f, 1.0f });
        addBox(values, { 0.0f, 5.8f, 2.6f }, { 13.0f, 0.18f, 5.2f }, wall);
        addBox(values, { -5.8f, 0.0f, 2.6f }, { 0.18f, 11.5f, 5.2f }, wall);

        addBox(values, { -3.3f, 3.8f, 0.82f }, { 2.8f, 0.9f, 0.12f }, wood);
        for(float x : { -4.4f, -2.2f }) {
            addBox(values, { x, 3.8f, 0.40f }, { 0.12f, 0.75f, 0.8f }, steel);
        }
        addBox(values, { -3.3f, 4.20f, 1.35f }, { 2.65f, 0.08f, 0.9f }, { 0.34f, 0.37f, 0.36f, 1.0f });

        addBox(values, { 3.6f, 4.2f, 1.1f }, { 1.5f, 0.70f, 2.2f }, cabinet);
        for(float z : { 0.45f, 0.9f, 1.35f, 1.8f }) {
            addBox(values, { 3.6f, 3.81f, z }, { 1.25f, 0.04f, 0.06f }, { 0.65f, 0.69f, 0.66f, 1.0f });
        }

        for(float x : { -1.2f, 0.0f, 1.2f }) {
            addBox(values, { x, 4.6f, 0.38f }, { 0.95f, 0.72f, 0.74f }, { 0.54f, 0.19f + (x + 1.2f) * 0.04f, 0.11f, 1.0f });
        }

        for(float x : { -4.7f, -3.0f }) {
            addBox(values, { x, -2.1f, 1.15f }, { 0.12f, 2.1f, 2.3f }, steel);
        }
        for(float z : { 0.35f, 1.15f, 1.95f }) {
            addBox(values, { -3.85f, -2.1f, z }, { 1.75f, 1.9f, 0.08f }, steel);
        }
        return values;
    }

    Primitives homePrimitives()
    {
        Primitives values;
        const glm::vec4 wall{ 0.72f, 0.73f, 0.69f, 1.0f };
        const glm::vec4 wood{ 0.46f, 0.29f, 0.15f, 1.0f };
        const glm::vec4 cabinet{ 0.58f, 0.62f, 0.58f, 1.0f };
        const glm::vec4 fabric{ 0.20f, 0.36f, 0.40f, 1.0f };

        addGround(values, { 0.30f, 0.20f, 0.12f, 1.0f });
        for(int i = -5; i <= 5; ++i) {
            addBox(values, { static_cast<float>(i) * 1.8f, 0.0f, -0.012f },
                { 0.025f, 18.0f, 0.012f }, { 0.16f, 0.10f, 0.055f, 1.0f });
        }
        addBox(values, { 0.0f, 5.8f, 2.6f }, { 13.0f, 0.18f, 5.2f }, wall);
        addBox(values, { -5.8f, 0.0f, 2.6f }, { 0.18f, 11.5f, 5.2f }, wall);

        addBox(values, { -3.8f, 4.4f, 0.55f }, { 2.7f, 1.0f, 1.1f }, cabinet);
        addBox(values, { -3.8f, 4.4f, 1.14f }, { 2.85f, 1.1f, 0.08f }, { 0.25f, 0.27f, 0.25f, 1.0f });
        addBox(values, { -1.4f, 4.4f, 0.95f }, { 1.6f, 0.65f, 1.9f }, cabinet);
        addBox(values, { -3.0f, 2.7f, 0.46f }, { 1.45f, 0.72f, 0.92f }, { 0.49f, 0.51f, 0.47f, 1.0f });
        addBox(values, { -3.0f, 2.7f, 0.96f }, { 1.60f, 0.84f, 0.08f }, { 0.27f, 0.29f, 0.27f, 1.0f });

        addBox(values, { 2.7f, 1.8f, 0.44f }, { 2.4f, 0.95f, 0.68f }, fabric);
        addBox(values, { 2.7f, 2.16f, 0.98f }, { 2.4f, 0.30f, 0.88f }, fabric);
        addBox(values, { 1.64f, 1.8f, 0.75f }, { 0.30f, 0.9f, 0.82f }, fabric);
        addBox(values, { 3.76f, 1.8f, 0.75f }, { 0.30f, 0.9f, 0.82f }, fabric);
        addBox(values, { 2.1f, -0.2f, 0.32f }, { 1.5f, 0.78f, 0.16f }, wood);
        for(float x : { 1.55f, 2.65f }) {
            addBox(values, { x, -0.2f, 0.15f }, { 0.10f, 0.58f, 0.30f }, wood);
        }
        addBox(values, { 2.1f, -0.2f, 0.006f }, { 2.6f, 1.7f, 0.02f }, { 0.20f, 0.38f, 0.36f, 1.0f });

        addBox(values, { 4.6f, -1.8f, 1.2f }, { 1.5f, 0.42f, 2.4f }, wood);
        for(float z : { 0.42f, 1.12f, 1.82f }) {
            addBox(values, { 4.6f, -1.8f, z }, { 1.35f, 0.56f, 0.07f }, { 0.26f, 0.17f, 0.10f, 1.0f });
        }
        return values;
    }

    Primitives makePrimitives(ProjectSceneEnvironmentPreset preset)
    {
        switch(preset) {
        case ProjectSceneEnvironmentPreset::Studio:
            return studioPrimitives();
        case ProjectSceneEnvironmentPreset::Factory:
            return factoryPrimitives();
        case ProjectSceneEnvironmentPreset::Workshop:
            return workshopPrimitives();
        case ProjectSceneEnvironmentPreset::Home:
            return homePrimitives();
        case ProjectSceneEnvironmentPreset::None:
        default:
            return {};
        }
    }
}

void ProjectSceneEnvironmentSystem::submit(scenecore::DebugDraw& draw) const
{
    for(const Primitive& primitive : makePrimitives(m_preset)) {
        glm::mat4 transform = glm::translate(
            glm::mat4(1.0f),
            primitive.position * m_sceneScale);
        if(primitive.yawRadians != 0.0f) {
            transform = glm::rotate(transform, primitive.yawRadians, glm::vec3(0.0f, 0.0f, 1.0f));
        }
        if(primitive.shape == Shape::Cylinder) {
            draw.drawCylinder(
                transform,
                primitive.size.x * m_sceneScale,
                primitive.size.z * m_sceneScale,
                primitive.color,
                scenecore::DrawType::UsingLitShader,
                kEnvironmentTag);
        } else {
            draw.drawCube(
                transform,
                primitive.size * m_sceneScale,
                primitive.color,
                scenecore::DrawType::UsingLitShader,
                kEnvironmentTag);
        }
    }
}

void ProjectSceneEnvironmentSystem::setSceneScale(float scale)
{
    m_sceneScale = std::clamp(scale, 0.55f, 1.5f);
}

std::size_t ProjectSceneEnvironmentSystem::primitiveCount() const
{
    return makePrimitives(m_preset).size();
}
