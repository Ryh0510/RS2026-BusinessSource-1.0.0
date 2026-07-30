#include <FileUtils/PathResolver.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace simscape
{
    static bool equalFilename(
        const fs::path& a,
        const fs::path& b)
    {
#ifdef _WIN32
        // Windows paths are case-insensitive.
        return _wcsicmp(
            a.wstring().c_str(),
            b.wstring().c_str()) == 0;
#else
        return a == b;
#endif
    }

    static std::string searchRecursively(
        const fs::path& root,
        const std::string& filenameUtf8)
    {
        if (!fs::exists(root) || !fs::is_directory(root))
            return "";

        fs::path targetName = fs::u8path(filenameUtf8);

        for (auto it = fs::recursive_directory_iterator(root);
            it != fs::recursive_directory_iterator(); ++it)
        {
            if (!it->is_regular_file())
                continue;

            if (equalFilename(it->path().filename(), targetName))
            {
                return fs::absolute(it->path()).u8string();
            }
        }

        return "";
    }


    std::string resolvePath(
        const std::string& xmlFilePath,
        const std::string& meshFileName,
        const std::vector<std::string>& extraSearchDirs)
    {
        if (meshFileName.empty())
            return "";

        fs::path xmlPath(xmlFilePath);

        if (!fs::exists(xmlPath))
            return meshFileName;

        fs::path currentDir = xmlPath.parent_path();

        // =====================================================
        // Step 1 + 2 : Recursively search the current directory.
        // =====================================================
        {
            std::string found =
                searchRecursively(currentDir, meshFileName);

            if (!found.empty())
                return found;
        }

        // =====================================================
        // Step 3 : Recursively search the parent directory.
        // =====================================================
        {
            fs::path parentDir = currentDir.parent_path();

            if (!parentDir.empty())
            {
                std::string found =
                    searchRecursively(parentDir, meshFileName);

                if (!found.empty())
                    return found;
            }
        }

        // =====================================================
        // Step 4 : Additional search directories.
        // =====================================================
        for (const auto& dir : extraSearchDirs)
        {
            fs::path extraPath(dir);

            std::string found =
                searchRecursively(extraPath, meshFileName);

            if (!found.empty())
                return found;
        }

        // Not found; return the original name.
        return meshFileName;
    }

}