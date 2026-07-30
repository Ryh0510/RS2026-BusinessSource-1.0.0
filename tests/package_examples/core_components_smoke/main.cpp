#include <Collision/CollisionGeometryBuilder.h>
#include <Collision/CollisionObject.h>
#include <Collision/CollisionResult.h>
#include <Collision/CollisionWorld.h>
#include <Kinematics/Kinematics.h>
#include <RobotCore/RobotModel.h>
#include <RobotInstance/RobotInstance.h>
#include <RobotRuntime/RobotState.h>
#include <RobotTrajectoryCore/RobotTrajectory.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    int smokeRobotCore()
    {
        auto dofMember = &robot::RobotModel::dof;
        auto isTreeMember = &robot::RobotModel::isTree;
        if(dofMember == nullptr || isTreeMember == nullptr) {
            std::cerr << "RobotCore member symbol check failed.\n";
            return 1;
        }

        std::cout << "RobotCore imported symbols are linkable.\n";
        return 0;
    }

    int smokeKinematics()
    {
        auto forwardFunction = &kine::Kinematics::forward;
        if(forwardFunction == nullptr) {
            std::cerr << "Kinematics::forward symbol check failed.\n";
            return 1;
        }

        std::cout << "Kinematics imported symbols are linkable.\n";
        return 0;
    }

    int smokeRobotTrajectoryCore()
    {
        robottrajectory::JointTrajectory trajectory;
        trajectory.points.push_back({0.0, {0.0}, {}, {}});
        trajectory.points.push_back({1.0, {2.0}, {}, {}});

        const robottrajectory::TimedJointPoint sample =
            robottrajectory::RobotTrajectorySampler::evaluate(trajectory, 0.5);
        if(sample.q.size() != 1 || std::abs(sample.q.front() - 1.0) > 1e-9) {
            std::cerr << "RobotTrajectoryCore interpolation smoke failed.\n";
            return 1;
        }

        std::cout << "RobotTrajectoryCore interpolation smoke passed.\n";
        return 0;
    }

    int smokeCollision()
    {
        auto boxA = collision::CollisionGeometryBuilder::buildBox({1.0, 1.0, 1.0});
        auto boxB = collision::CollisionGeometryBuilder::buildBox({1.0, 1.0, 1.0});
        if(!boxA || !boxB) {
            std::cerr << "Collision geometry build smoke failed.\n";
            return 1;
        }

        auto objectA = std::make_shared<collision::CollisionObject>(1, boxA);
        auto objectB = std::make_shared<collision::CollisionObject>(2, boxB);
        collision::Transform3 transformA = collision::Transform3::Identity();
        collision::Transform3 transformB = collision::Transform3::Identity();
        transformB.translation().x() = 3.0;
        objectA->setTransform(transformA);
        objectB->setTransform(transformB);

        collision::CollisionWorld world;
        collision::CollisionResult result;
        const double distance = world.distance(objectA, objectB, result);
        if(!(distance > 0.0)) {
            std::cerr << "Collision distance smoke failed.\n";
            return 1;
        }

        std::cout << "Collision geometry and distance smoke passed.\n";
        return 0;
    }

    int smokeRobotRuntime()
    {
        robotruntime::RobotState state;
        state.resize(2, 3);

        if(state.q.size() != 2 || state.linkWorldTransforms.size() != 3) {
            std::cerr << "RobotRuntime RobotState::resize smoke failed.\n";
            return 1;
        }

        std::cout << "RobotRuntime state resize smoke passed.\n";
        return 0;
    }

    int smokeRobotInstance()
    {
        auto instanceNameMember = &robotinstance::RobotInstance::instanceName;
        auto updateMember = &robotinstance::RobotInstance::update;
        if(instanceNameMember == nullptr || updateMember == nullptr) {
            std::cerr << "RobotInstance member symbol check failed.\n";
            return 1;
        }

        std::cout << "RobotInstance imported symbols are linkable.\n";
        return 0;
    }
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        std::cerr << "Usage: SMRobotCoreComponentsSmoke <RobotCore|Kinematics|Collision|RobotTrajectoryCore|RobotRuntime|RobotInstance>\n";
        return 2;
    }

    const std::string component = argv[1];
    if(component == "RobotCore") {
        return smokeRobotCore();
    }
    if(component == "Kinematics") {
        return smokeKinematics();
    }
    if(component == "Collision") {
        return smokeCollision();
    }
    if(component == "RobotTrajectoryCore") {
        return smokeRobotTrajectoryCore();
    }
    if(component == "RobotRuntime") {
        return smokeRobotRuntime();
    }
    if(component == "RobotInstance") {
        return smokeRobotInstance();
    }

    std::cerr << "Unknown component smoke: " << component << '\n';
    return 2;
}
