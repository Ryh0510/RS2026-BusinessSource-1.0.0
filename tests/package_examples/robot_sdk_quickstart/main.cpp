#include <RobotSDK/IRobotLoader.h>
#include <RobotSDK/IRobotSdk.h>
#include <RobotSDK/RobotSdkApi.h>

#include <iostream>

int main()
{
    smrobotgen2::sdk::IRobotSdk* sdk = createRobotSdk();
    if (sdk == nullptr)
    {
        std::cerr << "Failed to create RobotSDK.\n";
        return 1;
    }

    std::cout << "RobotSDK version: " << sdk->version()
        << " ABI: " << sdk->abiVersion() << "\n";

    smrobotgen2::sdk::IRobotLoader* loader = sdk->createRobotLoader();
    if (loader == nullptr)
    {
        destroyRobotSdk(sdk);
        std::cerr << "Failed to create robot loader.\n";
        return 2;
    }

    sdk->destroyRobotLoader(loader);
    destroyRobotSdk(sdk);
    return 0;
}

