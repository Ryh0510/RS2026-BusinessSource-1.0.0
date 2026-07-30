#pragma once

#include <Eigen/Dense>
#include <string>
#include <iostream>
#include <iomanip>

const int PRINT_PRECISION = 4;


namespace common
{
    namespace utility
    {

        // ===============================
        // Basic Printing
        // ===============================

        void printVector3(const Eigen::Vector3d& v, const std::string& name = "");
        std::string toString(const Eigen::Vector3d& v);

        void printMatrix3(const Eigen::Matrix3d& m, const std::string& name = "", int indent = 0);
        std::string toString(const Eigen::Matrix3d& m, int indent_ = 0);

        void printMatrix4(const Eigen::Matrix4d& m, const std::string& name = "");
        void printMatrix4(const Eigen::Matrix4f& m, const std::string& name = "");
        std::string toString(const Eigen::Matrix4d& m, int indent_ = 0);
        
        void printIsometry(const Eigen::Isometry3d& T, const std::string& name = "", int indent = 0);
        std::string toString(const Eigen::Isometry3d& T, int indent_ = 0);

        template<typename Derived>
        void printMatrix(
            const Eigen::MatrixBase<Derived>& m,
            const std::string& name,
            int indent)
        {
            std::string pad(indent, ' ');

            if (!name.empty())
                std::cout << pad << name << " =\n";

            std::cout << std::fixed
                << std::setprecision(PRINT_PRECISION);

            for (int i = 0; i < m.rows(); ++i)
            {
                std::cout << pad;
                for (int j = 0; j < m.cols(); ++j)
                    std::cout << std::setw(12) << m(i, j) << " ";
                std::cout << "\n";
            }
        }


        // ===============================
        // Convert to string (for logging system)
        // ===============================

        
        
        

        //std::string toStringVec3(const Eigen::Vector3d& v);


        // ===============================
        // Debugging only
        // ===============================

        void printRPY(
            const Eigen::Isometry3d& T,
            const std::string& name = "");

    } // namespace utility
} // namespace common

