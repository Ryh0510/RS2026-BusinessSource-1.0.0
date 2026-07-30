#include <RobotCore/RobotModel.h>
#include <RobotIO/IRobotLoader.h>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: SMRobotGen2RobotIOUrdfConsumer <robot.urdf>\n";
        return 1;
    }

    std::vector<robot::RobotModel> robots = IRobotLoader::get_robots(RobotType::URDFRobot, argv[1]);
    if (robots.empty())
    {
        std::cerr << "No robots loaded from: " << argv[1] << "\n";
        return 2;
    }

    const robot::RobotModel& model = robots.front();
    std::cout << "Loaded robot: " << model.name << "\n";
    std::cout << "Links: " << model.linkNames.size() << "\n";
    std::cout << "Joints: " << model.joints.size() << "\n";
    std::cout << "DOF: " << model.dof() << "\n";
    return 0;
}

