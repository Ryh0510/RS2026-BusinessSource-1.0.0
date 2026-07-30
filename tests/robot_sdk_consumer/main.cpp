#include <RobotSDK/RobotSdkApi.h>

int main()
{
    smrobotgen2::sdk::IRobotSdk* sdk = createRobotSdk();
    if (sdk == nullptr)
    {
        return 1;
    }

    if (sdk->abiVersion() <= 0)
    {
        destroyRobotSdk(sdk);
        return 2;
    }

    smrobotgen2::sdk::IRobotLoader* loader = sdk->createRobotLoader();
    if (loader == nullptr)
    {
        destroyRobotSdk(sdk);
        return 3;
    }

    sdk->destroyRobotLoader(loader);
    destroyRobotSdk(sdk);

    return 0;
}
