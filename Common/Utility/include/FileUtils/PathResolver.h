#pragma once

#include <string>
#include <vector>

namespace simscape
{
    std::string resolvePath(
        const std::string& targetPath,
        const std::string& targetFileName,
        const std::vector<std::string>& extraSearchDirs);
}
