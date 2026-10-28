#include "CalibrationTranslationModuleController.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerEvents.h"
#include "RobotQtViewerViewportServices.h"
#include <CalibrationInstructionTranslation/TransformUtils.h>
#include <QSaveFile>
#include <QDir>
#include <ProjectRuntimeBuilder.h>
#include <SimulationProject/AssetResolver.h>
#include <SimulationProject/ProjectSession.h>
#include <RobotInstance/RobotInstance.h>
#include <MotionPlanningIk/SprayIkPlanner.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <Models/CalibrationTranslationTranslations.h>
#include <Widgets/ABBTranslationPanel.h>
#include <Widgets/PostProcessingPanel.h>
#include <Widgets/WorkpieceCalibrationPanel.h>

#include <QFileInfo>

namespace smrobot::workbench::spray::translation
{
    namespace {
        Eigen::Isometry3d projectTransform(
            const simulation_project::TransformDesc& transform)
        {
            Eigen::Isometry3d result = Eigen::Isometry3d::Identity();
            result.translation() = Eigen::Vector3d(transform.x, transform.y, transform.z);
            result.linear() =
                Eigen::AngleAxisd(transform.yaw, Eigen::Vector3d::UnitZ()).toRotationMatrix() *
                Eigen::AngleAxisd(transform.pitch, Eigen::Vector3d::UnitY()).toRotationMatrix() *
                Eigen::AngleAxisd(transform.roll, Eigen::Vector3d::UnitX()).toRotationMatrix();
            return result;
        }

        std::array<double, 16> postPose(
            const Eigen::Isometry3d& transform,
            double translationScale)
        {
            std::array<double, 16> result{};
            for(int row = 0; row < 4; ++row) {
                for(int column = 0; column < 4; ++column) {
                    result[static_cast<std::size_t>(row * 4 + column)] =
                        transform.matrix()(row, column);
                }
            }
            result[3] *= translationScale;
            result[7] *= translationScale;
            result[11] *= translationScale;
            return result;
        }

        const simulation_project::RobotDesc* findRobot(
            const simulation_project::ProjectDocument& document,
            const std::string& robotId)
        {
            const auto iterator = std::find_if(
                document.robots.begin(), document.robots.end(),
                [&](const simulation_project::RobotDesc& robot) {
                    return robot.id == robotId;
                });
            return iterator == document.robots.end() ? nullptr : &(*iterator);
        }

        struct ToolDescription
        {
            std::string tcpLinkName;
            Eigen::Isometry3d linkFromTcp = Eigen::Isometry3d::Identity();
            bool mountedToolFound{ false };
        };

        ToolDescription findToolDescription(
            const simulation_project::ProjectDocument& document,
            const robot::RobotModel& model,
            const std::string& robotId)
        {
            for(const simulation_project::RobotMountDesc& mount : document.robotMounts) {
                if(mount.robotId != robotId || mount.linkName.empty()) {
                    continue;
                }
                for(const simulation_project::MountedAttachmentDesc& attachment :
                    document.mountedAttachments) {
                    if(!attachment.enabled || attachment.mountFrameId != mount.id) {
                        continue;
                    }
                    const auto asset = std::find_if(
                        document.attachmentAssets.begin(),
                        document.attachmentAssets.end(),
                        [&](const simulation_project::AttachmentAssetDesc& candidate) {
                            return candidate.id == attachment.assetId;
                        });
                    if(asset == document.attachmentAssets.end()) {
                        continue;
                    }
                    const simulation_project::AttachmentFunctionalFrameDesc* frame = nullptr;
                    for(const auto& candidate : asset->functionalFrames) {
                        if(candidate.primary) {
                            frame = &candidate;
                            break;
                        }
                    }
                    if(frame == nullptr && !asset->functionalFrames.empty()) {
                        frame = &asset->functionalFrames.front();
                    }
                    ToolDescription result;
                    result.tcpLinkName = mount.linkName;
                    result.linkFromTcp = projectTransform(mount.linkToMount) *
                        projectTransform(attachment.mountToAssetMount);
                    if(frame != nullptr) {
                        result.linkFromTcp = result.linkFromTcp *
                            projectTransform(frame->assetMountToFrame);
                    }
                    result.mountedToolFound = true;
                    return result;
                }
            }

            ToolDescription result;
            if(!model.tip_links.empty()) {
                result.tcpLinkName = model.tip_links.front();
            } else if(!model.linkNames.empty()) {
                result.tcpLinkName = model.linkNames.back();
            }
            return result;
        }

        std::vector<std::string> sixAxisJointNames(const robot::RobotModel& model)
        {
            std::vector<const robot::RobotJoint*> joints;
            for(const robot::RobotJoint& joint : model.joints) {
                if(joint.dofIndex >= 0) {
                    joints.push_back(&joint);
                }
            }
            std::sort(joints.begin(), joints.end(),
                [](const robot::RobotJoint* lhs, const robot::RobotJoint* rhs) {
                    return lhs->dofIndex < rhs->dofIndex;
                });
            std::vector<std::string> result;
            result.reserve(joints.size());
            for(const robot::RobotJoint* joint : joints) {
                result.push_back(joint->name);
            }
            return result;
        }

        std::vector<double> currentJointSeed(
            const simulation_project::RobotDesc& robot,
            const std::vector<std::string>& jointNames,
            const robot_qt_viewer::RobotQtViewerViewportServices* viewportServices)
        {
            std::unordered_map<std::string, double> initial;
            for(const simulation_project::JointValueDesc& joint : robot.initialJoints) {
                initial[joint.jointName] = joint.value;
            }
            std::vector<double> result;
            result.reserve(jointNames.size());
            for(const std::string& jointName : jointNames) {
                bool currentValueAvailable = false;
                double value = viewportServices != nullptr
                    ? viewportServices->robotJointValue(
                        QString::fromStdString(robot.id),
                        QString::fromStdString(jointName),
                        &currentValueAvailable)
                    : 0.0;
                if(!currentValueAvailable) {
                    const auto configured = initial.find(jointName);
                    value = configured != initial.end() ? configured->second : 0.0;
                }
                result.push_back(value);
            }
            return result;
        }

        std::string intermediateProgramPreview(
            const std::string& robotName,
            const std::string& tcpLinkName,
            bool mountedToolFound,
            const post_model::ProgramRequest& request)
        {
            std::ostringstream stream;
            stream << "Robot: " << robotName << "\n"
                << "TCP link: " << tcpLinkName << "\n"
                << "Tool: " << (mountedToolFound ? "mounted tool TCP" : "flange identity") << "\n"
                << "Commands: " << request.commands.size() << "\n\n";
            stream << std::fixed << std::setprecision(6);
            for(std::size_t index = 0; index < request.commands.size(); ++index) {
                const post_model::MotionCommand& command = request.commands[index];
                stream << (command.type == post_model::MotionType::Joint ? "MoveJ" : "MoveL")
                    << " P" << (index + 1) << "  joints(deg)={";
                for(std::size_t joint = 0; joint < command.jointsDegrees.size(); ++joint) {
                    if(joint > 0) stream << ", ";
                    stream << command.jointsDegrees[joint];
                }
                stream << "}  speed(mm/s)=" << command.speedMillimetersPerSecond << "\n";
            }
            return stream.str();
        }
}

    CalibrationTranslationModuleController::CalibrationTranslationModuleController(
        robot_qt_viewer::RobotQtViewerDocumentContext& context,
        WorkpieceCalibrationPanel& calibrationPanel,
        ABBTranslationPanel& abbPanel,
        PostProcessingPanel& postProcessingPanel,
        QObject* parent)
        : QObject(parent)
        , m_context(context)
        , m_calibrationPanel(calibrationPanel)
        , m_abbPanel(abbPanel)
        , m_postProcessingPanel(postProcessingPanel)
    {

        connect(&m_calibrationPanel, &WorkpieceCalibrationPanel::workspaceEdited,
            this, [this](const WorkpieceCalibrationWorkspace& workspace) {
                if(m_updating) return;
                report(updateCalibrationWorkspace(workspace));
            });
        connect(&m_calibrationPanel, &WorkpieceCalibrationPanel::baseTransformCalculated,
            this, [this](const domain::TransformComponents& components) {
                report(updateBaseComponents(components), 3500);
            });
        connect(&m_calibrationPanel, &WorkpieceCalibrationPanel::modeTwoDataImported,
            this, [this](const Eigen::Vector3d& safetyPositionBaseMeters,
                const QString& filePath) {
                domain::RapidExportSettings settings =
                    viewModel().rapidSettings;
                settings.safetyPositionBaseMeters = safetyPositionBaseMeters;
                TranslationControllerResult result =
                    updateRapidSettings(settings);
                if(result.success) {
                    result.message = CalibrationTranslationTranslations::text(
                        m_languageCode, "calibration.import_success")
                        .arg(QFileInfo(filePath).fileName());
                }
                report(result, 4000);
            });
        connect(&m_calibrationPanel, &WorkpieceCalibrationPanel::publishFrameChanged,
            this, [this](PublishFrame frame) {
                setPublishFrame(frame);
                refresh();
            });
        connect(&m_calibrationPanel, &WorkpieceCalibrationPanel::confirmFrameRequested,
            this, [this]() { report(confirmFrame(), 3500); });
        connect(&m_abbPanel, &ABBTranslationPanel::settingsEdited,
            this, [this](const domain::RapidExportSettings& settings) {
                report(updateRapidSettings(settings), 2500);
            });
        connect(&m_abbPanel, &ABBTranslationPanel::sequenceEdited,
            this, [this](const std::vector<domain::RapidSequenceEntry>& sequence) {
                report(updateRapidSequence(sequence), 2500);
            });
        connect(&m_abbPanel, &ABBTranslationPanel::generateRequested,
            this, [this](const domain::RapidExportSettings& settings,
                const std::vector<domain::RapidSequenceEntry>& sequence) {
                TranslationControllerResult result = updateRapidSettings(settings);
                if(result.success) result = updateRapidSequence(sequence);
                if(result.success) result = generateAndSaveRapidModule();
                report(result, 5000);
            });
        connect(&m_abbPanel, &ABBTranslationPanel::previewStepSelected,
            this, [this](int index) {
                setSelectedRapidPreviewStep(index >= 0
                    ? std::optional<std::size_t>(static_cast<std::size_t>(index))
                    : std::nullopt);
            });
        connect(&m_postProcessingPanel,
            &PostProcessingPanel::loadCurrentTrajectoryRequested,
            this,
            [this](const QString& robotId) {
                report(loadCurrentTrajectoryForPostProcessing(
                    robotId.toUtf8().toStdString()), 6000);
            });
        connect(&m_postProcessingPanel,
            &PostProcessingPanel::generateRequested,
            this,
            [this](const QString& templateName,
                const QString& robotId,
                const QString& programName,
                const QString& outputDirectory) {
                report(generatePostProcessedProgram(
                    templateName.toUtf8().toStdString(),
                    robotId.toUtf8().toStdString(),
                    programName.toUtf8().toStdString(),
                    outputDirectory.toUtf8().toStdString()), 6000);
            });
        m_view.postProcessingTemplates = post_model::PostProgramRunner::availableTemplates();
        refresh();
    }

    void CalibrationTranslationModuleController::setLanguageCode(const QString& languageCode)
    {
        m_languageCode = CalibrationTranslationTranslations::canonicalLanguageCode(languageCode);
        m_calibrationPanel.setLanguageCode(m_languageCode);
        m_abbPanel.setLanguageCode(m_languageCode);
        m_postProcessingPanel.setLanguageCode(m_languageCode);
        refresh();
    }

    QString CalibrationTranslationModuleController::languageCode() const
    {
        return m_languageCode;
    }

    void CalibrationTranslationModuleController::refresh()
    {
        m_updating = true;
        const CalibrationTranslationViewModel viewModel = this->viewModel();
        m_calibrationPanel.setViewModel(viewModel);
        m_abbPanel.setViewModel(viewModel);
        m_postProcessingPanel.setViewModel(viewModel);
        m_updating = false;
    }

    void CalibrationTranslationModuleController::report(
        const TranslationControllerResult& result,
        int timeoutMs)
    {
        refresh();
        if(!result.message.isEmpty()) {
            emit statusMessageRequested(result.message, timeoutMs);
        }
    }

    CalibrationTranslationViewModel CalibrationTranslationModuleController::viewModel() const
    {
        auto view = m_view;
        for(const auto& robot : m_context.document().robots) view.postProcessingRobots.push_back({robot.id, robot.name});
        view.postProcessingRobotId = m_postProcessingState->robotId;
        view.postProcessingPreview = m_postProcessingState->preview;
        view.postProcessingOutputFiles = m_postProcessingState->outputFiles;
        view.postProcessingStatus = m_postProcessingState->status;
        view.postProcessingTrajectoryLoaded = m_postProcessingState->trajectoryLoaded;
        view.postProcessingExportSucceeded = m_postProcessingState->exportSucceeded;
        view.canExportRapid = !view.trajectoryWorkspace.group.passes.empty();
        view.canLoadPostProcessingTrajectory = view.canExportRapid && !view.postProcessingRobots.empty();
        view.canExportPostProcessedProgram = m_postProcessingState->trajectoryLoaded;
        return view;
    }
    void CalibrationTranslationModuleController::invalidateOutput()
    {
        m_view.rapidModulePreview.reset(); m_view.rapidExportSucceeded=false;
        m_view.rapidExportStatus.clear(); m_view.rapidOutputFile.clear();
        *m_postProcessingState = {};
    }
    void CalibrationTranslationModuleController::clear()
    {
        m_objectId.clear(); m_view = {}; invalidateOutput();
        m_view.postProcessingTemplates = post_model::PostProgramRunner::availableTemplates(); refresh();
    }
    void CalibrationTranslationModuleController::handleEvent(const robot_qt_viewer::RobotQtViewerEvent& event)
    {
        if(event.kind == robot_qt_viewer::RobotQtViewerEventKind::ProjectOpened) clear();
        else if(event.kind == robot_qt_viewer::RobotQtViewerEventKind::ProjectDocumentChanged) { invalidateOutput(); refresh(); }
    }
    void CalibrationTranslationModuleController::synchronize(const QString& objectId, const spraytrajectory::SprayTrajectory& trajectory)
    {
        if(objectId.isEmpty()) { clear(); return; }
        if(m_objectId != objectId.toStdString()) clear();
        m_objectId = objectId.toStdString(); m_view.hasModel = true; m_view.canConfirmFrame = true;
        invalidateOutput(); m_view.trajectoryWorkspace.group.passes.clear(); m_view.rapidSequence.clear();
        int index=0;
        for(const auto& segment : trajectory.segments) {
            if(segment.points.empty()) continue;
            domain::TrajectoryPass pass; pass.order=++index; pass.id="freeform_"+std::to_string(index);
            for(std::size_t i=0;i<segment.points.size();++i) {
                const auto& source=segment.points[i]; domain::TrajectoryPosePoint point;
                point.timeSeconds=source.time; point.planningFromTool=source.tcpPose;
                point.sprayEnabled=segment.sprayEnabled && source.sprayEnabled;
                point.speedMetersPerSecond=source.velocity;
                if(point.speedMetersPerSecond<=0.0 && segment.points.size()>1) {
                    const auto& a=segment.points[i>0?i-1:i]; const auto& b=segment.points[i>0?i:i+1];
                    const double dt=b.time-a.time;
                    if(dt>0.0) point.speedMetersPerSecond=(b.tcpPose.translation()-a.tcpPose.translation()).norm()/dt;
                }
                pass.trajectory.linearPoints.push_back(point);
            }
            pass.trajectory.parameters.speedMetersPerSecond=pass.trajectory.linearPoints.front().speedMetersPerSecond;
            m_view.rapidSequence.push_back({domain::RapidSequenceEntryKind::Trajectory,pass.id});
            m_view.trajectoryWorkspace.group.passes.push_back(std::move(pass));
        }
        refresh();
    }
    domain::PublishedTrajectoryPlan CalibrationTranslationModuleController::plan() const
    {
        domain::PublishedTrajectoryPlan result; result.objectId=m_objectId;
        result.baseFromPlanning=m_view.publishFrame==PublishFrame::BaseFrame ? domain::makeTransform(m_view.baseFromPlanning) : Eigen::Isometry3d::Identity();
        result.group=m_view.trajectoryWorkspace.group; result.executionSequence=m_view.rapidSequence; return result;
    }
    TranslationControllerResult CalibrationTranslationModuleController::updateCalibrationWorkspace(const WorkpieceCalibrationWorkspace& workspace)
    { m_view.calibrationWorkspace=workspace; return {true,{}}; }
    TranslationControllerResult CalibrationTranslationModuleController::updateBaseComponents(const domain::TransformComponents& components)
    {
        if(!domain::isFiniteTransform(domain::makeTransform(components))) return {false,QStringLiteral("Invalid calibration transform.")};
        m_view.baseFromPlanning=components; invalidateOutput(); return {true,QStringLiteral("Calibration transform updated.")};
    }
    TranslationControllerResult CalibrationTranslationModuleController::updateRapidSettings(const domain::RapidExportSettings& settings)
    { m_view.rapidSettings=settings; invalidateOutput(); return {true,{}}; }
    TranslationControllerResult CalibrationTranslationModuleController::updateRapidSequence(const std::vector<domain::RapidSequenceEntry>& sequence)
    { m_view.rapidSequence=sequence; invalidateOutput(); return {true,{}}; }
    TranslationControllerResult CalibrationTranslationModuleController::confirmFrame()
    { return {m_view.hasModel, m_view.hasModel ? QStringLiteral("Translation frame confirmed.") : QStringLiteral("Load a freeform model first.")}; }
    void CalibrationTranslationModuleController::setPublishFrame(PublishFrame frame)
    { m_view.publishFrame=frame; invalidateOutput(); }
    void CalibrationTranslationModuleController::setSelectedRapidPreviewStep(std::optional<std::size_t> index)
    {
        if(index && m_view.rapidModulePreview && *index<m_view.rapidModulePreview->previewSteps.size())
            emit statusMessageRequested(QString::fromStdString(m_view.rapidModulePreview->previewSteps[*index].targetName),2000);
    }
    void CalibrationTranslationModuleController::publishState(const QString& message,int timeoutMs)
    { refresh(); if(!message.isEmpty()) emit statusMessageRequested(message,timeoutMs); }
    TranslationControllerResult CalibrationTranslationModuleController::generateAndSaveRapidModule()
    {
        auto generated=domain::RapidModuleGenerator::generate(plan(),m_view.rapidSettings,m_view.rapidSequence);
        if(!generated) return {false,QString::fromStdString(generated.error.message)};
        const auto& settings=m_view.rapidSettings;
        if(settings.outputDirectory.empty() || settings.fileName.empty()) return {false,QStringLiteral("Choose an output directory and file name.")};
        QDir directory(QString::fromStdString(settings.outputDirectory));
        if(!directory.exists() && !directory.mkpath(QStringLiteral("."))) return {false,QStringLiteral("Cannot create output directory.")};
        QString name=QFileInfo(QString::fromStdString(settings.fileName)).fileName();
        if(QFileInfo(name).suffix().isEmpty()) name+=QStringLiteral(".mod");
        const QString path=directory.filePath(name); QSaveFile file(path);
        const QByteArray bytes=QByteArray::fromStdString(generated.value.code);
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit()) return {false,file.errorString()};
        m_view.rapidModulePreview=std::move(generated.value); m_view.rapidExportSucceeded=true;
        m_view.rapidOutputFile=path.toStdString(); m_view.rapidExportStatus="Exported";
        return {true,QStringLiteral("RAPID exported: %1").arg(path)};
    }
    TranslationControllerResult
    CalibrationTranslationModuleController::loadCurrentTrajectoryForPostProcessing(
        const std::string& robotId)
    {
        if(m_busy) {
            return {false,QStringLiteral("Translation is busy.")};
        }
        if(m_postProcessingState == nullptr) {
            m_postProcessingState = std::make_unique<PostProcessingState>();
        }
        *m_postProcessingState = {};
        const simulation_project::ProjectDocument& document = m_context.document();
        const simulation_project::RobotDesc* robotDesc = findRobot(document, robotId);
        if(robotDesc == nullptr) {
            const QString message = QStringLiteral("Select a robot that exists in the current project.");
            m_postProcessingState->status = message.toStdString();
            publishState(message, 5000);
            return { false, message };
        }

        try {
            const std::filesystem::path projectPath = m_context.projectSession().path();
            const std::filesystem::path projectBasePath = projectPath.empty()
                ? std::filesystem::current_path()
                : (std::filesystem::is_directory(projectPath)
                    ? projectPath
                    : projectPath.parent_path());
            const simulation_project::AssetResolveContext resolveContext =
                simulation_project::AssetResolver::makeProjectContext(
                    projectBasePath, document);
            std::filesystem::path robotPath;
            if(robotDesc->sourcePath.find("://") != std::string::npos &&
                !simulation_project::AssetResolver::isAppGeneratedAssetReference(
                    robotDesc->sourcePath)) {
                const assetcore::AssetResolveResult resolved =
                    simulation_project::AssetResolver::resolveProjectReference(
                        resolveContext, robotDesc->sourcePath);
                if(!resolved.success()) {
                    throw std::runtime_error(
                        "Robot asset resolution failed: " + robotDesc->sourcePath +
                        (resolved.diagnostic.empty()
                            ? std::string()
                            : " (" + resolved.diagnostic + ")"));
                }
                robotPath = resolved.resolvedPath;
            } else {
                robotPath = simulation_project::AssetResolver::resolveProjectPath(
                    resolveContext, robotDesc->sourcePath);
            }

            robot::RobotModel robotModel = ProjectRuntimeBuilder::loadSingleRobot(
                robotPath,
                robotDesc->sourceType,
                robotDesc->sourceModelIndex,
                simulation_project::AssetResolver::urdfPackageRootsForReference(
                    resolveContext, robotDesc->sourcePath));
            const std::vector<std::string> jointNames = sixAxisJointNames(robotModel);
            if(jointNames.size() != 6) {
                throw std::runtime_error(
                    "Post Processing requires a six-axis robot; the selected model has " +
                    std::to_string(jointNames.size()) + " movable joints.");
            }
            const ToolDescription tool = findToolDescription(
                document, robotModel, robotDesc->id);
            if(tool.tcpLinkName.empty() ||
                robotModel.links.find(tool.tcpLinkName) == robotModel.links.end()) {
                throw std::runtime_error(
                    "A valid robot flange or TCP link could not be determined.");
            }
            const std::vector<double> seed = currentJointSeed(
                *robotDesc, jointNames, m_context.viewportServices());

            robotinstance::RobotInstance initialRobot(robotModel, "post_processing_seed");
            initialRobot.setBaseTransform(Eigen::Isometry3d::Identity());
            initialRobot.setJoints(seed);
            initialRobot.update();
            const Eigen::Isometry3d initialBaseFromTcp =
                initialRobot.getLinkTransform(tool.tcpLinkName) * tool.linkFromTcp;

            const auto exportPlan = plan();
            const auto generated = domain::RapidModuleGenerator::generate(exportPlan,m_view.rapidSettings,m_view.rapidSequence);
            if(!generated) throw std::runtime_error(generated.error.message);
            struct ExecutionTarget { double timeSeconds; Eigen::Isometry3d baseFromTool; bool safetyPoint; double speed; };
            struct { std::vector<ExecutionTarget> value; } execution;
            execution.value.push_back({0.0,initialBaseFromTcp,true,m_view.rapidSettings.safetySpeedMetersPerSecond});
            std::unordered_map<std::string,std::size_t> passPoints;
            for(const auto& step : generated.value.previewSteps) {
                double speed=m_view.rapidSettings.safetySpeedMetersPerSecond;
                if(step.sourceKind==domain::RapidSequenceEntryKind::Trajectory) {
                    const auto& pass=*std::find_if(exportPlan.group.passes.begin(),exportPlan.group.passes.end(),[&](const auto& p){return p.id==step.trajectoryPassId;});
                    auto& index=passPoints[step.trajectoryPassId];
                    speed=pass.trajectory.linearPoints[index++ % pass.trajectory.linearPoints.size()].speedMetersPerSecond;
                }
                const auto& previous=execution.value.back();
                const double dt=std::max(0.001,(step.baseFromTool.translation()-previous.baseFromTool.translation()).norm()/speed);
                execution.value.push_back({previous.timeSeconds+dt,step.baseFromTool,step.instruction=="MoveJ",speed});
            }

            motion_planning::ik::IkRequest ikRequest;
            ikRequest.robotModel = &robotModel;
            // Published rotation-body poses are already expressed in the selected
            // robot base coordinate system, so IK intentionally uses an identity base.
            ikRequest.baseTransform = Eigen::Isometry3d::Identity();
            ikRequest.tcpLinkName = tool.tcpLinkName;
            ikRequest.linkFromTcp = tool.linkFromTcp;
            ikRequest.jointNames = jointNames;
            ikRequest.seed = seed;
            ikRequest.options.maxIterations = 300;
            ikRequest.targets.reserve(execution.value.size());
            for(const ExecutionTarget& target : execution.value) {
                motion_planning::ik::IkTarget ikTarget;
                ikTarget.timeSeconds = target.timeSeconds;
                ikTarget.baseFromTcp = target.baseFromTool;
                ikTarget.safetyPoint =
                    target.safetyPoint;
                ikRequest.targets.push_back(std::move(ikTarget));
            }
            const motion_planning::ik::IkResult ikResult =
                motion_planning::ik::SprayIkPlanner::solve(ikRequest);
            if(!ikResult.success ||
                ikResult.trajectory.points.size() != execution.value.size()) {
                std::ostringstream message;
                message << "Inverse kinematics failed";
                if(!ikResult.diagnostics.empty()) {
                    const auto& diagnostic = ikResult.diagnostics.front();
                    message << " at point " << (diagnostic.targetIndex + 1)
                        << ": " << diagnostic.message
                        << " (position error "
                        << diagnostic.positionErrorMeters * 1000.0
                        << " mm, orientation error "
                        << diagnostic.orientationErrorRadians * 180.0 /
                            3.14159265358979323846
                        << " deg)";
                }
                throw std::runtime_error(message.str());
            }

            post_model::ProgramRequest request;
            request.robotName = robotDesc->name.empty() ? robotDesc->id : robotDesc->name;
            request.framePoseMillimeters = postPose(Eigen::Isometry3d::Identity(), 1000.0);
            request.toolPoseMillimeters = postPose(tool.linkFromTcp, 1000.0);
            request.commands.reserve(execution.value.size() - 1);
            constexpr double radiansToDegrees =
                180.0 / 3.14159265358979323846;
            for(std::size_t index = 1; index < execution.value.size(); ++index) {
                const ExecutionTarget& target = execution.value[index];
                const robottrajectory::TimedJointPoint& solved =
                    ikResult.trajectory.points[index];
                post_model::MotionCommand command;
                command.type = target.safetyPoint
                    ? post_model::MotionType::Joint
                    : post_model::MotionType::Linear;
                command.poseMillimeters = postPose(target.baseFromTool, 1000.0);
                command.jointsDegrees.reserve(solved.q.size());
                for(double jointRadians : solved.q) {
                    command.jointsDegrees.push_back(jointRadians * radiansToDegrees);
                }
                command.speedMillimetersPerSecond =
                    target.speed * 1000.0;
                request.commands.push_back(std::move(command));
            }
            if(request.commands.empty()) {
                throw std::runtime_error(
                    "The current execution sequence produced no exportable motion commands.");
            }

            m_postProcessingState->request = std::move(request);
            m_postProcessingState->robotId = robotDesc->id;
            m_postProcessingState->preview = intermediateProgramPreview(
                m_postProcessingState->request.robotName,
                tool.tcpLinkName,
                tool.mountedToolFound,
                m_postProcessingState->request);
            m_postProcessingState->trajectoryLoaded = true;
            m_postProcessingState->status =
                "The current freeform trajectory was loaded and all points passed IK.";
            const QString message = QStringLiteral(
                "Current trajectory loaded for Post Processing: %1 motion commands.")
                .arg(m_postProcessingState->request.commands.size());
            publishState(message, 5000);
            return { true, message };
        } catch(const std::exception& exception) {
            m_postProcessingState->status = exception.what();
            const QString message = QString::fromUtf8(exception.what());
            publishState(message, 6000);
            return { false, message };
        }
    }
    TranslationControllerResult
    CalibrationTranslationModuleController::generatePostProcessedProgram(
        const std::string& templateName,
        const std::string& robotId,
        const std::string& programName,
        const std::string& outputDirectory)
    {
        if(m_busy) {
            return {false,QStringLiteral("Translation is busy.")};
        }
        if(m_postProcessingState == nullptr ||
            !m_postProcessingState->trajectoryLoaded) {
            return {
                false,
                QStringLiteral("Load the current trajectory and solve IK before exporting.")
            };
        }
        if(robotId != m_postProcessingState->robotId) {
            return {
                false,
                QStringLiteral("The selected robot changed. Load the current trajectory again.")
            };
        }

        post_model::ProgramRequest request = m_postProcessingState->request;
        request.templateName = templateName;
        request.programName = programName;
        request.outputDirectory = outputDirectory;
        const post_model::ProgramResult generated =
            post_model::PostProgramRunner::generate(request);
        m_postProcessingState->exportSucceeded = generated.success;
        m_postProcessingState->outputFiles = generated.outputFiles;
        if(!generated.previewText.empty()) {
            m_postProcessingState->preview = generated.previewText;
        }
        if(!generated.success) {
            m_postProcessingState->status = generated.errorMessage;
            const QString message = QString::fromUtf8(generated.errorMessage.c_str());
            publishState(message, 6000);
            return { false, message };
        }

        m_postProcessingState->status =
            "The controller program was generated successfully.";
        const QString message = QStringLiteral(
            "Post-processing export completed: %1 file(s).")
            .arg(generated.outputFiles.size());
        publishState(message, 5000);
        return { true, message };
    }
}
