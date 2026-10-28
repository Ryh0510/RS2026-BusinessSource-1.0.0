#include <CalibrationInstructionTranslation/ABBTranslation/RapidModuleGenerator.h>

#include <CalibrationInstructionTranslation/TransformUtils.h>

#include <Eigen/Geometry>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>

namespace smrobot::spray::translation
{
    namespace
    {
        std::string number(double value)
        {
            if(std::abs(value) < 5.0e-10) {
                value = 0.0;
            }
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream << std::fixed << std::setprecision(6) << value;
            std::string result = stream.str();
            while(result.size() > 1 && result.back() == '0') {
                result.pop_back();
            }
            if(!result.empty() && result.back() == '.') {
                result.pop_back();
            }
            return result;
        }

        std::string padded2(int value)
        {
            std::ostringstream stream;
            stream << std::setw(2) << std::setfill('0') << value;
            return stream.str();
        }

        std::string padded3(int value)
        {
            std::ostringstream stream;
            stream << std::setw(3) << std::setfill('0') << value;
            return stream.str();
        }

        Eigen::Isometry3d basePose(
            const PublishedTrajectoryPlan& plan,
            const TrajectoryPosePoint& point)
        {
            return plan.baseFromPlanning * point.planningFromTool;
        }

        std::string robtarget(
            const std::string& name,
            const Eigen::Isometry3d& pose,
            const char* externalAxes)
        {
            Eigen::Quaterniond quaternion(pose.linear());
            quaternion.normalize();
            if(quaternion.w() < 0.0) {
                quaternion.coeffs() *= -1.0;
            }
            const Eigen::Vector3d millimeters = pose.translation() * 1000.0;
            std::ostringstream stream;
            stream << "    CONST robtarget " << name << ":=[["
                << number(millimeters.x()) << ','
                << number(millimeters.y()) << ','
                << number(millimeters.z()) << "],["
                << number(quaternion.w()) << ','
                << number(quaternion.x()) << ','
                << number(quaternion.y()) << ','
                << number(quaternion.z())
                << "],[0,0,0,0]," << externalAxes << "];\n";
            return stream.str();
        }

        const TrajectoryPass* findPass(const TrajectoryGroup& group, const std::string& id)
        {
            for(const auto& pass : group.passes) if(pass.id == id) return &pass;
            return nullptr;
        }
        PlanningResult<void> validateGroup(const TrajectoryGroup& group)
        {
            for(const auto& pass : group.passes) {
                if(!pass.trajectory.hasValidPoints()) return PlanningResult<void>::failure(PlanningErrorCode::InvalidArgument, "Empty trajectory pass.");
                for(const auto& point : pass.trajectory.linearPoints) {
                    if(!isFiniteTransform(point.planningFromTool) || !std::isfinite(point.timeSeconds) ||
                        !std::isfinite(point.speedMetersPerSecond) || point.speedMetersPerSecond <= 0.0)
                        return PlanningResult<void>::failure(PlanningErrorCode::InvalidArgument, "Invalid trajectory pose, time or speed.");
                }
            }
            return PlanningResult<void>::success();
        }

        const TrajectoryPass* sequencePass(
            const PublishedTrajectoryPlan& plan,
            const RapidSequenceEntry& entry)
        {
            return entry.kind == RapidSequenceEntryKind::Trajectory
                ? findPass(plan.group, entry.trajectoryPassId)
                : nullptr;
        }

        PlanningResult<Eigen::Matrix3d> safetyOrientation(
            const PublishedTrajectoryPlan& plan,
            const std::vector<RapidSequenceEntry>& sequence,
            std::size_t safetyIndex)
        {
            for(std::size_t index = safetyIndex + 1; index < sequence.size(); ++index) {
                if(const TrajectoryPass* pass = sequencePass(plan, sequence[index])) {
                    return PlanningResult<Eigen::Matrix3d>::success(
                        basePose(plan, pass->trajectory.linearPoints.front()).linear());
                }
            }
            for(std::size_t index = safetyIndex; index-- > 0;) {
                if(const TrajectoryPass* pass = sequencePass(plan, sequence[index])) {
                    return PlanningResult<Eigen::Matrix3d>::success(
                        basePose(plan, pass->trajectory.linearPoints.back()).linear());
                }
            }
            return PlanningResult<Eigen::Matrix3d>::failure(
                PlanningErrorCode::InvalidArgument,
                "A safety point requires an adjacent trajectory from which to inherit orientation.");
        }

        bool coincident(
            const Eigen::Vector3d& lhs,
            const Eigen::Vector3d& rhs) noexcept
        {
            return (lhs - rhs).norm() <= 1.0e-7;
        }
    }

    PlanningResult<RapidModule> RapidModuleGenerator::generate(
        const PublishedTrajectoryPlan& plan,
        const RapidExportSettings& settings,
        const std::vector<RapidSequenceEntry>& sequence)
    {
        if(plan.objectId.empty() || !plan.baseFromPlanning.matrix().allFinite() ||
            !settings.safetyPositionBaseMeters.allFinite() ||
            !std::isfinite(settings.safetySpeedMetersPerSecond) ||
            settings.safetySpeedMetersPerSecond <= 0.0 ||
            !isValidRapidIdentifier(settings.moduleName) ||
            !isValidRapidIdentifier(settings.toolDataName) || !isValidRapidIdentifier(settings.sprayOutputName) || sequence.empty()) {
            return PlanningResult<RapidModule>::failure(
                PlanningErrorCode::InvalidArgument,
                "ABB RAPID settings, workpiece base pose, or instruction sequence is invalid.");
        }
        PlanningResult<void> groupValidation = validateGroup(plan.group);
        if(!groupValidation) {
            return PlanningResult<RapidModule>::failure(
                groupValidation.error.code,
                groupValidation.error.message);
        }

        std::ostringstream declarations;
        std::ostringstream procedure;
        declarations.imbue(std::locale::classic());
        procedure.imbue(std::locale::classic());
        RapidPreviewSteps previewSteps;
        declarations << "    VAR speeddata vSafeCustom:=["
            << number(settings.safetySpeedMetersPerSecond * 1000.0)
            << ",100,5000,1000];\n";
        procedure << "    PROC main()\n"
            << "        ConfJ \\Off;\n"
            << "        ConfL \\Off;\n";

        constexpr const char* externalAxes =
            "[9E9,9E9,9E9,9E9,9E9,9E9]";
        int safetyNumber = 0;
        int trajectoryNumber = 0;
        std::optional<Eigen::Isometry3d> previousTrajectoryEnd;
        for(std::size_t sequenceIndex = 0;
            sequenceIndex < sequence.size(); ++sequenceIndex) {
            const RapidSequenceEntry& entry = sequence[sequenceIndex];
            if(entry.kind == RapidSequenceEntryKind::SafetyPoint) {
                PlanningResult<Eigen::Matrix3d> orientation =
                    safetyOrientation(plan, sequence, sequenceIndex);
                if(previousTrajectoryEnd) {
                    orientation = PlanningResult<Eigen::Matrix3d>::success(
                        previousTrajectoryEnd->linear());
                }
                if(!orientation) {
                    return PlanningResult<RapidModule>::failure(
                        orientation.error.code,
                        orientation.error.message);
                }
                ++safetyNumber;
                const std::string name = "pSafe" + padded3(safetyNumber);
                Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
                pose.translation() = settings.safetyPositionBaseMeters;
                pose.linear() = orientation.value;
                declarations << robtarget(name, pose, externalAxes);
                procedure << "        MoveJ " << name
                    << ",vSafeCustom,fine," << settings.toolDataName
                    << "\\WObj:=wobj0;\n";
                RapidPreviewStep preview;
                preview.sourceKind = RapidSequenceEntryKind::SafetyPoint;
                preview.instruction = "MoveJ";
                preview.targetName = name;
                preview.sequenceIndex = sequenceIndex;
                preview.baseFromTool = pose;
                previewSteps.push_back(std::move(preview));
                previousTrajectoryEnd.reset();
                continue;
            }

            const TrajectoryPass* pass = sequencePass(plan, entry);
            if(pass == nullptr || !pass->trajectory.hasValidPoints()) {
                return PlanningResult<RapidModule>::failure(
                    PlanningErrorCode::InvalidArgument,
                    "The ABB instruction sequence references a missing trajectory.");
            }
            ++trajectoryNumber;
            bool sprayOn = false;
            for(std::size_t pointIndex = 0; pointIndex < pass->trajectory.linearPoints.size(); ++pointIndex) {
                const auto& point = pass->trajectory.linearPoints[pointIndex];
                const auto pose = basePose(plan, point);
                const std::string suffix = padded3(trajectoryNumber) + "_" + std::to_string(pointIndex + 1);
                const std::string targetName = "pTraj" + suffix;
                const std::string speedName = "vTraj" + suffix;
                declarations << "    VAR speeddata " << speedName << ":=["
                    << number(point.speedMetersPerSecond * 1000.0) << ",100,5000,1000];\n"
                    << robtarget(targetName, pose, externalAxes);
                // Travel to the first point with spraying disabled, then honor each source edge.
                const bool nextSpray = pointIndex > 0 && pass->trajectory.linearPoints[pointIndex - 1].sprayEnabled;
                if(nextSpray != sprayOn) {
                    procedure << "        SetDO " << settings.sprayOutputName << ',' << (nextSpray ? 1 : 0) << ";\n";
                    sprayOn = nextSpray;
                }
                const char* instruction = pointIndex == 0 && !previousTrajectoryEnd ? "MoveJ" : "MoveL";
                procedure << "        " << instruction << ' ' << targetName << ',' << speedName
                    << ",fine," << settings.toolDataName << "\\WObj:=wobj0;\n";
                RapidPreviewStep preview;
                preview.sourceKind = RapidSequenceEntryKind::Trajectory;
                preview.instruction = instruction;
                preview.targetName = targetName;
                preview.trajectoryPassId = entry.trajectoryPassId;
                preview.sequenceIndex = sequenceIndex;
                preview.baseFromTool = pose;
                previewSteps.push_back(std::move(preview));
                previousTrajectoryEnd = pose;
            }
            if(sprayOn) procedure << "        SetDO " << settings.sprayOutputName << ",0;\n";
        }
        procedure << "    ENDPROC\n";

        RapidModule module;
        std::ostringstream output;
        output << "MODULE " << settings.moduleName << "\n"
            << "    ! Generated by RWS Translation.\n"
            << "    ! Workpiece-local poses are converted with T_base_planning.\n"
            << "    ! Verify reachability, collisions, TCP and robot configuration in RobotStudio.\n\n"
            << declarations.str() << '\n'
            << procedure.str()
            << "ENDMODULE\n";
        module.code = output.str();
        module.previewSteps = std::move(previewSteps);
        return PlanningResult<RapidModule>::success(std::move(module));
    }

    PlanningResult<RapidModule> RapidModuleGenerator::generateScheme(
        const PublishedTrajectoryPlan& plan,
        const RapidExportSettings& settings,
        const std::vector<RapidSequenceEntry>& sequence)
    { return generate(plan, settings, sequence); }

    bool RapidModuleGenerator::isValidRapidIdentifier(const std::string& value) noexcept
    {
        if(value.empty() || value.size() > 32 ||
            !std::isalpha(static_cast<unsigned char>(value.front()))) {
            return false;
        }
        return std::all_of(
            value.begin() + 1,
            value.end(),
            [](char character) {
                const unsigned char value = static_cast<unsigned char>(character);
                return std::isalnum(value) || character == '_';
            });
    }
}
