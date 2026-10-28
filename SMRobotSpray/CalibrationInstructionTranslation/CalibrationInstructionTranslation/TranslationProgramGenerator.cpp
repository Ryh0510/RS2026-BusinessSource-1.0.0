#include "TranslationProgramGenerator.h"

#include <Eigen/Geometry>

#include <iomanip>
#include <locale>
#include <sstream>

namespace calibrationtranslation
{
    namespace
    {
        std::string number(double value)
        {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::fixed << std::setprecision(3) << value;
            return stream.str();
        }

        std::string target(const std::string& name, const Eigen::Isometry3d& pose)
        {
            Eigen::Quaterniond quaternion(pose.rotation());
            quaternion.normalize();
            const Eigen::Vector3d millimeters = pose.translation() * 1000.0;
            std::ostringstream stream;
            stream << "    CONST robtarget " << name << ":=[["
                << number(millimeters.x()) << ',' << number(millimeters.y()) << ','
                << number(millimeters.z()) << "],[" << number(quaternion.w()) << ','
                << number(quaternion.x()) << ',' << number(quaternion.y()) << ','
                << number(quaternion.z()) << "],[0,0,0,0],[9E9,9E9,9E9,9E9,9E9,9E9]];\n";
            return stream.str();
        }
    }

    RapidProgram TranslationProgramGenerator::generate(
        const spraytrajectory::SprayTrajectory& trajectory,
        const RapidExportSettings& settings)
    {
        RapidProgram result;
        std::ostringstream declarations;
        std::ostringstream procedure;
        declarations.imbue(std::locale::classic());
        procedure.imbue(std::locale::classic());
        declarations << "    VAR speeddata vSafeCustom:=["
            << number(settings.safetySpeedMetersPerSecond * 1000.0) << ",100,5000,1000];\n";
        procedure << "    PROC main()\n"
            << "        ConfJ \\Off;\n"
            << "        ConfL \\Off;\n";

        int targetIndex = 0;
        for(const spraytrajectory::SpraySegment& segment : trajectory.segments) {
            if(segment.points.empty()) continue;
            const std::string prefix = "pTraj" + std::to_string(++targetIndex);
            for(std::size_t pointIndex = 0; pointIndex < segment.points.size(); ++pointIndex) {
                declarations << target(
                    prefix + "P" + std::to_string(pointIndex + 1),
                    segment.points[pointIndex].tcpPose);
            }
            procedure << "        MoveJ " << prefix << "P1,vSafeCustom,fine,"
                << settings.toolDataName << "\\WObj:=wobj0;\n";
            if(segment.sprayEnabled) {
                procedure << "        ! Spray ON\n";
            }
            for(std::size_t pointIndex = 1; pointIndex < segment.points.size(); ++pointIndex) {
                procedure << "        MoveL " << prefix << "P" << pointIndex + 1
                    << ",vSafeCustom,fine," << settings.toolDataName << "\\WObj:=wobj0;\n";
            }
            if(segment.sprayEnabled) {
                procedure << "        ! Spray OFF\n";
            }
        }
        if(targetIndex == 0) {
            result.warnings.push_back("The current freeform trajectory has no exportable segments.");
            return result;
        }
        procedure << "    ENDPROC\n";
        result.text = "MODULE " + settings.moduleName + "\n"
            "    ! Generated from the current freeform trajectory.\n\n" +
            declarations.str() + "\n" + procedure.str() + "ENDMODULE\n";
        return result;
    }
}
