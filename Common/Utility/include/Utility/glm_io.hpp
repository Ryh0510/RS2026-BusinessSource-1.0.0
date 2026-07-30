#pragma once

#include <glm/glm.hpp>
#include <ostream>

namespace common::io
{
    std::ostream& operator<<(std::ostream& os, const glm::mat4& m);

}
