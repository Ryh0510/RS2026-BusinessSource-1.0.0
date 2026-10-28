#pragma once
#include <CalibrationInstructionTranslation/Calibration/WorkpieceCalibration.h>
#include <CalibrationInstructionTranslation/ABBTranslation/RapidModuleGenerator.h>
#include <array>
#include <optional>
namespace smrobot::workbench::spray::translation {
namespace domain = smrobot::spray::translation;
enum class PublishFrame { BaseFrame, PlanningLocalFrame };
    using CalibrationCoordinateRow = std::array<std::optional<double>, 3>;

    struct WorkpieceCalibrationWorkspace
    {
        std::array<CalibrationCoordinateRow, 12> cylinderRows;
        std::array<CalibrationCoordinateRow, 6> circleRows;
        CalibrationCoordinateRow topReference;
        CalibrationCoordinateRow yDirectionStart;
        CalibrationCoordinateRow yDirectionEnd;
        double workpieceHeightMeters{ 0.12 };
        domain::CalibrationMode activeMode{ domain::CalibrationMode::Cylinder3d };
        domain::CalibrationMode axisSource{ domain::CalibrationMode::Cylinder3d };
        bool showCylinder{ true };
        bool showCircle{ true };
        std::optional<domain::CalibrationAxisFit> cylinderFit;
        std::optional<domain::CalibrationAxisFit> circleFit;
        std::optional<domain::WorkpieceFrameCalibration> frame;
    };

    struct PostProcessingRobotOption
    {
        std::string id;
        std::string name;
    };
struct CalibrationTranslationViewModel {
        domain::TransformComponents baseFromPlanning;
        PublishFrame publishFrame{ PublishFrame::BaseFrame };
        domain::TrajectoryWorkspace trajectoryWorkspace;
        domain::RapidExportSettings rapidSettings;
        std::vector<domain::RapidSequenceEntry> rapidSequence;
        WorkpieceCalibrationWorkspace calibrationWorkspace;
        std::string calibrationInputDirectory;
        std::optional<domain::RapidModule> rapidModulePreview;
        std::string rapidOutputFile;
        std::string rapidExportStatus;
        bool rapidExportSucceeded{ false };
        std::vector<PostProcessingRobotOption> postProcessingRobots;
        std::vector<std::string> postProcessingTemplates;
        std::string postProcessingRobotId;
        std::string postProcessingPreview;
        std::vector<std::string> postProcessingOutputFiles;
        std::string postProcessingStatus;
        bool postProcessingTrajectoryLoaded{ false };
        bool postProcessingExportSucceeded{ false };
bool hasModel{false}; bool canConfirmFrame{false}; bool isBusy{false};
bool canExportRapid{false}; bool canLoadPostProcessingTrajectory{false}; bool canExportPostProcessedProgram{false};
};
}
