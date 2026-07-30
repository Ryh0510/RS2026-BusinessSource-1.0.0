#include <Utility/EigenPrint.h>

#include <iostream>
#include <iomanip>
#include <sstream>

namespace common
{
    namespace utility
    {
        // Internal format control
        static constexpr int PRINT_PRECISION = 6;


        // =====================================================
        // Vector3
        // =====================================================
        void printVector3(const Eigen::Vector3d& v, const std::string& name)
        {
            if (!name.empty())
                std::cout << name << " = ";

            std::cout << "[ " << v.x() << ", " << v.y() << ", " << v.z() << " ]" << std::endl;
        }

        std::string toString(const Eigen::Vector3d& v)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(PRINT_PRECISION);
            ss << "[ " << v.x() << ", " << v.y() << ", " << v.z() << " ]";
            return ss.str();
        }


        // =====================================================
        // Matrix3
        // =====================================================
        void printMatrix3(const Eigen::Matrix3d& m, const std::string& name, int indent)
        {
            std::string pad(indent, ' ');

            if (!name.empty())
                std::cout << pad << name << " =\n";

            std::cout << std::fixed << std::setprecision(PRINT_PRECISION);

            for (int i = 0; i < 3; ++i)
            {
                std::cout << pad;
                for (int j = 0; j < 3; ++j)
                {
                    std::cout << std::setw(12) << m(i, j) << " ";
                }
                std::cout << "\n";
            }
        }

        std::string toString(const Eigen::Matrix3d& m, int indent_)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(PRINT_PRECISION);

            std::string indentStr(indent_, ' ');
            for (int i = 0; i < 3; ++i)
            {
                ss << indentStr;

                for (int j = 0; j < 3; ++j)
                    ss << std::setw(12) << m(i, j) << " ";
                ss << "\n";
            }
            return ss.str();
        }


        // =====================================================
        // Matrix4
        // =====================================================
        void printMatrix4(const Eigen::Matrix4d& m, const std::string& name)
        {
            if (!name.empty())
                std::cout << name << " =\n";

            std::cout << std::fixed << std::setprecision(PRINT_PRECISION) << m << std::endl;
        }

        void printMatrix4(const Eigen::Matrix4f& m, const std::string& name)
        {
            if (!name.empty())
                std::cout << name << " =\n";

            std::cout << std::fixed << std::setprecision(PRINT_PRECISION) << m << std::endl;
        }

        std::string toString(const Eigen::Matrix4d& m, int indent_)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(PRINT_PRECISION);

            const std::string pad(indent_, ' ');

            for (int i = 0; i < 4; ++i)
            {
                ss << pad;
                for (int j = 0; j < 4; ++j)
                    ss << std::setw(12) << m(i, j) << " ";
                ss << "\n";
            }
            return ss.str();
        }


        // =====================================================
        // Isometry3d
        // =====================================================

        void printIsometry(const Eigen::Isometry3d& T, const std::string& name, int indent)
        {
            std::string pad(indent, ' ');

            if (!name.empty())
                std::cout << pad << name << " =\n";

            std::cout << std::fixed << std::setprecision(PRINT_PRECISION);
            const Eigen::Matrix4d& M = T.matrix();
            for (int i = 0; i < 4; ++i)
            {
                std::cout << pad;
                for (int j = 0; j < 4; ++j)
                {
                    std::cout << std::setw(12) << M(i, j) << " ";
                }
                std::cout << "\n";
            }

            std::cout << std::endl;
        }

        std::string toString(const Eigen::Isometry3d& T, int indent_)
        {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(PRINT_PRECISION);

            const std::string pad(indent_, ' ');
            const Eigen::Matrix4d M = T.matrix();

            for (int i = 0; i < 4; ++i)
            {
                ss << pad;

                for (int j = 0; j < 4; ++j)
                {
                    ss << std::setw(12) << M(i, j) << " ";
                }

                ss << "\n";
            }

            return ss.str();
        }


        // =====================================================
        // RPY output (robot debugging is very important)
        // =====================================================

        void printRPY(
            const Eigen::Isometry3d& T,
            const std::string& name)
        {
            Eigen::Vector3d rpy =
                T.rotation().eulerAngles(0, 1, 2);

            if (!name.empty())
                std::cout << name << " RPY (rad) = ";

            std::cout << "[ "
                << rpy.x() << ", "
                << rpy.y() << ", "
                << rpy.z() << " ]"
                << std::endl;
        }

    } // namespace utility
} // namespace common