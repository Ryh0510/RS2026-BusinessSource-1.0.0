#include "Utility/glm_io.hpp"
#include <iomanip>

namespace common::io
{
    std::ostream& operator<<(std::ostream& os, const glm::mat4& m)
    {
        os << std::fixed << std::setprecision(5);

        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                os << std::setw(12) << m[col][row] << " ";
            }
            os << '\n';
        }
        return os;
    }
}
