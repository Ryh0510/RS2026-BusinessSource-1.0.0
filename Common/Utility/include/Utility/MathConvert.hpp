#pragma once

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <glm/glm.hpp>

namespace math
{
    // Eigen and glm both use column-major storage by default, but their public
    // matrix indexing APIs are different. These helpers copy by semantic row
    // and column indices so conversion does not depend on raw memory layout.
    //
    // Convention:
    // - transforms are right-handed unless a caller documents otherwise.
    // - matrix values are copied without unit conversion.
    // - translation is stored in the last column for 4x4 transforms.

    inline glm::mat4 eigenMatrixToGlm(const Eigen::Matrix4d& m)
    {
        glm::mat4 r(1.0f);

        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                r[col][row] = static_cast<float>(m(row, col));

        return r;
    }

    inline glm::mat4 eigenMatrixToGlm(const Eigen::Matrix4f& m)
    {
        glm::mat4 r(1.0f);

        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                r[col][row] = m(row, col);

        return r;
    }

    inline glm::mat4 eigenToGlm1by1(const Eigen::Matrix4d& m)
    {
        return eigenMatrixToGlm(m);
    }

    inline glm::mat4 eigenToGlm(const Eigen::Matrix4d& m)
    {
        return eigenMatrixToGlm(m);
    }

    inline glm::mat4 eigenToGlm(const Eigen::Matrix4f& m)
    {
        return eigenMatrixToGlm(m);
    }

    inline glm::mat4 eigenToGlm(const Eigen::Isometry3d& T)
    {
        return eigenMatrixToGlm(T.matrix());
    }

    inline glm::vec3 eigenToGlm(const Eigen::Vector3d& v)
    {
        return glm::vec3(
            static_cast<float>(v.x()),
            static_cast<float>(v.y()),
            static_cast<float>(v.z()));
    }

    inline glm::vec3 eigenToGlm(const Eigen::Vector3f& v)
    {
        return glm::vec3(v.x(), v.y(), v.z());
    }

    inline glm::vec4 eigenToGlm(const Eigen::Vector4d& v)
    {
        return glm::vec4(
            static_cast<float>(v.x()),
            static_cast<float>(v.y()),
            static_cast<float>(v.z()),
            static_cast<float>(v.w()));
    }

    inline glm::vec4 eigenToGlm(const Eigen::Vector4f& v)
    {
        return glm::vec4(v.x(), v.y(), v.z(), v.w());
    }

    inline Eigen::Matrix4d glmToEigen4d(const glm::mat4& m)
    {
        Eigen::Matrix4d r;

        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                r(row, col) = static_cast<double>(m[col][row]);

        return r;
    }

    inline Eigen::Matrix4f glmToEigen(const glm::mat4& m)
    {
        Eigen::Matrix4f r;

        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                r(row, col) = m[col][row];

        return r;
    }

    inline Eigen::Vector3d glmToEigen(const glm::vec3& v)
    {
        return Eigen::Vector3d(v.x, v.y, v.z);
    }

    inline Eigen::Vector3f glmToEigen3f(const glm::vec3& v)
    {
        return Eigen::Vector3f(v.x, v.y, v.z);
    }

    inline Eigen::Vector4d glmToEigen(const glm::vec4& v)
    {
        return Eigen::Vector4d(v.x, v.y, v.z, v.w);
    }

    inline Eigen::Vector4f glmToEigen4f(const glm::vec4& v)
    {
        return Eigen::Vector4f(v.x, v.y, v.z, v.w);
    }

}

