#pragma once

#include <Collision/CollisionObject.h>
#include <Collision/CollisionShapeDesc.h>
#include <Collision/RobotCollisionInstance.h>
#include <Collision/RobotCollisionModel.h>
#include <Kinematics/StewartPlatformKinematics.h>
#include <RenderCore/Material.h>
#include <RenderCore/Model.h>
#include <RobotCore/RobotModel.h>
#include <RobotInstance/RobotInstance.h>
#include <RobotRenderBridge/RobotVisualBridge.h>
#include <SceneCore/ModelNode.h>
#include <SceneCore/PointCloudNode.h>
#include <SceneCore/RenderQueue.h>
#include <SimulationRuntime/ProjectParallelMechanismRuntime.h>
#include <VisualizationSDK/SurfaceScalarOverlay.h>

#include <Eigen/Core>
#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace simulation_runtime
{
    struct RuntimeSceneObject;
    struct RuntimePointCloud;
}

class VisibleModelNode : public scenecore::ModelNode
{
public:
    explicit VisibleModelNode(std::shared_ptr<rendercore::Model> model)
        : scenecore::ModelNode(std::move(model))
    {
    }

    void setVisible(bool visible)
    {
        m_visible = visible;
    }

    void collect(scenecore::RenderQueue& queue) override
    {
        if(m_visible) {
            scenecore::ModelNode::collect(queue);
        }
    }

private:
    bool m_visible = true;
};

struct MeshOverlay
{
    std::shared_ptr<VisibleModelNode> node;
    std::shared_ptr<rendercore::Material> material;
    std::string linkName;
};

using StewartLegRuntimeControl = simulation_runtime::ParallelLegRuntimeControl;

struct RuntimeRobot
{
    RuntimeRobot();
    explicit RuntimeRobot(simulation_runtime::ProjectParallelRobotState& state);
    RuntimeRobot(const RuntimeRobot& other);
    RuntimeRobot(RuntimeRobot&& other) noexcept;
    RuntimeRobot& operator=(const RuntimeRobot& other);
    RuntimeRobot& operator=(RuntimeRobot&& other) noexcept;
    ~RuntimeRobot();

    std::shared_ptr<simulation_runtime::ProjectParallelRobotState> ownedParallelState;
    simulation_runtime::ProjectParallelRobotState* parallelState = nullptr;
    simulation_runtime::RuntimeRobot* simulationRobot = nullptr;
    uint64_t& runtimeId;
    std::string& documentId;
    robot::RobotModel& model;
    std::shared_ptr<robotinstance::RobotInstance>& instance;
    std::shared_ptr<robot_render::RobotVisualBridge> visualBridge;
    collision::RobotCollisionModelPtr collisionModel;
    collision::RobotCollisionInstancePtr collisionInstance;
    std::unordered_map<collision::ObjectID, MeshOverlay> meshOverlays;
    std::unordered_map<std::string, std::vector<std::shared_ptr<rendercore::Material>>> linkOriginalMaterials;
    std::unordered_set<std::string> highlightedLinks;
    std::string& name;
    std::string& sourceType;
    std::string& sourcePath;
    int& sourceModelIndex;
    collision::Transform3& baseTransform;
    bool& collisionEnabled;
    bool& autoMotionEnabled;
    double& autoMotionAmplitude;
    double& autoMotionSpeed;
    bool& parallelControlEnabled;
    collision::Transform3& parallelHomeBaseTransform;
    kine::StewartPlatformGeometry& parallelGeometry;
    kine::StewartPlatformPose& parallelPose;
    std::array<double, 6>& parallelActuatorLengths;
    std::array<double, 6>& parallelActuatorHomeLengths;
    std::array<double, 6>& parallelActuatorRates;
    std::array<int, 6>& parallelActuatorDofIndices;
    std::array<double, 6>& parallelActuatorSigns;
    std::array<StewartLegRuntimeControl, 6>& parallelLegControls;
    bool& parallelInternalPlatformVisualsEnabled;
    std::vector<std::string>& parallelInternalPlatformDrivenLinks;
    std::unordered_map<std::string, collision::Transform3>& parallelInternalPlatformHomeLocalTransforms;
    bool& parallelFollowerEnabled;
    int& parallelFollowerLegIndex;
    collision::Transform3& parallelFollowerHomeTransform;
    bool& parallelFollowerAnchorsValid;
    collision::Vec3& parallelFollowerHomeBaseAnchor;
    collision::Vec3& parallelFollowerHomePlatformAnchor;
    int& parallelFollowerActuatorDofIndex;
    double& parallelFollowerActuatorSign;
    double& parallelFollowerHomeLength;
    std::vector<std::string>& parallelFollowerDrivenLinks;
    std::unordered_map<std::string, collision::Transform3>& parallelFollowerHomeLinkTransforms;
    std::string sprayNozzleLinkName;
    collision::Transform3 sprayNozzleLocalTransform = collision::Transform3::Identity();
};

struct RuntimeSceneCollisionObject
{
    collision::CollisionObjectPtr collisionObject;
    collision::CollisionShapeDesc collisionShape;
    collision::Transform3 localTransform = collision::Transform3::Identity();
    std::string modelId;
    bool currentModel = true;
};

struct RuntimeSurfaceScalarSubMesh
{
    std::vector<Eigen::Vector3d> positions;
    std::vector<uint32_t> indices;
    std::vector<double> values;
};

struct RuntimeSurfaceScalarOverlay
{
    smrobot::visualization::SurfaceScalarOverlay descriptor;
    std::vector<RuntimeSurfaceScalarSubMesh> subMeshes;
    std::shared_ptr<rendercore::Model> originalModel;
    std::shared_ptr<rendercore::Model> overlayModel;
    bool visible{ false };
};

struct RuntimeSceneObject
{
    simulation_runtime::RuntimeSceneObject* simulationObject = nullptr;
    simulation_runtime::RuntimePointCloud* simulationPointCloud = nullptr;
    uint64_t runtimeId = 0;
    std::string documentId;
    std::string name;
    std::string objectType;
    collision::Transform3 transform = collision::Transform3::Identity();
    glm::mat4 visualLocal = glm::mat4(1.0f);
    std::shared_ptr<VisibleModelNode> visualNode;
    std::shared_ptr<scenecore::PointCloudNode> pointCloudNode;
    std::shared_ptr<rendercore::Model> visualModel;
    std::vector<std::shared_ptr<rendercore::Material>> originalMaterials;
    std::shared_ptr<rendercore::Material> highlightMaterial;
    std::shared_ptr<RuntimeSurfaceScalarOverlay> surfaceScalarOverlay;
    collision::CollisionObjectPtr collisionObject;
    collision::CollisionShapeDesc collisionShape;
    std::vector<RuntimeSceneCollisionObject> collisionObjects;
    bool collisionEnabled = true;
    bool highlighted = false;
    float pointCloudBasePointSize = 2.0f;
    bool pointCloudBoundsValid = false;
    collision::Vec3 pointCloudLocalBoundsMin = collision::Vec3::Zero();
    collision::Vec3 pointCloudLocalBoundsMax = collision::Vec3::Zero();
};

struct MaterialOverride
{
    std::shared_ptr<rendercore::Model> model;
    unsigned int subMeshIndex = 0;
    std::shared_ptr<rendercore::Material> originalMaterial;
    Eigen::Vector4f color = Eigen::Vector4f(0.15f, 1.0f, 0.35f, 1.0f);
    Eigen::Vector3f emissive = Eigen::Vector3f(0.02f, 0.18f, 0.04f);
};
