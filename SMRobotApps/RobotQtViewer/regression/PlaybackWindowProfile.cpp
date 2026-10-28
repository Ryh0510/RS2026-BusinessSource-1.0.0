#include "PlaybackWindowProfile.h"

#include "SceneExplorerModuleController.h"
#include "SceneExplorerTaskWidget.h"
#include "SceneExplorerWidget.h"
#include "ToolTransformEditorWidget.h"

#include <RobotViewport.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDockWidget>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QInputDialog>
#include <SimulationProject/ProjectIO.h>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <algorithm>
#include <iostream>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace
{
    class ImportFileResponder : public QObject
    {
    public:
        explicit ImportFileResponder(QString path) : m_path(std::move(path)) {}

        bool eventFilter(QObject* object, QEvent* event) override
        {
            if(!m_handled && event->type() == QEvent::Show) {
                if(auto* dialog = qobject_cast<QFileDialog*>(object)) {
                    m_handled = true;
                    QTimer::singleShot(0, dialog, [path = m_path, dialog]() {
                        dialog->selectFile(path);
                        static_cast<QDialog*>(dialog)->accept();
                    });
                }
            }
            return QObject::eventFilter(object, event);
        }

    private:
        QString m_path;
        bool m_handled = false;
    };

    struct ProfilePose { Eigen::Matrix4d pose = Eigen::Matrix4d::Identity(); double time = 0; };

    // Independent reader restricted to the two user fixtures and the exported matrix layout.
    std::vector<ProfilePose> readProfilePoses(const QString& path)
    {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) return {};
        std::vector<ProfilePose> points;
        ProfilePose point;
        int row = 0;
        for(const auto& line : QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
            const auto text = line.trimmed();
            if(text.isEmpty() || text.startsWith('#')) continue;
            const auto fields = text.split(QRegularExpression(QStringLiteral("\\s+")));
            QVector<double> values;
            for(const auto& field : fields) {
                bool ok = false;
                const double value = field.toDouble(&ok);
                if(!ok || !std::isfinite(value)) return {};
                values.push_back(value);
            }
            if(values.size() == 12 && row == 0) {
                ProfilePose pose;
                pose.pose.block<3,1>(0,3) = Eigen::Vector3d(values[0],values[1],values[2]) * .001;
                pose.pose.block<3,3>(0,0) = Eigen::Quaterniond(values[3],values[4],values[5],values[6]).normalized().toRotationMatrix();
                pose.time = values[9];
                points.push_back(pose);
            } else if(values.size() == 4 && row < 4) {
                for(int col = 0; col < 4; ++col) point.pose(row,col) = values[col];
                ++row;
            } else if(values.size() == 1 && row == 4) {
                point.pose.block<3,1>(0,3) *= .001;
                point.time = values[0];
                points.push_back(point);
                row = 0;
            } else return {};
        }
        return row == 0 ? points : std::vector<ProfilePose>{};
    }

    class TransformSettingsResponder : public QObject
    {
    public:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if(event->type() == QEvent::Show && object->objectName() == QStringLiteral("trajectoryTransformDialog")) {
                if(auto* dialog = qobject_cast<QDialog*>(object)) {
                    QTimer::singleShot(0, dialog, [dialog]() {
                        const auto args = qApp->arguments();
                        const int localOption = args.indexOf(QStringLiteral("--profile-local-rotation"));
                        if(localOption >= 0 && localOption + 2 < args.size()) {
                            auto* mode = dialog->findChild<QComboBox*>(QStringLiteral("trajectoryTransformMode"));
                            auto* axis = dialog->findChild<QComboBox*>(QStringLiteral("trajectoryLocalRotationAxis"));
                            auto* angle = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("trajectoryLocalRotationAngle"));
                            const int axisIndex = QStringLiteral("XYZ").indexOf(args[localOption+1].toUpper());
                            if(!mode || !axis || !angle || axisIndex < 0) { dialog->reject(); return; }
                            mode->setCurrentIndex(1);
                            axis->setCurrentIndex(axisIndex);
                            angle->setValue(args[localOption+2].toDouble());
                            const int reportOption = args.indexOf(QStringLiteral("--profile-playback-report"));
                            if(reportOption >= 0 && reportOption + 1 < args.size()) {
                                dialog->grab().save(args[reportOption+1] + QStringLiteral(".dialog.png"));
                            }
                            dialog->accept();
                            return;
                        }
                        const double values[] = {100, -200, 300, 90, 90, 0};
                        for(int i = 0; i < 6; ++i) {
                            if(auto* field = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("transformValue%1").arg(i))) field->setValue(values[i]);
                        }
                        dialog->accept();
                    });
                }
            }
            return QObject::eventFilter(object, event);
        }
    };

    class IkSettingsResponder : public QObject
    {
    public:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if(event->type() == QEvent::Show) {
                if(auto* dialog = qobject_cast<QDialog*>(object)) {
                    if(dialog->windowTitle() == QStringLiteral("\u5168\u9006\u89e3 - \u641c\u7d22\u8bbe\u7f6e")) {
                        QTimer::singleShot(0, dialog, &QDialog::accept);
                    }
                }
            }
            return QObject::eventFilter(object, event);
        }
    };

    struct ProfileState
    {
        QElapsedTimer clock;
        qint64 lastFrame = -1;
        qint64 lastHeartbeat = 0;
        std::vector<qint64> frameGaps;
        std::vector<qint64> heartbeatGaps;
        QPointer<QPushButton> play;
        QPointer<QWidget> editor;
        bool started = false;
        std::vector<double> firstJoints;
        double maxJointTravel = 0.0;
        bool profileDistance = false;
        int nearestFrames = 0;
        int overlayFrames = 0;
        int missingNearestFrames = 0;
        double minDistance = std::numeric_limits<double>::infinity();
        double maxDistance = 0.0;
        QJsonObject lastNearestStates;
        QJsonObject lastDetectorResults;
    };

    QPushButton* button(QWidget& widget, const QString& text)
    {
        for(auto* candidate : widget.findChildren<QPushButton*>()) {
            if(candidate->text() == text) {
                return candidate;
            }
        }
        return nullptr;
    }

    qint64 percentile(std::vector<qint64> values, int p)
    {
        if(values.empty()) {
            return 0;
        }
        std::sort(values.begin(), values.end());
        return values[(values.size() - 1) * p / 100];
    }

    void fail(const char* message)
    {
        std::cerr << "Main-window playback profile: " << message << std::endl;
        QTimer::singleShot(0, qApp, []() { qApp->exit(2); });
    }
}

// Opt-in regression in the real MainWindow. No project file is saved.
void startMoveBaseWindowProfile(QWidget& window, const QString& reportFile)
{
    window.showMaximized();
    auto* ready = new QTimer(&window);
    auto elapsed = std::make_shared<QElapsedTimer>();
    elapsed->start();
    QObject::connect(ready, &QTimer::timeout, &window, [&window, reportFile, ready, elapsed]() {
        QAction* assembly = nullptr;
        for(auto* action : window.findChildren<QAction*>()) {
            if(action->actionGroup() && action->text() == QStringLiteral("Project Assembly")) {
                assembly = action;
                break;
            }
        }
        auto* explorer = window.findChild<SceneExplorerWidget*>();
        auto* controller = window.findChild<robot_qt_viewer::SceneExplorerModuleController*>();
        auto* task = window.findChild<SceneExplorerTaskWidget*>();
        auto* viewport = window.findChild<RobotViewport*>();
        if(!assembly || !assembly->isEnabled() || !explorer || !controller || !task ||
            !viewport || explorer->treeWidget()->topLevelItemCount() == 0) {
            if(elapsed->elapsed() > 60000) {
                ready->stop();
                fail("Move Base: original workbench did not become ready");
            }
            return;
        }
        ready->stop();
        using namespace robot_qt_viewer;
        QJsonArray checks;
        bool passed = true;
        const auto check = [&](const char* name, bool ok) {
            checks.append(QJsonObject{{"check", QString::fromLatin1(name)}, {"passed", ok}});
            passed = passed && ok;
        };
        const auto settle = []() {
            QEventLoop loop;
            QTimer::singleShot(200, &loop, &QEventLoop::quit);
            loop.exec();
        };
        auto* tree = explorer->treeWidget();
        tree->setCurrentItem(nullptr);
        controller->refreshViewModel();
        assembly->trigger();
        settle();
        check("unselected assembly task is hidden", !task->isVisible());
        auto* editor = task->findChild<ToolTransformEditorWidget*>();
        auto* apply = task->findChild<QPushButton*>(QStringLiteral("sceneExplorerApplyTransformButton"));
        auto* cancel = task->findChild<QPushButton*>(QStringLiteral("sceneExplorerCancelTransformButton"));
        QDockWidget* dock = nullptr;
        for(QWidget* parent = task->parentWidget(); parent; parent = parent->parentWidget()) {
            if((dock = qobject_cast<QDockWidget*>(parent))) break;
        }
        const auto findItem = [&](const QString& kind, const QString& id = QString()) {
            for(QTreeWidgetItemIterator it(tree); *it; ++it) {
                if((*it)->data(0, kSceneExplorerRoleType).toString() == kind &&
                    (id.isEmpty() || (*it)->data(0, kSceneExplorerRoleId).toString() == id)) return *it;
            }
            return static_cast<QTreeWidgetItem*>(nullptr);
        };
        const auto openMoveMenu = [&](QTreeWidgetItem* item, const QString& label) {
            if(!item) return false;
            tree->scrollToItem(item);
            auto triggered = std::make_shared<bool>(false);
            // Enter the actual context-menu handler without a preceding left click.
            QTimer::singleShot(0, tree, [triggered, label]() {
                if(auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                    for(auto* action : menu->actions()) {
                        if(action->text() == label && action->isEnabled()) {
                            action->trigger();
                            *triggered = true;
                            break;
                        }
                    }
                    menu->close();
                }
            });
            tree->customContextMenuRequested(tree->visualItemRect(item).center());
            settle();
            return *triggered;
        };
        const auto panelDisplayed = [&]() {
            auto* stack = qobject_cast<QStackedWidget*>(task->parentWidget());
            return stack && stack->indexOf(task) >= 0 && stack->currentWidget() == task &&
                task->isVisible() && task->width() > 200 && task->height() > 200 &&
                editor && editor->isVisible() && apply && apply->isVisible();
        };
        check("original controls available", editor && apply && cancel && dock);
        check("direct right-click Move Base action", openMoveMenu(findItem(kSceneExplorerNodeRobot), QStringLiteral("Move Base...")));
        check("direct right-click displays transform panel", panelDisplayed());
        if(dock) dock->hide();
        check("hidden panel Move Base action", openMoveMenu(findItem(kSceneExplorerNodeRobot), QStringLiteral("Move Base...")));
        check("Move Base restores hidden panel", dock && dock->isVisible() && panelDisplayed());
        if(passed) {
            const QString robotId = controller->currentNode().id;
            std::vector<std::string> joints;
            for(QTreeWidgetItemIterator it(tree); *it; ++it) {
                if((*it)->data(0, kSceneExplorerRoleType).toString() == kSceneExplorerNodeJoint &&
                    (*it)->data(0, kSceneExplorerRoleId).toString() == robotId) {
                    const auto name = (*it)->data(0, kSceneExplorerRoleName).toString().toStdString();
                    if(viewport->robotForwardKinematics(robotId, {name}, false)) {
                        joints = {name};
                        break;
                    }
                }
            }
            const auto position = [&]() -> Eigen::Vector3d {
                const auto fk = viewport->robotForwardKinematics(robotId, joints, false);
                return fk ? Eigen::Vector3d(fk({0.0}).translation()) : Eigen::Vector3d::Constant(1e20);
            };
            check("runtime FK available", !joints.empty());
            const auto original = editor->transform();
            const Eigen::Vector3d originalPosition = position();
            const auto fields = editor->findChildren<QDoubleSpinBox*>();
            check("six transform fields", fields.size() == 6);
            if(fields.size() == 6) {
                fields.front()->setValue(original.x + 0.1);
                settle();
                check("preview moves runtime by 100mm", (position() - originalPosition - Eigen::Vector3d(0.1, 0, 0)).norm() < 1e-8);
                cancel->click();
                settle();
                check("cancel restores document and runtime", std::abs(editor->transform().x - original.x) < 1e-8 && (position() - originalPosition).norm() < 1e-8);
                fields.front()->setValue(original.x + 0.1);
                apply->click();
                settle();
                fields.front()->setValue(original.x + 0.2);
                cancel->click();
                settle();
                check("apply commits document and runtime", std::abs(editor->transform().x - original.x - 0.1) < 1e-8 && (position() - originalPosition - Eigen::Vector3d(0.1, 0, 0)).norm() < 1e-8);
                fields.front()->setValue(original.x);
                apply->click();
                settle();
                check("original base restored", std::abs(editor->transform().x - original.x) < 1e-8 && (position() - originalPosition).norm() < 1e-8);
            }
            QAction* planning = nullptr;
            for(auto* action : window.findChildren<QAction*>()) {
                if(action->actionGroup() && action->text() == QStringLiteral("Motion Planning")) {
                    planning = action;
                    break;
                }
            }
            check("Motion Planning action available", planning && planning->isEnabled());
            if(planning && planning->isEnabled()) {
                planning->trigger();
                settle();
                check("assembly editor hidden in other workbench", planning->isChecked() && !task->isVisible());
                assembly->trigger();
                settle();
                check("assembly editor restored after switching back", assembly->isChecked() && panelDisplayed());
            }
            if(auto* fixture = findItem(kSceneExplorerNodeRobot, QStringLiteral("burnner"))) {
                check("Move Base can target fixture", openMoveMenu(fixture, QStringLiteral("Move Base...")) &&
                    controller->currentNode().id == QStringLiteral("burnner") && panelDisplayed());
                check("Move Base can return to robot", openMoveMenu(findItem(kSceneExplorerNodeRobot, robotId), QStringLiteral("Move Base...")) &&
                    controller->currentNode().id == robotId && panelDisplayed());
            }
        }
        const QJsonObject report{{"passed", passed}, {"checks", checks}};
        QFile output(reportFile);
        if(!output.open(QIODevice::WriteOnly) || output.write(QJsonDocument(report).toJson()) < 0) passed = false;
        window.grab().save(reportFile + QStringLiteral(".png"));
        std::cout << "Main-window Move Base: " << QJsonDocument(report).toJson(QJsonDocument::Compact).constData() << std::endl;
        QTimer::singleShot(0, qApp, [passed]() { qApp->exit(passed ? 0 : 2); });
    });
    ready->start(250);
}

// Accepts a generic trajectory JSON. The driver uses the
// original MainWindow, import button, editor, controller and rendered viewport.
void startPlaybackWindowProfile(QWidget& window, const QString& trajectoryFile,
    const QString& reportFile)
{
    QFile trajectory(trajectoryFile);
    if(reportFile.isEmpty() || !trajectory.open(QIODevice::ReadOnly)) {
        fail("readable trajectory and report paths are required");
        return;
    }
    const bool basicIk=qApp->arguments().contains(QStringLiteral("--profile-basic-ik")) ||
        qApp->arguments().contains(QStringLiteral("--profile-ga-apf"));
    const int matrixPoints=basicIk ? int(readProfilePoses(trajectoryFile).size()) : 0;
    const int sourcePoints=matrixPoints>0 ? matrixPoints : QJsonDocument::fromJson(trajectory.readAll()).object()
        .value(QStringLiteral("trajectory")).toObject()
        .value(QStringLiteral("points")).toArray().size();
    if(sourcePoints == 0 && !qApp->arguments().contains(QStringLiteral("--profile-cartesian-import")) &&
        !qApp->arguments().contains(QStringLiteral("--profile-cdf-top1")) &&
        !qApp->arguments().contains(QStringLiteral("--profile-transform-program-export")) &&
        !qApp->arguments().contains(QStringLiteral("--profile-trajectory-transform")) &&
        !qApp->arguments().contains(QStringLiteral("--profile-trajectory-orientations"))) {
        fail("trajectory JSON has no source points");
        return;
    }
    window.showMaximized();
    window.raise();
    window.activateWindow();
    const auto state = std::make_shared<ProfileState>();
    auto* viewport = window.findChild<RobotViewport*>();
    if(!viewport) {
        fail("original viewport is unavailable");
        return;
    }
    QObject::connect(viewport, &QOpenGLWidget::frameSwapped, &window, [state, viewport]() {
        if(!state->started) {
            return;
        }
        const auto now = state->clock.elapsed();
        for(std::size_t i=0;i<state->firstJoints.size();++i)
            state->maxJointTravel=std::max(state->maxJointTravel,std::abs(viewport->robotJointValue(
                QStringLiteral("m710ic_50_drake"),QStringLiteral("J%1").arg(i+1))-state->firstJoints[i]));
        if(state->lastFrame >= 0) {
            state->frameGaps.push_back(now - state->lastFrame);
        }
        state->lastFrame = now;
        if(state->profileDistance) {
            bool nearest = false, overlay = false;
            for(const auto& detector : viewport->collisionDetectors()) {
                state->lastNearestStates[QString::fromStdString(detector.id)] =
                    QString::fromStdString(detector.nearest.state + ": " + detector.nearest.reason);
                state->lastDetectorResults[QString::fromStdString(detector.id)] = QJsonObject{
                    {"inCollision", detector.inCollision}, {"contactCount", int(detector.contactCount)},
                    {"firstPairA", QString::fromStdString(detector.firstPairA)},
                    {"firstPairB", QString::fromStdString(detector.firstPairB)}};
                if(detector.id != "cdf_abb4600_burnner_detector" ||
                    !detector.enabled || !detector.visible || !detector.valid) { continue; }
                overlay = overlay || detector.frameOverlayNearestCount > 0;
                if(detector.nearest.valid && std::isfinite(detector.nearest.distance)) {
                    nearest = true;
                    state->minDistance = std::min(state->minDistance, detector.nearest.distance);
                    state->maxDistance = std::max(state->maxDistance, detector.nearest.distance);
                }
            }
            state->nearestFrames += nearest ? 1 : 0;
            state->overlayFrames += overlay ? 1 : 0;
            state->missingNearestFrames += nearest ? 0 : 1;
        }
    });
    auto* setupTimeout = new QTimer(&window);
    setupTimeout->setSingleShot(true);
    QObject::connect(setupTimeout, &QTimer::timeout, &window, []() {
        fail("import/playback setup timed out");
    });
    setupTimeout->start(60000);
    auto* heartbeat = new QTimer(&window);
    heartbeat->setInterval(10);
    QObject::connect(heartbeat, &QTimer::timeout, &window,
        [&window, state, heartbeat, reportFile, sourcePoints, trajectoryFile, viewport]() {
        if(!state->started) {
            return;
        }
        const auto now = state->clock.elapsed();
        state->heartbeatGaps.push_back(now - state->lastHeartbeat);
        state->lastHeartbeat = now;
        const bool finished = state->play && state->play->text() == QStringLiteral("Play IK result");
        if(state->editor && !finished && now < 120000) {
            return;
        }
        heartbeat->stop();
        state->started = false;
        QString summary;
        if(state->editor) {
            for(auto* label : state->editor->findChildren<QLabel*>()) {
                if(label->text().contains(QStringLiteral("Collision states:"))) {
                    summary = label->text();
                }
            }
        }
        const auto counts = QRegularExpression(QStringLiteral(
            "Collision states: ([0-9]+) / ([0-9]+).*Invalid states: ([0-9]+)")).match(summary);
        const bool allPointsChecked = counts.hasMatch() && counts.captured(2).toInt() == sourcePoints;
        QJsonObject result{
            {"finished", finished}, {"elapsedMs", double(now)},
            {"sourcePoints", sourcePoints}, {"allPointsChecked", allPointsChecked},
            {"requestedPreviewSeconds", 5.0},
            {"frames", state->lastFrame >= 0 ? int(state->frameGaps.size() + 1) : 0},
            {"frameP95Ms", double(percentile(state->frameGaps, 95))},
            {"frameMaxMs", double(percentile(state->frameGaps, 100))},
            {"heartbeatP95Ms", double(percentile(state->heartbeatGaps, 95))},
            {"heartbeatMaxMs", double(percentile(state->heartbeatGaps, 100))},
            {"summary", summary}, {"maxJointTravelRadians",state->maxJointTravel}};
        const bool distancePassed = !state->profileDistance ||
            (state->nearestFrames > 10 && state->overlayFrames == state->nearestFrames &&
             state->missingNearestFrames == 0 && state->maxDistance - state->minDistance > 1e-6);
        if(state->profileDistance) {
            result["distancePassed"] = distancePassed;
            result["nearestFrames"] = state->nearestFrames;
            result["overlayFrames"] = state->overlayFrames;
            result["missingNearestFrames"] = state->missingNearestFrames;
            result["minDistanceMeters"] = std::isfinite(state->minDistance) ? state->minDistance : -1.0;
            result["maxDistanceMeters"] = state->maxDistance;
            result["lastNearestStates"] = state->lastNearestStates;
            result["lastDetectorResults"] = state->lastDetectorResults;
        }
        bool plotPassed = true;
        if(finished && qApp->arguments().contains(QStringLiteral("--profile-nearest-distance-plot"))) {
            auto* plotButton = state->editor->findChild<QPushButton*>(QStringLiteral("plotNearestDistances"));
            plotPassed = plotButton && plotButton->isEnabled();
            if(plotPassed) {
                plotButton->click();
                auto* dialog = state->editor->findChild<QDialog*>(QStringLiteral("nearestDistancePlot"));
                auto* exportButton = dialog ? dialog->findChild<QPushButton*>(QStringLiteral("exportNearestDistances")) : nullptr;
                plotPassed = dialog && exportButton;
                if(plotPassed) {
                    ImportFileResponder responder(reportFile + QStringLiteral(".distances.txt"));
                    qApp->installEventFilter(&responder);
                    exportButton->click();
                    qApp->removeEventFilter(&responder);
                    QFile data(reportFile + QStringLiteral(".distances.txt"));
                    plotPassed = data.open(QIODevice::ReadOnly);
                    const auto rows = QString::fromUtf8(data.readAll()).split('\n', Qt::SkipEmptyParts);
                    int count = 0, invalid = 0;
                    for(const auto& row : rows) {
                        if(row.startsWith('#') || row.startsWith(QStringLiteral("index"))) continue;
                        const auto columns = row.split('\t');
                        if(columns.size() < 7 || columns[0].toInt() != count + 1) { plotPassed = false; break; }
                        if(columns[3] != QStringLiteral("1")) ++invalid;
                        ++count;
                    }
                    plotPassed = plotPassed && count == sourcePoints && invalid == 0;
                    result["distanceRows"] = count;
                    result["invalidDistanceRows"] = invalid;
                    plotPassed = dialog->grab().save(reportFile + QStringLiteral(".distances.png")) && plotPassed;
                }
            }
            result["distancePlotPassed"] = plotPassed;
        }
        bool workpiecePassed = true;
        if(qApp->arguments().contains(QStringLiteral("--profile-workpiece-measurement"))) {
            QFile fixture(trajectoryFile);
            workpiecePassed = fixture.open(QIODevice::ReadOnly);
            const auto source = QJsonDocument::fromJson(fixture.readAll()).object();
            const auto robotId = source.value(QStringLiteral("robotId")).toString();
            const auto measurement = viewport->sprayMeasurement(robotId);
            result["sprayValid"] = measurement.valid;
            result["sprayDistanceMeters"] = measurement.distanceMeters;
            result["sprayAngleDegrees"] = measurement.angleDegrees;
            result["sprayError"] = QString::fromStdString(measurement.errorMessage);
            std::vector<std::string> names;
            for(const auto& name : source.value(QStringLiteral("jointNames")).toArray()) names.push_back(name.toString().toStdString());
            const auto rows = source.value(QStringLiteral("trajectory")).toObject().value(QStringLiteral("points")).toArray();
            std::vector<double> q;
            if(!rows.isEmpty()) for(const auto& value : rows.last().toObject().value(QStringLiteral("q")).toArray()) q.push_back(value.toDouble());
            const auto fk = viewport->robotForwardKinematics(robotId, names, true);
            workpiecePassed = workpiecePassed && measurement.valid && fk && q.size()==6;
            if(workpiecePassed) {
                for(const int axis : {0,3,4,5}) q[axis] = -q[axis];
                const auto pose = fk(q);QJsonArray matrix;
                for(int r=0;r<4;++r) { QJsonArray row;for(int c=0;c<4;++c) row.append(pose.matrix()(r,c));matrix.append(row); }
                result["tcpWorld"] = matrix;
                auto* plot = state->editor->findChild<QPushButton*>(QStringLiteral("plotSprayMeasurements"));
                workpiecePassed = plot && plot->isEnabled();
                if(workpiecePassed) {
                    plot->click();bool captured=false;
                    for(auto* dialog : state->editor->findChildren<QDialog*>()) {
                        if(dialog->windowTitle().startsWith(QStringLiteral("Spray measurements on A.STL"))) {
                            captured = dialog->grab().save(reportFile + QStringLiteral(".spray.png"));dialog->close();break;
                        }
                    }
                    workpiecePassed = captured;
                }
            }
            result["workpiecePassed"] = workpiecePassed;
        }
        const auto json = QJsonDocument(result).toJson();
        QFile report(reportFile);
        const bool reportSaved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
        const bool screenshotSaved = window.grab().save(reportFile + QStringLiteral(".png"));
        std::cout << "Main-window playback profile: "
            << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
        const bool moved=state->firstJoints.empty() || state->maxJointTravel>1e-4;
        qApp->exit(finished && moved && allPointsChecked && distancePassed && plotPassed && workpiecePassed && reportSaved && screenshotSaved ? 0 : 2);
    });
    // Let the original project's deferred startup activation finish before the
    // export diagnostic opens nested file dialogs and captures the CDF page.
    const int setupDelay = (qApp->arguments().contains(QStringLiteral("--profile-fanuc-export")) ||
        qApp->arguments().contains(QStringLiteral("--profile-trajectory-orientations")) ||
        qApp->arguments().contains(QStringLiteral("--profile-transform-program-export")) ||
        qApp->arguments().contains(QStringLiteral("--profile-trajectory-transform"))) ? 3000 : 500;
    QTimer::singleShot(setupDelay, &window, [&window, state, heartbeat, setupTimeout, trajectoryFile, reportFile, sourcePoints, viewport]() {
        for(auto* action : window.findChildren<QAction*>()) {
            // A dock toggle can share this label; use the actual workbench action.
            if(action->actionGroup() && (action->text() == QStringLiteral("Motion Planning") ||
                action->text() == QStringLiteral("action.trajectoryPlanningWorkbench") ||
                action->text() == QStringLiteral("\u8fd0\u52a8\u89c4\u5212"))) {
                if(action->isEnabled()) {
                    action->trigger();
                }
                break;
            }
        }
        for(auto* widget : window.findChildren<QWidget*>()) {
            if(QByteArray(widget->metaObject()->className()) == "MotionPlanningEditorWidget") {
                state->editor = widget;
            }
        }
        if(!state->editor) {
            fail("original Motion Planning editor is unavailable; run with --language en-US");
            return;
        }
        const bool transformProfile = qApp->arguments().contains(QStringLiteral("--profile-trajectory-transform")) ||
            qApp->arguments().contains(QStringLiteral("--profile-transform-program-export"));
        if(transformProfile) {
            for(auto* tabs : state->editor->findChildren<QTabWidget*>()) {
                if(tabs->count() == 3 && tabs->tabText(2) == QStringLiteral("Trajectory Transform")) tabs->setCurrentIndex(2);
            }
        }
        auto* import = transformProfile ? state->editor->findChild<QPushButton*>(QStringLiteral("importTransformTrajectory")) :
            button(*state->editor, QStringLiteral("Import trajectory..."));
        if(!import || !import->isEnabled()) {
            fail("original import control is unavailable");
            return;
        }
        ImportFileResponder responder(trajectoryFile);
        qApp->installEventFilter(&responder);
        import->click();
        qApp->removeEventFilter(&responder);
        const auto arguments = qApp->arguments();
        if(arguments.contains(QStringLiteral("--profile-ga-apf"))) {
            setupTimeout->stop();
            auto* useCurrent=state->editor->findChild<QPushButton*>(QStringLiteral("useCurrentTrajectoryForCdf"));
            if(useCurrent && useCurrent->isEnabled())useCurrent->click();
            auto* run=state->editor->findChild<QPushButton*>(QStringLiteral("runGaApfCdf"));
            auto* optional=state->editor->findChild<QCheckBox*>(QStringLiteral("cdfOptionalOptimization"));
            auto* table=state->editor->findChild<QTableWidget*>(QStringLiteral("trajectoryCartesianControlPoints"));
            const bool qpDefaultOff=optional && !optional->isChecked();
            if(!qpDefaultOff){fail("CDF/QP must be disabled by default");return;}
            if(optional)optional->setChecked(arguments.contains(QStringLiteral("--profile-with-cdf")));
            auto* sprayDistance=state->editor->findChild<QDoubleSpinBox*>(QStringLiteral("gaSprayDistance"));
            const int sprayOption=arguments.indexOf(QStringLiteral("--profile-spray-distance-mm"));
            if(sprayDistance && sprayOption>=0 && sprayOption+1<arguments.size())
                sprayDistance->setValue(arguments[sprayOption+1].toDouble());
            const bool enabled=run && run->isEnabled();
            QElapsedTimer planningClock;planningClock.start();
            if(enabled)run->click();
            const double planningSeconds=planningClock.elapsed()/1000.;
            auto* stages=state->editor->findChild<QComboBox*>(QStringLiteral("cdfStageSelection"));
            QJsonArray names;if(stages)for(int i=0;i<stages->count();++i)names.append(stages->itemText(i));
            auto* distance=state->editor->findChild<QPushButton*>(QStringLiteral("plotNearestDistances"));
            bool distanceValid=false;
            if(distance && distance->isEnabled()) {
                distance->click();
                auto* plot=state->editor->findChild<QDialog*>(QStringLiteral("nearestDistancePlot"));
                auto* exporter=plot?plot->findChild<QPushButton*>(QStringLiteral("exportNearestDistances")):nullptr;
                if(exporter) {
                    ImportFileResponder save(reportFile+QStringLiteral(".distances.txt"));
                    qApp->installEventFilter(&save);exporter->click();qApp->removeEventFilter(&save);
                    QFile data(reportFile+QStringLiteral(".distances.txt"));
                    if(data.open(QIODevice::ReadOnly)) {
                        int rows=0,invalid=0;
                        for(const auto& row:QString::fromUtf8(data.readAll()).split('\n',Qt::SkipEmptyParts)) {
                            if(row.startsWith('#') || row.startsWith(QStringLiteral("index")))continue;
                            const auto values=row.split('\t');++rows;
                            if(values.size()<7 || values[3]!=QStringLiteral("1"))++invalid;
                        }
                        distanceValid=rows>0 && invalid==0;
                    }
                    plot->grab().save(reportFile+QStringLiteral(".distances.png"));
                }
                if(plot)plot->close();
            }
            bool exportEnabled=false;
            for(auto* candidate:state->editor->findChildren<QPushButton*>())
                if(candidate->text()==QStringLiteral("Export APF + CDF/QP trajectory..."))exportEnabled=candidate->isEnabled();
            const bool passed=enabled && stages && stages->count()>=2 && exportEnabled && distanceValid;
            QJsonObject result{{"success",passed},{"sourcePoints",sourcePoints},{"runButtonEnabled",enabled},
                {"sprayDistanceMm",sprayDistance?sprayDistance->value():-1},
                {"qpDefaultOff",qpDefaultOff},{"planningSeconds",planningSeconds},
                {"stages",names},{"finalExportEnabled",exportEnabled},{"distanceWithoutPlayback",distanceValid}};
            QFile output(reportFile);output.open(QIODevice::WriteOnly);output.write(QJsonDocument(result).toJson());
            window.grab().save(reportFile+QStringLiteral(".png"));
            std::cout<<QJsonDocument(result).toJson(QJsonDocument::Compact).constData()<<std::endl;
            qApp->exit(passed?0:2);return;
        }
        if(arguments.contains(QStringLiteral("--profile-transform-program-export"))) {
            setupTimeout->stop();
            const bool transformed = arguments.contains(QStringLiteral("--profile-local-rotation")) ||
                arguments.contains(QStringLiteral("--profile-apply-transform"));
            if(transformed) {
                auto* apply = state->editor->findChild<QPushButton*>(QStringLiteral("transformTrajectory"));
                if(!apply || !apply->isEnabled()) { fail("transform before program export unavailable"); return; }
                TransformSettingsResponder transformResponder;
                qApp->installEventFilter(&transformResponder); apply->click(); qApp->removeEventFilter(&transformResponder);
            }
            const auto exportMatrix = [&](const QString& path) {
                auto* button = state->editor->findChild<QPushButton*>(QStringLiteral("exportTransformTrajectory"));
                if(!button || !button->isEnabled()) return false;
                ImportFileResponder responder(path);
                qApp->installEventFilter(&responder); button->click(); qApp->removeEventFilter(&responder);
                return QFileInfo::exists(path);
            };
            const QString before = reportFile + QStringLiteral(".trajectory.txt");
            const QString after = reportFile + QStringLiteral(".after.txt");
            bool valid = exportMatrix(before);
            QJsonArray exports;
            const auto source = readProfilePoses(trajectoryFile);
            for(bool fanuc : {false, true}) {
                auto* control = state->editor->findChild<QPushButton*>(fanuc ? QStringLiteral("exportTransformFanuc") : QStringLiteral("exportTransformRapid"));
                if(!control || !control->isEnabled()) { fail("direct Cartesian robot program export unavailable"); return; }
                for(QWidget* parent = control->parentWidget(); parent; parent = parent->parentWidget())
                    if(auto* scroll = qobject_cast<QScrollArea*>(parent)) scroll->ensureWidgetVisible(control);
                const QString path = QFileInfo(reportFile).absolutePath() +
                    (fanuc ? QStringLiteral("/XPORT.LS") : QStringLiteral("/MainModule.mod"));
                ImportFileResponder saveResponder(path);
                qApp->installEventFilter(&saveResponder); control->click(); qApp->removeEventFilter(&saveResponder);
                QFile file(path);
                const bool saved = file.open(QIODevice::ReadOnly);
                const auto content = saved ? file.readAll() : QByteArray();
                const bool format = fanuc ? (content.startsWith("/PROG  XPORT\r\n") &&
                    content.count("400mm/sec CNT100") == int(source.size())) :
                    (content.startsWith("MODULE MainModule\r\n") &&
                    content.count(", v50, z10, tool0\\WObj:=wobj0;") == int(source.size()));
                valid = valid && saved && format;
                exports.append(QJsonObject{{"fanuc",fanuc},{"path",path},{"saved",saved},{"formatPassed",format}});
            }
            valid = exportMatrix(after) && valid;
            QFile a(before), b(after);
            valid = valid && a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll();
            const QJsonObject result{{"success",valid},{"sourcePoints",int(source.size())},
                {"transformed",transformed},{"exports",exports},{"exportDidNotChangeCartesianPlan",valid}};
            QFile report(reportFile);
            const auto json = QJsonDocument(result).toJson();
            const bool saved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
            QTimer::singleShot(300, &window, [&window, reportFile, result, valid, saved]() {
                const bool screenshot = window.grab().save(reportFile + QStringLiteral(".png"));
                std::cout << "Main-window Cartesian program export: " << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                qApp->exit(valid && saved && screenshot ? 0 : 2);
            });
            return;
        }
        if(transformProfile) {
            setupTimeout->stop();
            auto* apply = state->editor->findChild<QPushButton*>(QStringLiteral("transformTrajectory"));
            auto* exportButton = state->editor->findChild<QPushButton*>(QStringLiteral("exportTransformTrajectory"));
            auto* remove = state->editor->findChild<QPushButton*>(QStringLiteral("deleteTransformTrajectory"));
            auto* points = state->editor->findChild<QCheckBox*>(QStringLiteral("trajectoryPointsVisible"));
            if(!apply || !exportButton || !remove || !points || !apply->isEnabled() || !points->isChecked()) {
                fail("transform controls/automatic point display not enabled"); return;
            }
            TransformSettingsResponder transformResponder;
            qApp->installEventFilter(&transformResponder);
            apply->click();
            qApp->removeEventFilter(&transformResponder);
            if(arguments.contains(QStringLiteral("--profile-local-rotation"))) {
                if(auto* axes = state->editor->findChild<QCheckBox*>(QStringLiteral("trajectoryOrientationsVisible"))) axes->setChecked(true);
            }
            const QString outputPath = reportFile + QStringLiteral(".trajectory.txt");
            ImportFileResponder saveResponder(outputPath);
            qApp->installEventFilter(&saveResponder);
            exportButton->click();
            qApp->removeEventFilter(&saveResponder);
            const auto original = readProfilePoses(trajectoryFile);
            const auto output = readProfilePoses(outputPath);
            bool valid = !original.empty() && original.size() == output.size();
            double positionError = 0, rotationError = 0, timeError = 0;
            Eigen::Matrix3d rotation;
            rotation << 0, 1, 0, 0, 0, -1, -1, 0, 0;
            const int localOption = arguments.indexOf(QStringLiteral("--profile-local-rotation"));
            const bool local = localOption >= 0 && localOption + 2 < arguments.size();
            const int localAxis = local ? QStringLiteral("XYZ").indexOf(arguments[localOption+1].toUpper()) : 2;
            const double angle = local ? arguments[localOption+2].toDouble() : 0;
            valid = valid && localAxis >= 0;
            for(std::size_t i = 0; valid && i < original.size(); ++i) {
                const auto& a = original[i];
                const auto& b = output[i];
                const Eigen::Vector3d expectedPosition = local ? a.pose.block<3,1>(0,3).eval()
                    : (rotation * a.pose.block<3,1>(0,3) + Eigen::Vector3d(.1,-.2,.3)).eval();
                const Eigen::Matrix3d expectedRotation = local
                    ? (Eigen::AngleAxisd(angle * 3.14159265358979323846 / 180.0,
                        a.pose.block<3,3>(0,0).col(localAxis)).toRotationMatrix() * a.pose.block<3,3>(0,0)).eval()
                    : (rotation * a.pose.block<3,3>(0,0)).eval();
                positionError = std::max(positionError, (b.pose.block<3,1>(0,3) - expectedPosition).norm());
                rotationError = std::max(rotationError, (b.pose.block<3,3>(0,0) - expectedRotation).norm());
                timeError = std::max(timeError, std::abs(a.time - b.time));
            }
            valid = valid && positionError < 1e-9 && rotationError < 1e-9 && timeError < 1e-9;
            QTimer::singleShot(300, &window, [&window, state, reportFile, valid, positionError, rotationError, timeError, count = original.size(), local, localAxis, angle]() {
                const bool screenshot = window.grab().save(reportFile + QStringLiteral(".png"));
                auto* removeButton = state->editor->findChild<QPushButton*>(QStringLiteral("deleteTransformTrajectory"));
                auto* combo = state->editor->findChild<QComboBox*>(QStringLiteral("transformTrajectorySelection"));
                const QString removedId = combo ? combo->currentData().toString() : QString();
                const int previousCount = combo ? combo->count() : 0;
                removeButton->click();
                // Other Cartesian plans may already exist in the loaded project.
                const bool deleted = combo && !removedId.isEmpty() && combo->findData(removedId) < 0 &&
                    combo->count() == previousCount - 1;
                const QJsonObject result{{"success", valid && deleted}, {"sourcePoints", int(count)}, {"deletePassed", deleted},
                    {"maxPositionErrorMeters", positionError}, {"maxRotationMatrixError", rotationError}, {"maxTimeErrorSeconds", timeError},
                    {"localOrientationOnly",local},{"localAxis",localAxis},{"angleDegrees",angle}};
                QFile report(reportFile);
                const auto json = QJsonDocument(result).toJson();
                const bool saved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
                std::cout << "Main-window trajectory transform: " << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                qApp->exit(valid && deleted && saved && screenshot ? 0 : 2);
            });
            return;
        }
        if(arguments.contains(QStringLiteral("--profile-cdf-top1"))) {
            setupTimeout->stop();
            auto* solve = state->editor->findChild<QPushButton*>(QStringLiteral("solveAllIk"));
            auto* graph = state->editor->findChild<QPushButton*>(QStringLiteral("layeredGraphFilter"));
            auto* results = state->editor->findChild<QTableWidget*>(QStringLiteral("layeredGraphResults"));
            auto* use = state->editor->findChild<QPushButton*>(QStringLiteral("useLayeredGraphResult"));
            if(!solve || !graph || !results || !use || !solve->isEnabled()) {
                fail("original all-IK / Top-M controls are unavailable"); return;
            }
            const QString folder = reportFile + QStringLiteral(".stages");
            if(!QDir().mkpath(folder)) { fail("cannot create CDF profile output directory"); return; }
            auto* poll = new QTimer(&window);
            poll->setInterval(250);
            auto phase = std::make_shared<int>(0);
            auto elapsed = std::make_shared<QElapsedTimer>(); elapsed->start();
            QObject::connect(poll, &QTimer::timeout, &window,
                [&window, state, graph, results, use, poll, phase, elapsed, folder, reportFile]() {
                if(elapsed->elapsed() > 600000) { poll->stop(); fail("all-IK / Top-M setup timeout"); return; }
                if(*phase == 0) {
                    if(!graph->isEnabled()) return;
                    *phase = 1; graph->click();
                    std::cout << "Main-window CDF profile: all-IK complete, solving Top-M" << std::endl;
                    return;
                }
                if(results->rowCount() == 0 || !use->isEnabled()) return;
                poll->stop();
                auto* tabs = state->editor->findChild<QTabWidget*>(QStringLiteral("layeredGraphResultTabs"));
                if(tabs) tabs->setCurrentIndex(0);
                results->selectRow(0);
                QStringList topRow;
                for(int c=0;c<results->columnCount();++c)
                    topRow.push_back(results->item(0,c) ? results->item(0,c)->text() : QString());
                use->click();
                auto* input = state->editor->findChild<QTableWidget*>(QStringLiteral("cdfInitialJointAngles"));
                auto* margin = state->editor->findChild<QDoubleSpinBox*>(QStringLiteral("cdfSafetyMargin"));
                auto* repair = state->editor->findChild<QPushButton*>(QStringLiteral("runGaApfCdf"));
                if(!input || input->rowCount()==0 || !margin || margin->value()!=0.01 || !repair || !repair->isEnabled()) {
                    fail("original CDF input / restored 10 mm margin is unavailable"); return;
                }
                QFile seed(folder + QStringLiteral("/top1-displayed.txt"));
                if(!seed.open(QIODevice::WriteOnly)) { fail("cannot save displayed Top-1 input"); return; }
                seed.write("# Joint angle unit: degrees\ntime_s\tJ1_deg\tJ2_deg\tJ3_deg\tJ4_deg\tJ5_deg\tJ6_deg\n");
                for(int row=0;row<input->rowCount();++row) {
                    QStringList values;
                    for(int c=1;c<input->columnCount();++c)
                        values.push_back(input->item(row,c) ? input->item(row,c)->text() : QString());
                    seed.write(values.join('\t').toUtf8()); seed.write("\n");
                }
                seed.close();
                auto* sprayMode=state->editor->findChild<QCheckBox*>(QStringLiteral("cdfSprayGuidance"));
                const bool guided=sprayMode && sprayMode->isEnabled() && sprayMode->isChecked();
                if(qApp->arguments().contains(QStringLiteral("--profile-spray-guidance")) && !guided) {
                    fail("calibrated spray guidance is not enabled on original CDF page"); return;
                }
                const int inputCount=input->rowCount();
                std::cout << "Main-window CDF profile: original repair button, Top-1="
                    << topRow.join(QStringLiteral(" | ")).toStdString() << std::endl;
                QElapsedTimer repairClock; repairClock.start();
                repair->click(); // Production controller owns its modal worker/event loop.
                const auto repairMs=repairClock.elapsed();
                auto* stages=state->editor->findChild<QComboBox*>(QStringLiteral("cdfStageSelection"));
                auto* exporter=state->editor->findChild<QPushButton*>(QStringLiteral("cdfStageExport"));
                auto* finalExport=button(*state->editor,QStringLiteral("Export APF + CDF/QP trajectory..."));
                const bool outputAvailable=finalExport && finalExport->isEnabled();
                QJsonArray labels;
                for(auto* label:state->editor->findChildren<QLabel*>())
                    if(!label->text().isEmpty()) labels.append(label->text());
                QJsonArray exports;
                if(stages && exporter) for(int i=0;i<stages->count();++i) {
                    stages->setCurrentIndex(i);
                    const QString path=folder+QStringLiteral("/stage-%1.txt").arg(i);
                    ImportFileResponder save(path); qApp->installEventFilter(&save);
                    exporter->click(); qApp->removeEventFilter(&save);
                    exports.append(QJsonObject{{"stage",stages->itemText(i)}, {"path",path}, {"saved",QFile::exists(path)}});
                }
                const QString quality=folder+QStringLiteral("/quality.csv");
                ImportFileResponder save(quality); qApp->installEventFilter(&save);
                QMetaObject::invokeMethod(state->editor,"exportCdfQualityRequested",Qt::DirectConnection);
                qApp->removeEventFilter(&save);
                const QJsonObject result{{"outputAvailable",outputAvailable},{"inputPoints",inputCount},
                    {"safetyMarginMeters",margin->value()},{"sprayGuidanceEnabled",guided},{"repairElapsedMs",double(repairMs)},
                    {"top1",topRow.join(QStringLiteral(" | "))},{"labels",labels},{"exports",exports}};
                QFile report(reportFile); const auto json=QJsonDocument(result).toJson();
                const bool saved=report.open(QIODevice::WriteOnly) && report.write(json)==json.size();
                const bool screenshot=window.grab().save(reportFile+QStringLiteral(".png"));
                std::cout << "Main-window CDF profile: output=" << outputAvailable
                    << ", repair ms=" << repairMs << ", report=" << reportFile.toStdString() << std::endl;
                qApp->exit(outputAvailable && exports.size()==(guided?4:3) && saved && screenshot ? 0 : 2);
            });
            IkSettingsResponder settingsResponder;
            qApp->installEventFilter(&settingsResponder);
            solve->click();
            qApp->removeEventFilter(&settingsResponder);
            elapsed->restart(); poll->start();
            return;
        }
        if(arguments.contains(QStringLiteral("--profile-trajectory-orientations"))) {
            setupTimeout->stop();
            auto* axes = state->editor->findChild<QCheckBox*>(QStringLiteral("trajectoryOrientationsVisible"));
            auto* points = state->editor->findChild<QCheckBox*>(QStringLiteral("trajectoryPointsVisible"));
            auto* table = state->editor->findChild<QTableWidget*>(QStringLiteral("cartesianControlPoints"));
            const auto poses = readProfilePoses(trajectoryFile);
            bool lastSourcePresent = false;
            if(table) for(int i = 0; i < table->rowCount(); ++i) {
                if(table->item(i,0) && table->item(i,0)->text().toInt() == int(poses.size())) lastSourcePresent = true;
            }
            if(!axes || !points || !table || poses.empty() || !lastSourcePresent || axes->isChecked()) {
                fail("orientation toggle/default/imported pose count incorrect"); return;
            }
            points->setChecked(false);
            viewport->setEnabled(false);
            for(QWidget* parent = axes->parentWidget(); parent; parent = parent->parentWidget()) {
                if(auto* scroll = qobject_cast<QScrollArea*>(parent)) { scroll->ensureWidgetVisible(axes); }
            }
            QTimer::singleShot(500, &window, [&window, viewport, axes, points, table, reportFile, count = poses.size()]() {
                const QImage off = viewport->grabFramebuffer();
                off.save(reportFile + QStringLiteral(".off.png"));
                axes->setChecked(true);
                QTimer::singleShot(500, &window, [&window, viewport, axes, points, table, off, reportFile, count]() {
                    const QImage on = viewport->grabFramebuffer();
                    on.save(reportFile + QStringLiteral(".axes.png"));
                    window.grab().save(reportFile + QStringLiteral(".png"));
                    int changed = 0, red = 0, green = 0, blue = 0;
                    if(on.size() == off.size()) {
                        for(int y = 0; y < on.height(); ++y) for(int x = 0; x < on.width(); ++x) {
                            const QColor a = off.pixelColor(x,y), b = on.pixelColor(x,y);
                            if(a == b) { continue; }
                            ++changed;
                            red += b.red() > 180 && b.green() < 80 && b.blue() < 80;
                            green += b.green() > 180 && b.red() < 80 && b.blue() < 80;
                            blue += b.blue() > 180 && b.red() < 80 && b.green() < 80;
                        }
                    }
                    const bool independent = !points->isChecked() && axes->isChecked();
                    axes->setChecked(false);
                    QTimer::singleShot(500, &window, [viewport, off, reportFile, count, changed, red, green, blue, independent]() {
                        const QImage hidden = viewport->grabFramebuffer();
                        hidden.save(reportFile + QStringLiteral(".hidden.png"));
                        const bool restored = hidden == off;
                        const bool passed = independent && restored && changed > 30 && red > 5 && green > 5 && blue > 5;
                        const QJsonObject result{{"passed",passed},{"sourcePoints",int(count)},
                            {"independentOfSphereMarkers",independent},{"hideRestoresViewport",restored},
                            {"changedPixels",changed},{"redAxisPixels",red},{"greenAxisPixels",green},{"blueAxisPixels",blue},
                            {"renderer","original MainWindow"}};
                        QFile file(reportFile); const auto bytes = QJsonDocument(result).toJson();
                        const bool saved = file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
                        std::cout << "Main-window trajectory orientations: " << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                        qApp->exit(passed && saved ? 0 : 2);
                    });
                });
            });
            return;
        }
        if(arguments.contains(QStringLiteral("--profile-cartesian-import"))) {
            setupTimeout->stop();
            QFile input(trajectoryFile);
            if(!input.open(QIODevice::ReadOnly)) { fail("cannot read Cartesian fixture"); return; }
            const auto lines = QString::fromUtf8(input.readAll()).split('\n', Qt::SkipEmptyParts);
            auto* table = state->editor->findChild<QTableWidget*>(QStringLiteral("cartesianControlPoints"));
            auto* points = state->editor->findChild<QCheckBox*>(QStringLiteral("trajectoryPointsVisible"));
            auto* margin = state->editor->findChild<QDoubleSpinBox*>(QStringLiteral("cdfSafetyMargin"));
            bool valid = table && points && margin && margin->value() == 0.01 && table->rowCount() == lines.size();
            double maxPositionError = 0.0, maxTimeError = 0.0;
            const QRegularExpression number(QStringLiteral(R"([-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?)"));
            for(int i = 0; valid && i < lines.size(); ++i) {
                const auto fields = lines[i].trimmed().split(QRegularExpression(QStringLiteral("\\s+")));
                if(fields.size() != 12 || !table->item(i, 1) || !table->item(i, 2)) { valid = false; break; }
                maxTimeError = std::max(maxTimeError, std::abs(table->item(i, 1)->text().toDouble() - fields[9].toDouble()));
                auto matches = number.globalMatch(table->item(i, 2)->text());
                for(int j = 0; j < 3; ++j) {
                    if(!matches.hasNext()) { valid = false; break; }
                    maxPositionError = std::max(maxPositionError,
                        std::abs(matches.next().captured().toDouble() - fields[j].toDouble() * 0.001));
                }
            }
            valid = valid && maxPositionError < 1.0e-6 && maxTimeError < 1.0e-6;
            if(points) { points->setChecked(true); }
            bool sprayControls = true;
            if(arguments.contains(QStringLiteral("--profile-spray-controls"))) {
                auto* spray = state->editor->findChild<QCheckBox*>(QStringLiteral("cdfSprayGuidance"));
                auto* profile = state->editor->findChild<QLabel*>(QStringLiteral("cdfSprayProfileLabel"));
                sprayControls = spray && profile && spray->isEnabled() && spray->isChecked() && !margin->isEnabled();
                if(sprayControls) {
                    spray->setChecked(false); sprayControls = margin->isEnabled() && margin->value() == 0.01;
                    spray->setChecked(true); sprayControls = sprayControls && !margin->isEnabled() && margin->value() == 0.01;
                    for(auto* tabs : state->editor->findChildren<QTabWidget*>())
                        for(int i=0;i<tabs->count();++i) if(tabs->tabText(i)==QStringLiteral("CDF")) tabs->setCurrentIndex(i);
                }
                valid = valid && sprayControls;
            }
            const QJsonObject result{{"success", valid}, {"sprayControlsPassed", sprayControls}, {"sourcePoints", lines.size()},
                {"displayedPoints", table ? table->rowCount() : 0}, {"maxDisplayedPositionErrorMeters", maxPositionError},
                {"maxDisplayedTimeErrorSeconds", maxTimeError}, {"safetyMarginMeters", margin ? margin->value() : -1}};
            QFile report(reportFile);
            const auto json = QJsonDocument(result).toJson();
            const bool saved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
            QTimer::singleShot(300, &window, [&window, result, valid, saved, reportFile]() {
                const bool screenshot = window.grab().save(reportFile + QStringLiteral(".png"));
                std::cout << "Main-window Cartesian import: " << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                qApp->exit(valid && saved && screenshot ? 0 : 2);
            });
            return;
        }
        const int rapidOption = arguments.indexOf(QStringLiteral("--profile-rapid-export"));
        const int fanucOption = arguments.indexOf(QStringLiteral("--profile-fanuc-export"));
        const int tcpOption = arguments.indexOf(QStringLiteral("--profile-tcp-export"));
        const bool tcp = tcpOption >= 0;
        const bool fanuc = fanucOption >= 0;
        const int exportOption = tcp ? tcpOption : (fanuc ? fanucOption : rapidOption);
        if(exportOption >= 0 && exportOption + 1 < arguments.size()) {
            const QString outputPath = arguments[exportOption + 1];
            auto* exportButton = state->editor->findChild<QPushButton*>(tcp ? QStringLiteral("exportCdfTcpTrajectory") : (fanuc ? QStringLiteral("exportCdfFanuc") : QStringLiteral("exportCdfRapid")));
            if(!exportButton || !exportButton->isEnabled()) {
                fail("CDF robot-program export button was not enabled for the imported CDF result");
                return;
            }
            // Importing can select a scene node and return the main window to
            // Browse. Re-enter through the same action the user clicks.
            for(auto* action : window.findChildren<QAction*>()) {
                if(action->actionGroup() && action->isEnabled() &&
                    (action->text() == QStringLiteral("Motion Planning") ||
                     action->text() == QStringLiteral("action.trajectoryPlanningWorkbench") ||
                     action->text() == QStringLiteral("\u8fd0\u52a8\u89c4\u5212"))) {
                    action->trigger();
                    break;
                }
            }
            for(auto* tabs : state->editor->findChildren<QTabWidget*>()) {
                for(int index = 0; index < tabs->count(); ++index) {
                    if(tabs->tabText(index) == QStringLiteral("CDF")) { tabs->setCurrentIndex(index); }
                }
            }
            for(QWidget* parent = exportButton->parentWidget(); parent; parent = parent->parentWidget()) {
                if(auto* scroll = qobject_cast<QScrollArea*>(parent)) { scroll->ensureWidgetVisible(exportButton); }
            }
            if(!exportButton->isVisible()) {
                for(auto* action : window.findChildren<QAction*>()) {
                    if(action->actionGroup()) std::cerr << "Workbench action: " << action->text().toStdString()
                        << " enabled=" << action->isEnabled() << " checked=" << action->isChecked() << '\n';
                }
                for(QWidget* parent = exportButton; parent; parent = parent->parentWidget()) {
                    std::cerr << "Export ancestor: " << parent->metaObject()->className()
                        << " name=" << parent->objectName().toStdString() << " visible=" << parent->isVisible() << '\n';
                }
                fail("export button is not visible in the original CDF page"); return;
            }
            ImportFileResponder saveResponder(outputPath);
            qApp->installEventFilter(&saveResponder);
            exportButton->click();
            qApp->removeEventFilter(&saveResponder);
            setupTimeout->stop();
            QFile exportedFile(outputPath);
            if(!exportedFile.open(QIODevice::ReadOnly)) { fail("robot program was not saved"); return; }
            const QString program = QString::fromUtf8(exportedFile.readAll());
            const QRegularExpression targetPattern(fanuc ? QStringLiteral(
                R"(P\[(\d+)\]\{\s+GP1:\s+UF : 2, UT : 1,\s+CONFIG : 'N U T, 0, 0, 0',\s+X = ([-0-9.]+)  mm,\s+Y = ([-0-9.]+)  mm,\s+Z = ([-0-9.]+)  mm,\s+W = ([-0-9.]+) deg,\s+P = ([-0-9.]+) deg,\s+R = ([-0-9.]+) deg\s+\};)") : QStringLiteral(
                R"(CONST\s+robtarget\s+p(\d+)\s*:=\s*\[\[([^\]]+)\],\[([^\]]+)\])"));
            auto targets = targetPattern.globalMatch(program);
            std::vector<Eigen::Isometry3d> poses;
            bool parsed = fanuc ? (program.startsWith(QStringLiteral("/PROG  %1\r\n").arg(QFileInfo(outputPath).completeBaseName().toUpper())) &&
                program.endsWith(QStringLiteral("/END\r\n")) &&
                program.contains(QStringLiteral("LINE_COUNT\t= %1;").arg(sourcePoints + 2)) &&
                program.count(QStringLiteral("400mm/sec CNT100")) == sourcePoints) :
                (program.startsWith(QStringLiteral("MODULE MainModule")) &&
                !program.contains(QStringLiteral("tooldata")) &&
                program.count(QStringLiteral(", v50, z10, tool0\\WObj:=wobj0;")) == sourcePoints);
            std::vector<ProfilePose> tcpPoints;
            if(tcp) {
                tcpPoints = readProfilePoses(outputPath);
                parsed = !tcpPoints.empty();
                for(const auto& point : tcpPoints) {
                    Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
                    pose.matrix() = point.pose;
                    poses.push_back(pose);
                }
            }
            while(!tcp && targets.hasNext()) {
                const auto match = targets.next();
                if(fanuc) {
                    if(match.captured(1).toInt() != poses.size() + 1) { parsed = false; break; }
                    Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
                    for(int j = 0; j < 3; ++j) pose.translation()[j] = match.captured(j + 2).toDouble() * .001;
                    const double radians = 3.14159265358979323846 / 180.0;
                    pose.linear() = (Eigen::AngleAxisd(match.captured(7).toDouble() * radians, Eigen::Vector3d::UnitZ()) *
                        Eigen::AngleAxisd(match.captured(6).toDouble() * radians, Eigen::Vector3d::UnitY()) *
                        Eigen::AngleAxisd(match.captured(5).toDouble() * radians, Eigen::Vector3d::UnitX())).toRotationMatrix();
                    poses.push_back(pose);
                    continue;
                }
                const auto position = match.captured(2).split(',');
                const auto rotation = match.captured(3).split(',');
                if(position.size() != 3 || rotation.size() != 4 || match.captured(1).toInt() != poses.size() + 1) {
                    parsed = false;
                    break;
                }
                Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
                for(int j = 0; j < 3; ++j) { pose.translation()[j] = position[j].toDouble() * 0.001; }
                pose.linear() = Eigen::Quaterniond(rotation[0].toDouble(), rotation[1].toDouble(),
                    rotation[2].toDouble(), rotation[3].toDouble()).normalized().toRotationMatrix();
                poses.push_back(pose);
            }
            QFile sourceFile(trajectoryFile);
            if(!sourceFile.open(QIODevice::ReadOnly)) { fail("cannot reopen source trajectory"); return; }
            const auto source = QJsonDocument::fromJson(sourceFile.readAll()).object();
            const auto sourceRows = source.value(QStringLiteral("trajectory")).toObject()
                .value(QStringLiteral("points")).toArray();
            std::vector<std::string> names;
            for(const auto& name : source.value(QStringLiteral("jointNames")).toArray()) {
                names.push_back(name.toString().toStdString());
            }
            const auto fk = viewport->robotForwardKinematics(source.value(QStringLiteral("robotId")).toString(), names, true);
            bool valid = parsed && fk && poses.size() == sourcePoints;
            double positionError = 0.0, orientationError = 0.0, timeError = 0.0;
            for(int i = 0; valid && i < sourcePoints; ++i) {
                std::vector<double> q;
                for(const auto& value : sourceRows[i].toObject().value(QStringLiteral("q")).toArray()) { q.push_back(value.toDouble()); }
                // This diagnostic fixture is an ABB4600 CDF plan in stored IK convention.
                if(q.size() != 6) { valid = false; break; }
                for(const int axis : {0, 3, 4, 5}) { q[axis] = -q[axis]; }
                if(tcp) timeError = std::max(timeError,
                    std::abs(tcpPoints[i].time - sourceRows[i].toObject().value(QStringLiteral("time")).toDouble()));
                const auto expected = fk(q);
                const auto& actual = poses[i];
                if(!actual.matrix().allFinite()) { valid = false; break; }
                positionError = std::max(positionError, (expected.translation() - actual.translation()).norm());
                orientationError = std::max(orientationError,
                    Eigen::AngleAxisd(expected.linear().transpose() * actual.linear()).angle());
            }
            valid = valid && positionError < (fanuc ? 9e-7 : 1e-8) && orientationError < (fanuc ? 3e-5 : 1e-8) && timeError < 1e-12;
            const QJsonObject result{{"success", valid}, {"sourcePoints", sourcePoints},
                {"exportedPoints", int(poses.size())},
                {"maxPositionErrorMeters", positionError}, {"maxOrientationErrorRadians", orientationError}, {"maxTimeErrorSeconds", timeError}};
            QFile report(reportFile);
            const auto json = QJsonDocument(result).toJson();
            const bool saved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
            QTimer::singleShot(150, &window, [&window, reportFile, result, valid, saved, fanuc, tcp]() {
                const bool screenshot = window.grab().save(reportFile + QStringLiteral(".png"));
                std::cout << (tcp ? "Main-window TCP matrix export: " : (fanuc ? "Main-window FANUC export: " : "Main-window RAPID export: "))
                    << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                qApp->exit(valid && saved && screenshot ? 0 : 2);
            });
            return;
        }
        if(arguments.contains(QStringLiteral("--profile-basic-ik"))) {
            auto* solve=button(*state->editor,QStringLiteral("Solve IK and apply"));
            if(!solve || !solve->isEnabled()){fail("basic IK solve button unavailable");return;}
            solve->click();
            for(int i=1;i<=6;++i)state->firstJoints.push_back(viewport->robotJointValue(
                QStringLiteral("m710ic_50_drake"),QStringLiteral("J%1").arg(i)));
        }
        state->play = button(*state->editor, QStringLiteral("Play IK result"));
        if(!state->play || !state->play->isEnabled()) {
            fail("import did not enable the original playback control");
            return;
        }
        if(auto* trace = state->editor->findChild<QCheckBox*>(QStringLiteral("endEffectorTraceVisible"))) {
            trace->setChecked(true);
        }
        for(auto* spin : state->editor->findChildren<QDoubleSpinBox*>()) {
            if(spin->toolTip().startsWith(QStringLiteral("Preview duration."))) {
                spin->setValue(5.0);
            }
        }
        state->profileDistance = arguments.contains(QStringLiteral("--profile-collision-distance"));
        if(state->profileDistance) {
            viewport->resetCamera();
            // Exercise the existing main-window query switch, not a private test viewport.
            for(auto* action : window.findChildren<QAction*>()) {
                if(action->text() == QStringLiteral("Collision Detection") && action->isCheckable()) {
                    action->setChecked(true);
                    break;
                }
            }
            if(!viewport->collisionQueriesEnabled()) {
                fail("original Collision Detection action did not enable queries"); return;
            }
        }
        // Allow import layout/overlays to settle. Timing excludes creation of
        // the playback collision scene, which takes place inside play->click().
        QTimer::singleShot(500, &window, [state, heartbeat, setupTimeout]() {
            if(!state->play) {
                fail("playback control was destroyed during setup");
                return;
            }
            state->play->click();
            setupTimeout->stop();
            state->clock.start();
            state->started = true;
            heartbeat->start();
        });
    });
}

// Exercises the real Import Object action, unit dialog, viewport reload and project IO.
void startStlImportWindowProfile(QWidget& window, const QString& meshFile, const QString& reportFile)
{
    window.showMaximized();
    QTimer::singleShot(5000, &window, [&window, meshFile, reportFile]() {
        QAction* importAction = nullptr;
        QAction* saveAction = nullptr;
        for(auto* action : window.findChildren<QAction*>()) {
            if(action->text() == QStringLiteral("Import Object")) importAction = action;
            if(action->text() == QStringLiteral("Save Project As")) saveAction = action;
        }
        if(!importAction || !saveAction || !importAction->isEnabled()) { fail("STL import actions unavailable"); return; }
        const auto saveSnapshot = [&](const QString& suffix, simulation_project::ProjectDocument& document) {
            const QString path = reportFile + suffix + QStringLiteral(".sys.json");
            ImportFileResponder responder(path);
            qApp->installEventFilter(&responder);
            saveAction->trigger();
            qApp->removeEventFilter(&responder);
            std::string error;
            return simulation_project::loadProjectDocument(std::filesystem::path(path.toStdWString()), document, &error);
        };
        class UnitResponder : public QObject {
        public:
            int mode = 0;
            bool handled = false;
            QString screenshot;
            bool eventFilter(QObject* object, QEvent* event) override {
                if(!handled && event->type() == QEvent::Show && object->objectName() == QStringLiteral("stlImportUnitDialog")) {
                    if(auto* dialog = qobject_cast<QInputDialog*>(object)) {
                        handled = true;
                        QTimer::singleShot(0, dialog, [this, dialog]() {
                            if(mode < 0) { dialog->reject(); return; }
                            const auto items = dialog->comboBoxItems();
                            if(items.size() != 2) { dialog->reject(); return; }
                            dialog->setTextValue(items[mode]);
                            dialog->grab().save(screenshot);
                            dialog->accept();
                        });
                    }
                }
                return QObject::eventFilter(object, event);
            }
        };
        simulation_project::ProjectDocument before, cancelled, after;
        bool valid = saveSnapshot(QStringLiteral(".before"), before);
        bool allDialogs = true;
        for(int mode : {-1, 0, 1}) {
            ImportFileResponder fileResponder(meshFile);
            UnitResponder unitResponder;
            unitResponder.mode = mode;
            unitResponder.screenshot = reportFile + QStringLiteral(".unit%1.png").arg(mode);
            qApp->installEventFilter(&fileResponder);
            qApp->installEventFilter(&unitResponder);
            importAction->trigger();
            qApp->removeEventFilter(&unitResponder);
            qApp->removeEventFilter(&fileResponder);
            allDialogs = allDialogs && unitResponder.handled;
            if(mode == -1) valid = saveSnapshot(QStringLiteral(".cancelled"), cancelled) && valid;
        }
        valid = saveSnapshot(QStringLiteral(".after"), after) && valid;
        const bool cancellationPassed = before.objects.size() == cancelled.objects.size();
        bool meterPassed = false, millimeterPassed = false;
        const std::string name = QFileInfo(meshFile).completeBaseName().toStdString();
        for(const auto& object : after.objects) {
            if(object.name != name) continue;
            meterPassed = meterPassed || (object.visualScale == 1.0 && object.collisionScale == 1.0);
            millimeterPassed = millimeterPassed || (object.visualScale == .001 && object.collisionScale == .001);
        }
        valid = valid && allDialogs && cancellationPassed && meterPassed && millimeterPassed &&
            after.objects.size() == before.objects.size() + 2;
        const QJsonObject result{{"success",valid},{"unitDialogsShown",allDialogs},{"cancellationPassed",cancellationPassed},
            {"meterScalesPassed",meterPassed},{"millimeterScalesPassed",millimeterPassed},
            {"beforeObjects",int(before.objects.size())},{"afterObjects",int(after.objects.size())}};
        const auto bytes = QJsonDocument(result).toJson();
        QFile report(reportFile);
        const bool saved = report.open(QIODevice::WriteOnly) && report.write(bytes) == bytes.size();
        QTimer::singleShot(300, &window, [&window, reportFile, valid, saved]() {
            const bool screenshot = window.grab().save(reportFile + QStringLiteral(".png"));
            qApp->exit(valid && saved && screenshot ? 0 : 2);
        });
    });
}
