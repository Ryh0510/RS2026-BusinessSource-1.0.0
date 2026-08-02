#include "MotionPlanningModuleController.h"

#include "MotionPlanningEditorWidget.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportServices.h"

#include <MotionPlanningCore/MotionPlanning.h>
#include <ProjectMotionPlanning/TrajectoryImport.h>
#include <SimulationProject/ProjectDocumentService.h>

#include <QFileDialog>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    bool parseJointVector(
        const QString& text,
        std::vector<double>& values,
        QString& error)
    {
        values.clear();
        const QStringList parts = text.split(',', Qt::SkipEmptyParts);
        for(const QString& part : parts) {
            bool ok = false;
            const double value = part.trimmed().toDouble(&ok);
            if(!ok) {
                error = QStringLiteral("Invalid joint value: %1").arg(part.trimmed());
                return false;
            }
            values.push_back(value);
        }
        if(values.empty()) {
            error = QStringLiteral("Enter at least one joint value.");
            return false;
        }
        return true;
    }

    std::vector<std::string> selectedRobotJointNames(
        const simulation_project::ProjectDocument& document,
        const QString& robotId)
    {
        const auto robotIt = std::find_if(
            document.robots.begin(),
            document.robots.end(),
            [&](const simulation_project::RobotDesc& robot) {
                return robot.id == robotId.toStdString();
            });
        if(robotIt == document.robots.end()) {
            return {};
        }

        std::vector<std::string> names;
        names.reserve(robotIt->initialJoints.size());
        for(const simulation_project::JointValueDesc& joint : robotIt->initialJoints) {
            names.push_back(joint.jointName);
        }
        return names;
    }

    QString formatDouble(double value)
    {
        return QString::number(value, 'g', 8);
    }

    QString formatJointValues(
        const std::vector<std::string>& jointNames,
        const std::vector<double>& values)
    {
        QStringList parts;
        for(std::size_t index = 0; index < values.size(); ++index) {
            const QString name = index < jointNames.size()
                ? QString::fromStdString(jointNames[index])
                : QStringLiteral("q%1").arg(static_cast<qulonglong>(index + 1));
            parts.push_back(QStringLiteral("%1=%2").arg(name, formatDouble(values[index])));
        }
        return parts.join(QStringLiteral(", "));
    }

    QString formatCartesianPose(const robottrajectory::TimedCartesianPoint& point)
    {
        const Eigen::Vector3d translation = point.tcpPose.translation();
        return QStringLiteral("x=%1, y=%2, z=%3")
            .arg(formatDouble(translation.x()))
            .arg(formatDouble(translation.y()))
            .arg(formatDouble(translation.z()));
    }

    QString formatCartesianEuler(const robottrajectory::TimedCartesianPoint& point)
    {
        const Eigen::Vector3d euler = point.tcpPose.linear().eulerAngles(2, 1, 0);
        const double yawDeg = euler[0] * 180.0 / kPi;
        const double pitchDeg = euler[1] * 180.0 / kPi;
        const double rollDeg = euler[2] * 180.0 / kPi;
        return QStringLiteral("roll=%1, pitch=%2, yaw=%3")
            .arg(formatDouble(rollDeg))
            .arg(formatDouble(pitchDeg))
            .arg(formatDouble(yawDeg));
    }

    simulation_project::TransformDesc transformDescFromPose(const Eigen::Isometry3d& pose)
    {
        simulation_project::TransformDesc transform;
        transform.x = pose.translation().x();
        transform.y = pose.translation().y();
        transform.z = pose.translation().z();
        const Eigen::Vector3d euler = pose.linear().eulerAngles(2, 1, 0);
        transform.yaw = euler[0];
        transform.pitch = euler[1];
        transform.roll = euler[2];
        return transform;
    }

    std::vector<simulation_project::TransformDesc> cartesianControlPointTransforms(
        const motion_planning::StoredMotionPlan& plan)
    {
        std::vector<simulation_project::TransformDesc> transforms;
        transforms.reserve(plan.cartesianControlPoints.points.size());
        for(const robottrajectory::TimedCartesianPoint& point : plan.cartesianControlPoints.points) {
            transforms.push_back(transformDescFromPose(point.tcpPose));
        }
        return transforms;
    }

    std::filesystem::path toFilesystemPath(const QString& path)
    {
#ifdef _WIN32
        return std::filesystem::path(path.toStdWString());
#else
        return std::filesystem::path(path.toStdString());
#endif
    }
}

namespace robot_qt_viewer
{
    MotionPlanningModuleController::MotionPlanningModuleController(
        MotionPlanningEditorWidget& widget,
        RobotQtViewerDocumentContext& context,
        QObject* parent)
        : QObject(parent)
        , m_widget(widget)
        , m_context(context)
    {
        connect(&m_widget, &MotionPlanningEditorWidget::planRequested,
            this, &MotionPlanningModuleController::planTrajectory);
        connect(&m_widget, &MotionPlanningEditorWidget::importTrajectoryRequested,
            this, &MotionPlanningModuleController::importTrajectory);
        connect(&m_widget, &MotionPlanningEditorWidget::trajectorySelectionChanged,
            this, &MotionPlanningModuleController::setSelectedTrajectory);
        setSelectedRobot(m_context.selectionModel().state().robotId);
        refreshTrajectoryView();
    }

    void MotionPlanningModuleController::handleEvent(const RobotQtViewerEvent& event)
    {
        if(event.kind == RobotQtViewerEventKind::SelectionChanged) {
            setSelectedRobot(event.selection.robotId);
        } else if(event.kind == RobotQtViewerEventKind::ProjectOpened) {
            setSelectedRobot(m_context.selectionModel().state().robotId);
            refreshTrajectoryView();
        } else if(event.kind == RobotQtViewerEventKind::ProjectDocumentChanged) {
            refreshTrajectoryView();
        }
    }

    void MotionPlanningModuleController::planTrajectory(
        const QString& startJoints,
        const QString& goalJoints,
        const QString& jointNames,
        double duration,
        int sampleCount)
    {
        if(m_selectedRobotId.isEmpty()) {
            m_widget.setResult(QStringLiteral("Select a robot before planning."), false);
            return;
        }

        motion_planning::MotionPlanningRequest request;
        request.robotId = m_selectedRobotId.toStdString();
        request.constraint.duration = duration;
        request.constraint.sampleCount = static_cast<std::size_t>(sampleCount);
        const QStringList jointNameParts = jointNames.split(',', Qt::SkipEmptyParts);
        for(const QString& jointName : jointNameParts) {
            request.jointNames.push_back(jointName.trimmed().toStdString());
        }

        QString parseError;
        if(!parseJointVector(startJoints, request.startJoints, parseError) ||
            !parseJointVector(goalJoints, request.goalJoints, parseError)) {
            m_widget.setResult(parseError, false);
            return;
        }

        const motion_planning::LinearJointMotionPlanner planner;
        const motion_planning::MotionPlanningResult planningResult = planner.plan(request);
        if(!planningResult.succeeded()) {
            const QString message = planningResult.diagnostics.empty()
                ? QStringLiteral("Motion planning failed.")
                : QString::fromStdString(planningResult.diagnostics.front().message);
            m_widget.setResult(message, false);
            emit statusMessageRequested(message, 5000);
            return;
        }

        motion_planning::StoredMotionPlan plan;
        plan.id = request.robotId + "_linear_plan";
        plan.name = plan.id;
        plan.robotId = request.robotId;
        plan.jointNames = request.jointNames;
        plan.trajectory = planningResult.trajectory;

        const ProjectMutationResult mutation = m_context.documentController().mutateProject(
            QStringLiteral("motionPlanning"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                if(!motion_planning::MotionPlanningProjectStore::upsertPlan(
                       service.document(), plan, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutation.success) {
            m_widget.setResult(mutation.message, false);
            emit statusMessageRequested(mutation.message, 5000);
            return;
        }

        const QString planId = QString::fromStdString(plan.id);
        const QString summary = QStringLiteral("Stored %1 points as %2")
            .arg(static_cast<int>(plan.trajectory.points.size()))
            .arg(planId);
        m_selectedTrajectoryId = planId;
        refreshTrajectoryView();
        m_widget.setResult(summary, true);
        emit trajectoryPlanned(planId);
        emit statusMessageRequested(summary, 4000);
    }

    void MotionPlanningModuleController::importTrajectory()
    {
        if(m_selectedRobotId.isEmpty()) {
            m_widget.setResult(QStringLiteral("Select a robot before importing a trajectory."), false);
            return;
        }

        const QString filename = QFileDialog::getOpenFileName(
            &m_widget,
            QStringLiteral("Import trajectory"),
            QString(),
            QStringLiteral("Trajectory Files (*.csv *.txt *.json *.kf *.mod);;CSV Files (*.csv);;Text Files (*.txt);;JSON Files (*.json);;ABB Robot Programs (*.mod);;KeyFrame Files (*.kf);;All Files (*)"));
        if(filename.isEmpty()) {
            return;
        }

        motion_planning::TrajectoryImportOptions options;
        options.robotId = m_selectedRobotId.toStdString();
        options.jointNames = selectedRobotJointNames(m_context.document(), m_selectedRobotId);

        const motion_planning::TrajectoryImportResult importResult =
            motion_planning::ProjectTrajectoryImporter::importFile(
                toFilesystemPath(filename),
                options);
        if(!importResult.success) {
            const QString message = importResult.diagnostics.empty()
                ? QStringLiteral("Trajectory import failed.")
                : QString::fromStdString(importResult.diagnostics.front().message);
            m_widget.setResult(message, false);
            emit statusMessageRequested(message, 6000);
            return;
        }

        const motion_planning::StoredMotionPlan plan = importResult.plan;
        const ProjectMutationResult mutation = m_context.documentController().mutateProject(
            QStringLiteral("motionPlanningImport"),
            ProjectDirtyPolicy::UserEdit,
            [plan](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                if(!motion_planning::MotionPlanningProjectStore::upsertPlan(
                       service.document(), plan, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutation.success) {
            m_widget.setResult(mutation.message, false);
            emit statusMessageRequested(mutation.message, 6000);
            return;
        }

        const int pointCount = !plan.trajectory.empty()
            ? static_cast<int>(plan.trajectory.points.size())
            : static_cast<int>(plan.cartesianControlPoints.points.size());
        const QString kindText = !plan.trajectory.empty()
            ? QStringLiteral("joint trajectory")
            : QStringLiteral("cartesian control points");
        const QString planId = QString::fromStdString(plan.id);
        m_selectedTrajectoryId = planId;
        refreshTrajectoryView();

        QString summary = QStringLiteral("Imported %1 %2 as %3")
            .arg(pointCount)
            .arg(kindText)
            .arg(planId);
        if(!importResult.diagnostics.empty()) {
            summary += QStringLiteral(". %1")
                .arg(QString::fromStdString(importResult.diagnostics.front().message));
        }
        m_widget.setResult(summary, true);
        emit trajectoryPlanned(planId);
        emit statusMessageRequested(summary, 5000);
    }

    void MotionPlanningModuleController::setSelectedTrajectory(const QString& trajectoryId)
    {
        if(m_selectedTrajectoryId == trajectoryId) {
            return;
        }
        m_selectedTrajectoryId = trajectoryId;
        refreshTrajectoryView();
    }

    void MotionPlanningModuleController::setSelectedRobot(const QString& robotId)
    {
        m_selectedRobotId = robotId;
        m_widget.setRobotId(robotId);
        if(robotId.isEmpty()) {
            refreshTrajectoryView();
            return;
        }
        const simulation_project::ProjectDocument& document = m_context.document();
        const auto robotIt = std::find_if(
            document.robots.begin(),
            document.robots.end(),
            [&](const simulation_project::RobotDesc& robot) {
                return robot.id == robotId.toStdString();
            });
        if(robotIt == document.robots.end() || robotIt->initialJoints.empty()) {
            refreshTrajectoryView();
            return;
        }
        QStringList names;
        QStringList values;
        for(const simulation_project::JointValueDesc& joint : robotIt->initialJoints) {
            names.push_back(QString::fromStdString(joint.jointName));
            values.push_back(QString::number(joint.value, 'g', 12));
        }
        m_widget.setJointDefaults(names.join(QStringLiteral(", ")), values.join(QStringLiteral(", ")));
        refreshTrajectoryView();
    }

    void MotionPlanningModuleController::refreshTrajectoryView()
    {
        QVector<MotionPlanningEditorWidget::TrajectoryListItem> items;
        QVector<MotionPlanningEditorWidget::TrajectoryPointRow> rows;

        const std::vector<motion_planning::StoredMotionPlan> plans =
            motion_planning::MotionPlanningProjectStore::plans(m_context.document());

        const motion_planning::StoredMotionPlan* selectedPlan = nullptr;
        for(const motion_planning::StoredMotionPlan& plan : plans) {
            if(!m_selectedRobotId.isEmpty() && plan.robotId != m_selectedRobotId.toStdString()) {
                continue;
            }

            MotionPlanningEditorWidget::TrajectoryListItem item;
            item.id = QString::fromStdString(plan.id);
            item.label = QString::fromStdString(plan.name.empty() ? plan.id : plan.name);
            if(!plan.trajectory.empty()) {
                item.kind = QStringLiteral("joint");
                item.pointCount = static_cast<int>(plan.trajectory.points.size());
            } else if(!plan.cartesianControlPoints.empty()) {
                item.kind = QStringLiteral("cartesian");
                item.pointCount = static_cast<int>(plan.cartesianControlPoints.points.size());
            } else {
                continue;
            }
            items.push_back(item);

            if(item.id == m_selectedTrajectoryId) {
                selectedPlan = &plan;
            }
            if(selectedPlan == nullptr && m_selectedTrajectoryId.isEmpty()) {
                selectedPlan = &plan;
                m_selectedTrajectoryId = item.id;
            }
        }

        if(selectedPlan == nullptr && !items.empty()) {
            m_selectedTrajectoryId = items.front().id;
            const std::string selectedId = m_selectedTrajectoryId.toStdString();
            const auto selectedIt = std::find_if(
                plans.begin(),
                plans.end(),
                [&](const motion_planning::StoredMotionPlan& plan) {
                    return plan.id == selectedId;
                });
            if(selectedIt != plans.end()) {
                selectedPlan = &(*selectedIt);
            }
        }
        if(items.empty()) {
            m_selectedTrajectoryId.clear();
        }

        if(selectedPlan != nullptr && !selectedPlan->trajectory.empty()) {
            const std::vector<std::string>& jointNames = selectedPlan->jointNames;
            for(std::size_t index = 0; index < selectedPlan->trajectory.points.size(); ++index) {
                const robottrajectory::TimedJointPoint& point =
                    selectedPlan->trajectory.points[index];
                MotionPlanningEditorWidget::TrajectoryPointRow row;
                row.index = static_cast<int>(index + 1);
                row.timeText = formatDouble(point.time);
                row.valueText = formatJointValues(jointNames, point.q);
                row.orientationText.clear();
                rows.push_back(row);
            }
        } else if(selectedPlan != nullptr && !selectedPlan->cartesianControlPoints.empty()) {
            for(std::size_t index = 0; index < selectedPlan->cartesianControlPoints.points.size(); ++index) {
                const robottrajectory::TimedCartesianPoint& point =
                    selectedPlan->cartesianControlPoints.points[index];
                MotionPlanningEditorWidget::TrajectoryPointRow row;
                row.index = static_cast<int>(index + 1);
                row.timeText = formatDouble(point.time);
                row.valueText = formatCartesianPose(point);
                row.orientationText = formatCartesianEuler(point);
                rows.push_back(row);
            }
        }

        const QString emptyText = items.empty()
            ? QStringLiteral("No stored trajectory for the selected robot.")
            : QStringLiteral("No control points in the selected trajectory.");
        m_widget.setTrajectoryView(items, m_selectedTrajectoryId, rows, emptyText);

        if(RobotQtViewerViewportServices* viewportServices = m_context.viewportServices()) {
            if(selectedPlan != nullptr && !selectedPlan->cartesianControlPoints.empty()) {
                viewportServices->setTrajectoryControlPointOverlay(
                    QString::fromStdString(selectedPlan->id),
                    cartesianControlPointTransforms(*selectedPlan));
            } else {
                viewportServices->clearTrajectoryControlPointOverlay();
            }
        }
    }
}
