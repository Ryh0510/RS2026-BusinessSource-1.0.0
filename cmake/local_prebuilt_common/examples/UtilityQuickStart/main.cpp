#include <Utility/MathConvert.hpp>

int main()
{
    const Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();
    const glm::mat4 converted = math::eigenToGlm(transform);
    return converted[3][3] == 1.0f ? 0 : 1;
}
