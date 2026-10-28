#pragma once
#include "Models/CalibrationTranslationViewModel.h"
#include <PostModel/PostProgramRunner.h>
#include <SprayTrajectoryCore/SprayTrajectory.h>
#include <QObject>
#include <QString>
namespace robot_qt_viewer { class RobotQtViewerDocumentContext; struct RobotQtViewerEvent; }
namespace smrobot::workbench::spray::translation {
class WorkpieceCalibrationPanel; class ABBTranslationPanel; class PostProcessingPanel;
struct TranslationControllerResult { bool success{false}; QString message; };
class CalibrationTranslationModuleController final : public QObject {
Q_OBJECT
public:
CalibrationTranslationModuleController(robot_qt_viewer::RobotQtViewerDocumentContext&, WorkpieceCalibrationPanel&, ABBTranslationPanel&, PostProcessingPanel&, QObject* parent=nullptr);
void setLanguageCode(const QString&); QString languageCode() const; void refresh();
void synchronize(const QString& objectId, const spraytrajectory::SprayTrajectory& trajectory);
void clear(); void handleEvent(const robot_qt_viewer::RobotQtViewerEvent& event);
signals:
void statusMessageRequested(const QString&, int);
private:
CalibrationTranslationViewModel viewModel() const;
TranslationControllerResult updateCalibrationWorkspace(const WorkpieceCalibrationWorkspace&);
TranslationControllerResult updateBaseComponents(const domain::TransformComponents&);
TranslationControllerResult updateRapidSettings(const domain::RapidExportSettings&);
TranslationControllerResult updateRapidSequence(const std::vector<domain::RapidSequenceEntry>&);
TranslationControllerResult confirmFrame(); void setPublishFrame(PublishFrame);
TranslationControllerResult generateAndSaveRapidModule();
void setSelectedRapidPreviewStep(std::optional<std::size_t>);
TranslationControllerResult loadCurrentTrajectoryForPostProcessing(const std::string&);
TranslationControllerResult generatePostProcessedProgram(const std::string&,const std::string&,const std::string&,const std::string&);
void report(const TranslationControllerResult&,int timeoutMs=3500);
void invalidateOutput(); void publishState(const QString& message=QString(),int timeoutMs=3500);
domain::PublishedTrajectoryPlan plan() const;
struct PostProcessingState {
post_model::ProgramRequest request; std::string robotId,preview,status; std::vector<std::string> outputFiles;
bool trajectoryLoaded{false}, exportSucceeded{false};
};
robot_qt_viewer::RobotQtViewerDocumentContext& m_context;
WorkpieceCalibrationPanel& m_calibrationPanel; ABBTranslationPanel& m_abbPanel; PostProcessingPanel& m_postProcessingPanel;
CalibrationTranslationViewModel m_view; std::string m_objectId;
std::unique_ptr<PostProcessingState> m_postProcessingState = std::make_unique<PostProcessingState>();
QString m_languageCode{QStringLiteral("en")}; bool m_updating{false}; bool m_busy{false};
}; }
