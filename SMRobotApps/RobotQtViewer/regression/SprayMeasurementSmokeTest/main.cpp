#include <QWindow>
#include <ProjectMotionPlanning/GaApfCdfPipeline.h>
#include <nlohmann/json.hpp>
#include <ProjectMotionPlanning/CartesianMotion.h>
#include <ProjectMotionPlanning/TrajectoryControlPointEditing.h>
#include <ProjectScene.h>
#include <ProjectMotionPlanning/ProjectMotionPlanning.h>
#include "../../../../SMRobotMotionPlanning/ProjectMotionPlanning/src/ApfLocalPlanner.h"
#include "../../../../SMRobotMotionPlanning/ProjectMotionPlanning/src/PathRefinement.h"
#include "../../../../SMRobotMotionPlanning/ProjectMotionPlanning/src/CdfQueryBatch.h"
#include <MotionPlanningEditorWidget.h>
#include <ConfigurationSelectionDialog.h>
#include <CdfTrajectoryAnalysisDialog.h>
#include "RobotQtViewerTheme.h"
#include <DigitalTwinTelemetryPanel.h>
#include <DigitalTwinTelemetryController.h>
#include <QListWidget>
#include <QRegularExpression>
#include <QPointer>
#include <MotionPlanningModuleController.h>
#include <RobotQtViewerDocumentContext.h>
#include <RobotQtViewerDocumentController.h>
#include <RobotQtViewerEventHub.h>
#include <RobotQtViewerOperationStatus.h>
#include <RobotQtViewerSelectionModel.h>
#include <RobotQtViewerViewportPreviewState.h>
#include <RobotViewport.h>
#include "RobotQtViewerViewportServicesAdapter.h"
#include "RobotQtViewerViewportEventController.h"
#include <SceneExplorerModuleController.h>
#include <SceneExplorerWidget.h>
#include <CollisionWorkbenchModuleController.h>
#include <CollisionWorkbenchPanel.h>
#include <CollisionWorkbenchServices.h>
#include <CollisionDetectorCommandController.h>
#include <SceneEntityWorkflowController.h>
#include <MotionPlanningCore/MotionPlanning.h>
#include <ProjectMotionPlanning/TrajectoryImport.h>
#include <ProjectMotionPlanning/CdfJointAngleImport.h>
#include <QLocale>
#include <QTableWidget>
#include <QComboBox>
#include <QElapsedTimer>
#include <QSpinBox>
#include <QLineEdit>
#include <set>
#include <QLabel>
#include <chrono>
#include <ProjectMotionPlanning/TrajectoryInverseKinematics.h>
#include <ProjectMotionPlanning/LayeredIkGraph.h>
#include <SimulationProject/ProjectIo.h>
#include <SimulationProject/ProjectDocumentService.h>
#include <Eigen/SVD>
#include <iomanip>

#include <QApplication>
#include <QCheckBox>
#include <QTabWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QImage>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QTimer>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace
{
    int failures = 0;
    void require(bool ok, const char* message)
    {
        std::cout << (ok ? "[OK] " : "[FAIL] ") << message << '\n';
        if(!ok) { ++failures; }
    }

    void verifyTelemetryPanel(QApplication& application)
    {
        using namespace robot_qt_viewer;
        struct Port final : IRobotQtViewerTelemetryPort {
            TelemetrySnapshot state;
            int starts = 0, scans = 0, stops = 0;
            bool discover() override { ++scans; state.endpoints = {{QStringLiteral("station-a"), QStringLiteral("A")}, {QStringLiteral("station-b"), QStringLiteral("B")}}; return true; }
            bool start(const QString& id, int group, int interval, const QString& path) override {
                ++starts; state.busy = state.streaming = true; state.source = id;
                state.sequence = 12; state.receivedUnixNs = 1795320000123456789LL;
                state.elapsedNs = 240000000; state.readLatencyNs = 2000000;
                state.joints = {1., -2., 3., 4., 5., 225.2136}; state.csvPath = path;
                state.tcpPoseValid = true; state.tcpXyzwpr = {2044.2, 906.7, 667.6, -178.9, 5.8, 5.5};
                state.tcpUserFrame = 0; state.tcpUserTool = 1;
                require(group == 1 && interval == 20 && !path.isEmpty(), "Telemetry controller forwards requested recording options"); return true;
            }
            void stop() override { ++stops; state.busy = state.streaming = false; }
            TelemetrySnapshot snapshot() const override { return state; }
            TelemetrySpeedStatistics speedStatistics(double start, double end, TelemetrySpeedCalculationMethod method, double minimumInterval) const override {
                TelemetrySpeedStatistics result; result.valid = end > start; result.startSeconds = start; result.endSeconds = end;
                result.method = method; result.minimumIntervalSeconds = minimumInterval;
                result.sampleCount = result.valid ? 10 : 0; result.tcpLinearAverageMmPerSecond = 12.; result.tcpLinearMaxMmPerSecond = 20.;
                result.tcpAngularAverageDegPerSecond = 3.; result.tcpAngularMaxDegPerSecond = 5.; result.jointAverageSpeed = {1., 2., 3., 4., 5., 6.}; result.jointMaxSpeed = {2., 4., 6., 8., 10., 12.};
                for(int i = 0; i < 10; ++i) { TelemetrySpeedPoint point; point.elapsedSeconds = start + (end - start) * i / 9.; point.tcpLinearMmPerSecond = i; point.tcpAngularDegPerSecond = i * .5; point.jointAbsSpeed = {double(i), 2., 3., 4., 5., 6.}; result.points.push_back(point); }
                if(!result.valid) result.diagnostic = QStringLiteral("bad interval");
                return result;
            }
        } port;
        RobotViewport followViewport;
        struct FollowViewportPort final : RobotQtViewerMotionPlanningViewportAdapter {
            explicit FollowViewportPort(RobotViewport& view) : RobotQtViewerMotionPlanningViewportAdapter(view) {}
            int applies = 0, traces = 0, clears = 0;
            bool visible = false;
            std::vector<double> received;
            bool setRobotJointValuesByDof(const std::vector<double>& values, QString* robotId) override {
                ++applies; received = values; *robotId = QStringLiteral("scene-six-axis"); return true;
            }
            void setTelemetryEndEffectorTraceVisible(bool value) override { visible = value; }
            void appendTelemetryEndEffectorTraceSample(const QString&) override { ++traces; }
            void clearTelemetryEndEffectorTrace() override { ++clears; }
        } followPort(followViewport);
        DigitalTwinTelemetryPanel panel(nullptr);
        DigitalTwinTelemetryController controller(panel, port, &followPort);
        panel.resize(440, 760); panel.show();
        auto* start = panel.findChild<QPushButton*>(QStringLiteral("telemetryStart"));
        auto* stop = panel.findChild<QPushButton*>(QStringLiteral("telemetryStop"));
        auto* refresh = panel.findChild<QPushButton*>(QStringLiteral("telemetryRefresh"));
        auto* endpoints = panel.findChild<QComboBox*>(QStringLiteral("telemetryEndpoints"));
        auto* joints = panel.findChild<QTableWidget*>(QStringLiteral("telemetryJoints"));
        require(start && stop && refresh && endpoints && joints, "Digital twin communication panel exposes controls");
        if(!start || !stop || !refresh || !endpoints || !joints) return;
        refresh->click();
        require(port.scans == 1 && endpoints->count() == 3, "Refresh displays all discovered controllers");
        endpoints->setCurrentIndex(2); start->click(); application.processEvents();
        require(port.starts == 1 && port.state.source == QStringLiteral("station-b") && !start->isEnabled() && stop->isEnabled(),
            "Start uses selected workstation and prevents duplicate starts");
        require(joints->rowCount() == 6 && joints->item(5,1)->text().contains(QStringLiteral("225.213600")), "UI preserves full joint turns");
        auto* follow = panel.findChild<QCheckBox*>(QStringLiteral("telemetrySceneRobotFollow"));
        auto* traceVisible = panel.findChild<QCheckBox*>(QStringLiteral("telemetryEndEffectorTraceVisible"));
        require(follow && traceVisible && !follow->isChecked() && !traceVisible->isChecked() && followPort.applies == 0,
            "Scene follow and telemetry trace are opt-in switches");
        if(follow && traceVisible) {
            follow->setChecked(true);
            require(followPort.applies == 1 && followPort.received == port.state.joints && followPort.traces == 1,
                "Enabling scene follow applies the current received joint group and collects TCP");
            QElapsedTimer wait; wait.start();
            while(wait.elapsed() < 90) application.processEvents();
            require(followPort.applies == 1, "Repeated polling does not reapply a stale telemetry frame");
            const int clears = followPort.clears;
            traceVisible->setChecked(true); traceVisible->setChecked(false);
            require(!followPort.visible && followPort.clears == clears,
                "Trace visibility changes preserve collected points");
            ++port.state.sequence; port.state.joints[0] = 25.;
            wait.restart(); while(wait.elapsed() < 90) application.processEvents();
            require(followPort.applies == 2 && followPort.traces == 2 && followPort.received[0] == 25.,
                "A new communication frame updates scene and hidden TCP trace");
            follow->setChecked(false); ++port.state.sequence;
            wait.restart(); while(wait.elapsed() < 90) application.processEvents();
            require(followPort.applies == 2, "Disabling follow leaves telemetry active without moving the scene");
        }
        auto* time = panel.findChild<QLabel*>(QStringLiteral("telemetryTime"));
        require(time && time->text().contains(QString::number(port.state.receivedUnixNs)) && time->text().contains(QStringLiteral("0.240000")), "UI shows receipt timestamp and monotonic elapsed time");
        auto* speedStart = panel.findChild<QDoubleSpinBox*>(QStringLiteral("telemetrySpeedStart"));
        auto* speedEnd = panel.findChild<QDoubleSpinBox*>(QStringLiteral("telemetrySpeedEnd"));
        auto* speedMethod = panel.findChild<QComboBox*>(QStringLiteral("telemetrySpeedMethod"));
        auto* speedMinimum = panel.findChild<QDoubleSpinBox*>(QStringLiteral("telemetrySpeedMinimumInterval"));
        auto* speedButton = panel.findChild<QPushButton*>(QStringLiteral("telemetrySpeedCalculate"));
        auto* openSpeedButton = panel.findChild<QPushButton*>(QStringLiteral("telemetrySpeedOpenWindow"));
        auto* speedTabs = panel.findChild<QTabWidget*>(QStringLiteral("telemetrySpeedTabs"));
        auto* speedInfo = panel.findChild<QLabel*>(QStringLiteral("telemetrySpeedInfo"));
        auto* mainTabs = panel.findChild<QTabWidget*>(QStringLiteral("telemetryMainTabs"));
        auto* tcpSpeedPlot = panel.findChild<QWidget*>(QStringLiteral("telemetryTcpSpeedPlot"));
        auto* jointSpeedPlot = panel.findChild<QWidget*>(QStringLiteral("telemetryJoint1SpeedPlot"));
        require(speedStart && speedEnd && speedMethod && speedMinimum && speedButton && openSpeedButton && speedTabs && speedInfo && mainTabs && tcpSpeedPlot && jointSpeedPlot,
            "Speed analysis exposes interval and plot controls");
        if(speedStart && speedEnd && speedMethod && speedMinimum && speedButton && openSpeedButton && speedTabs && speedInfo && mainTabs && tcpSpeedPlot && jointSpeedPlot) {
            speedStart->setValue(0.1); speedEnd->setValue(2.0); speedButton->click(); application.processEvents();
            require(speedTabs->count() == 8 && speedTabs->tabText(0).contains(QStringLiteral("TCP")) && speedTabs->tabText(1).contains(QStringLiteral("TCP")) &&
                speedTabs->tabText(2).contains(QStringLiteral("J1")) && speedTabs->tabText(7).contains(QStringLiteral("J6")) &&
                speedInfo->text().contains(QStringLiteral("20.000")) && speedInfo->text().contains(QStringLiteral("J6")),
                "Speed analysis separates TCP linear, TCP angular, and each joint curve");
            mainTabs->setCurrentIndex(1); application.processEvents();
            require(tcpSpeedPlot->isVisible() && tcpSpeedPlot->height() >= 200 && jointSpeedPlot->height() >= 200,
                "TCP speed plot remains visible at a usable height on the dedicated page");
            speedTabs->setCurrentIndex(2); application.processEvents();
            require(jointSpeedPlot->isVisible() && jointSpeedPlot->height() >= 200,
                "Joint speed plot remains visible at a usable height on the dedicated page");
            speedMethod->setCurrentIndex(1); speedMinimum->setValue(.5); speedButton->click(); application.processEvents();
            require(speedInfo->text().contains(QStringLiteral("20.000")),
                "Window-average speed method accepts a minimum calculation interval");
            mainTabs->setCurrentIndex(1); application.processEvents();
            panel.grab().save(QStringLiteral("digital_twin_speed_panel.png"));
            openSpeedButton->click(); application.processEvents();
            auto* speedDialog = panel.findChild<QDialog*>(QStringLiteral("telemetrySpeedDialog"));
            auto* dialogPlotTabs = panel.findChild<QTabWidget*>(QStringLiteral("telemetryDialogSpeedTabs"));
            require(speedDialog && speedDialog->isVisible() && dialogPlotTabs && dialogPlotTabs->count() == 8 &&
                dialogPlotTabs->tabText(0).contains(QStringLiteral("TCP")) && dialogPlotTabs->tabText(1).contains(QStringLiteral("TCP")) &&
                dialogPlotTabs->tabText(2).contains(QStringLiteral("J1")) && dialogPlotTabs->tabText(7).contains(QStringLiteral("J6")),
                "Speed analysis opens a separate window with individual TCP and joint pages");
            if(speedDialog) speedDialog->grab().save(QStringLiteral("digital_twin_speed_dialog.png"));
            if(speedDialog) speedDialog->close();
            speedStart->setValue(2.0); speedEnd->setValue(1.0); speedButton->click(); application.processEvents();
            require(speedInfo->text().contains(QStringLiteral("bad interval")), "Invalid speed interval is reported without stale statistics");
        }
        auto* dataTabs = panel.findChild<QTabWidget*>(QStringLiteral("telemetryDataTabs"));
        auto* tcp = panel.findChild<QTableWidget*>(QStringLiteral("telemetryTcpPose"));
        auto* tcpInfo = panel.findChild<QLabel*>(QStringLiteral("telemetryTcpInfo"));
        require(dataTabs && dataTabs->count() == 2 && tcp && tcpInfo, "Telemetry separates joint and TCP pose pages");
        if(dataTabs && tcp && tcpInfo) {
            dataTabs->setCurrentIndex(1);
            require(tcp->rowCount() == 6 && tcp->item(0, 1)->text() == QStringLiteral("2044.200000") &&
                tcp->item(3, 1)->text() == QStringLiteral("-178.900000") && tcpInfo->text().contains(QStringLiteral("UF0")) &&
                tcpInfo->text().contains(QStringLiteral("UT1")), "TCP page displays XYZ/WPR and explicit frame/tool identity");
            port.state.tcpPoseValid = false; port.state.tcpDiagnostic = QStringLiteral("Unsupported pose");
            panel.render(port.snapshot());
            require(tcp->rowCount() == 0 && tcpInfo->text().contains(QStringLiteral("Unsupported pose")) && joints->rowCount() == 6,
                "Unavailable pose clears old coordinates without losing joint display");
            port.state.tcpPoseValid = true; port.state.tcpDiagnostic.clear(); panel.render(port.snapshot());
        }
        panel.grab().save(QStringLiteral("digital_twin_telemetry_panel.png"));
        stop->click(); panel.render(port.snapshot());
        require(port.stops == 1 && start->isEnabled() && !stop->isEnabled(), "Stop returns UI to idle");
    }

    void writeFixture(const std::filesystem::path& folder)
    {
        std::ofstream(folder / "gun.urdf") << R"(<robot name="gun">
<link name="base"/><link name="carriage"/><link name="Link6"><visual><geometry><mesh filename="nozzle.stl"/></geometry></visual><collision><geometry><sphere radius="0.01"/></geometry></collision></link>
<joint name="slide" type="prismatic"><parent link="base"/><child link="carriage"/>
<axis xyz="1 0 0"/><limit lower="-10" upper="10" effort="1" velocity="1"/></joint>
<joint name="tilt" type="revolute"><parent link="carriage"/><child link="Link6"/>
<axis xyz="1 0 0"/><limit lower="-3.14" upper="3.14" effort="1" velocity="1"/></joint>
</robot>)";
        std::ofstream nozzle(folder / "nozzle.stl");
        nozzle << "solid nozzle\n";
        const Eigen::Vector3d vertices[] = {{-.025,-.025,-.065},{.025,-.025,-.065},{0,.025,-.065},{0,0,0}};
        const int faces[][3] = {{0,2,1},{0,1,3},{1,2,3},{2,0,3}};
        for(const auto& face:faces) {
            const Eigen::Vector3d n=(vertices[face[1]]-vertices[face[0]]).cross(vertices[face[2]]-vertices[face[0]]).normalized();
            nozzle << "facet normal "<<n.x()<<' '<<n.y()<<' '<<n.z()<<"\nouter loop\n";
            for(int index:face) nozzle << "vertex "<<vertices[index].x()<<' '<<vertices[index].y()<<' '<<vertices[index].z()<<'\n';
            nozzle << "endloop\nendfacet\n";
        }
        nozzle << "endsolid nozzle\n"; nozzle.close();
        for(bool reversed : { false, true }) {
            const auto filename = reversed ? "flipped.stl" : "surface.stl";
            std::ofstream stl(folder / filename);
            stl << "solid surface\n";
            // Far face deliberately precedes near face to test nearest-hit selection.
            for(double z : { 0.3, 0.6, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 0.0 }) {
                stl << "facet normal 0 0 1\nouter loop\nvertex -10 -10 " << z << '\n';
                stl << (reversed ? "vertex 0 10 " : "vertex 10 -10 ") << z << '\n';
                stl << (reversed ? "vertex 10 -10 " : "vertex 0 10 ") << z << '\n';
                stl << "endloop\nendfacet\n";
            }
            stl << "endsolid surface\n";
            std::ofstream urdf(folder / (reversed ? "flipped.urdf" : "target.urdf"));
            urdf << "<robot name=\"target\"><link name=\"base_link\"><visual><geometry><mesh filename=\""
                 << (folder / filename).generic_string()
                 << "\"/></geometry></visual></link></robot>";
        }
    }

    void verifyFanucTelemetryMapping(const std::filesystem::path& folder)
    {
        const double pi = 3.14159265358979323846;
        const auto source = std::filesystem::path(__FILE__).parent_path() /
            "../../../../data/fanuc_m710ic50/m710ic_50_drake.urdf";
        std::ifstream file(source);
        const std::string original((std::istreambuf_iterator<char>(file)), {});
        require(!original.empty(), "Load shipped FANUC M710 model for telemetry alignment");
        if(original.empty()) return;
        QString stripped = QString::fromStdString(original);
        stripped.remove(QRegularExpression(QStringLiteral("<(visual|collision)>.*?</\\1>"),
            QRegularExpression::DotMatchesEverythingOption));
        // Reverse movable-joint XML order to exercise controller axis names,
        // independently of whichever DOF indices RobotIO assigns internally.
        const QRegularExpression joints(QStringLiteral("<joint name=\"J[1-6]\".*?</joint>"),
            QRegularExpression::DotMatchesEverythingOption);
        QStringList ordered;
        auto matches = joints.globalMatch(stripped);
        while(matches.hasNext()) ordered.push_back(matches.next().captured());
        stripped.remove(joints);
        std::reverse(ordered.begin(), ordered.end());
        stripped.replace(QStringLiteral("</robot>"), ordered.join(QStringLiteral("\n")) + QStringLiteral("\n</robot>"));
        const auto modelPath = folder / "fanuc_telemetry.urdf";
        const auto save = [&](const QString& text) { std::ofstream out(modelPath); out << text.toStdString(); };
        save(stripped);
        simulation_project::ProjectDocument document;
        document.collision.query.enabled = false;
        simulation_project::RobotDesc robot;
        robot.id = "fanuc"; robot.sourceType = "urdf"; robot.sourcePath = modelPath.generic_string();
        robot.collisionEnabled = false;
        document.robots.push_back(robot);
        ProjectScene scene;
        scene.setProjectDocument(document, folder);
        require(scene.initialize(), "FANUC telemetry scene initializes");
        const std::vector<std::vector<double>> encoders = {
            {56.2003708494686, -5.67360292889005, -22.1955620193391,
                104.192029109119, -58.997924125952, 217.27371334755},
            {49.400599926924507, -10.61717389494798, -12.090397887595541,
                -85.741925972108817, 43.717646895947532, 227.48939506270622},
            {0., 25., -30., 10., 20., 725.}};
        // Independent WORLD/UT1 observations from Robot Server, 2026-11-22/24.
        const std::array<std::array<double, 6>, 2> measured = {{
            {1989.43286132813, 889.273132324219, 639.008056640625,
                -3.42402148246765, -2.57907709055871e-6, 4.08158712161821e-6},
            {2044.2376708984375, 906.76544189453125, 667.601318359375,
                -178.92384338378906, 5.80877685546875, 5.5520462989807129}}};
        const auto transform = [](const simulation_project::TransformDesc& d) {
            Eigen::Isometry3d t = Eigen::Isometry3d::Identity();
            t.translation() = Eigen::Vector3d(d.x, d.y, d.z);
            t.linear() = (Eigen::AngleAxisd(d.yaw, Eigen::Vector3d::UnitZ()) *
                Eigen::AngleAxisd(d.pitch, Eigen::Vector3d::UnitY()) *
                Eigen::AngleAxisd(d.roll, Eigen::Vector3d::UnitX())).toRotationMatrix();
            return t;
        };
        simulation_project::TransformDesc tool;
        tool.x = -.000737; tool.y = -.000759; tool.z = 1.2198;
        tool.roll = -pi / 6.; tool.pitch = -pi / 2.; tool.yaw = pi;
        for(std::size_t pose = 0; pose < encoders.size(); ++pose) {
            require(scene.setRobotJointValuesByDof(encoders[pose]), "Apply native FANUC telemetry");
            for(std::size_t axis = 0; axis < 6; ++axis) {
                double actual = 0.;
                const double expected = (encoders[pose][axis] + (axis == 2 ? encoders[pose][1] : 0.)) * pi / 180.;
                require(scene.robotJointValue("fanuc", "J" + std::to_string(axis + 1), actual) &&
                    std::abs(actual - expected) < 1.e-10, "Named axis mapping, serial J3 coupling and full turns");
            }
            if(pose >= measured.size()) continue;
            simulation_project::TransformDesc flange, world;
            require(scene.robotLinkTransform("fanuc", "fanuc_flange", flange) &&
                scene.robotLinkTransform("fanuc", "wbase", world), "Resolve model flange and FANUC WORLD");
            const Eigen::Isometry3d tcp = transform(world).inverse() * transform(flange) * transform(tool);
            simulation_project::TransformDesc observed;
            observed.x = measured[pose][0] * .001; observed.y = measured[pose][1] * .001;
            observed.z = measured[pose][2] * .001; observed.roll = measured[pose][3] * pi / 180.;
            observed.pitch = measured[pose][4] * pi / 180.; observed.yaw = measured[pose][5] * pi / 180.;
            const double mm = (tcp.translation() - transform(observed).translation()).norm() * 1000.;
            const double deg = Eigen::AngleAxisd(tcp.linear().transpose() * transform(observed).linear()).angle() * 180. / pi;
            std::cout << "FANUC pose " << pose << " WORLD TCP error mm=" << mm << " deg=" << deg << '\n';
            require(mm < .01 && deg < .001, "Scene FK matches independent ROBOGUIDE WORLD TCP observation");
        }
        auto bad = encoders.back(); bad[5] = std::numeric_limits<double>::quiet_NaN();
        require(!scene.setRobotJointValuesByDof(bad), "Reject nonfinite native telemetry atomically");
        double last = 0.; scene.robotJointValue("fanuc", "J3", last);
        require(std::abs(last - (-5. * pi / 180.)) < 1.e-10, "Invalid group preserves previous coupled pose");
        // A different robot with identical axis labels must not inherit FANUC coupling.
        QString generic = stripped;
        generic.replace(QStringLiteral("name=\"m710ic_50\""), QStringLiteral("name=\"custom_six_axis\""));
        const auto genericPath = folder / "generic_telemetry.urdf";
        { std::ofstream out(genericPath); out << generic.toStdString(); }
        document.robots[0].sourcePath = genericPath.generic_string();
        ProjectScene genericScene;
        genericScene.setProjectDocument(document, folder);
        require(genericScene.initialize() && genericScene.setRobotJointValuesByDof(encoders[0]), "Generic six-axis named mapping still works");
        genericScene.robotJointValue("fanuc", "J3", last);
        require(std::abs(last - encoders[0][2] * pi / 180.) < 1.e-10, "No FANUC coupling on unrelated six-axis models");
    }

    void verifyGeometry(const std::filesystem::path& folder)
    {
        simulation_project::ProjectDocument document;
        document.collision.query.enabled = false;
        simulation_project::RobotDesc gun;
        gun.id = "gun";
        gun.sourceType = "urdf";
        gun.sourcePath = (folder / "gun.urdf").generic_string();
        gun.collisionEnabled = false;
        document.robots.push_back(gun);
        simulation_project::RobotDesc target;
        target.id = "burnner";
        target.sourceType = "urdf";
        target.sourcePath = (folder / "target.urdf").generic_string();
        target.baseTransform.z = 0.3;
        target.collisionEnabled = false;
        document.robots.push_back(target);
        target.id = "flipped";
        target.sourcePath = (folder / "flipped.urdf").generic_string();
        document.robots.push_back(target);

        ProjectScene scene;
        scene.setProjectDocument(document, folder);
        require(scene.initialize(), "Synthetic scene initializes with collision disabled");
        auto value = scene.sprayMeasurement("gun", "burnner");
        require(value.valid && std::abs(value.distanceMeters - 0.3) < 1.0e-6,
            "Nearest forward STL intersection is 0.3 m");
        require(value.valid && std::abs(value.angleDegrees) < 1.0e-6,
            "Normal incidence is zero degrees");
        require(value.poseValid && (value.hitPosition - value.tcpPosition - value.distanceMeters * value.sprayDirection).norm() < 1e-10,
            "Recorded world-space geometry agrees with the measured ray distance");
        scene.setSprayDirectionNegativeZ(true);
        auto reverse = scene.sprayMeasurement("gun", "burnner");
        require(!reverse.valid && reverse.poseValid && reverse.sprayDirection.z() < -.999,
            "Z- does not silently fall back to the forward Z+ hit");
        simulation_project::TransformDesc above;
        above.z = .8;
        scene.setRobotBaseTransform("gun", above);
        reverse = scene.sprayMeasurement("gun", "burnner");
        require(reverse.valid && std::abs(reverse.distanceMeters - .2) < 1e-6 && std::abs(reverse.hitPosition.z() - .6) < 1e-6,
            "Z- finds the nearest surface behind local +Z and returns its world hit");
        scene.setRobotJointValue("gun", "tilt", 3.141592653589793 / 6.);
        reverse = scene.sprayMeasurement("gun", "burnner");
        require(reverse.valid && std::abs(reverse.distanceMeters - .2 / std::cos(3.141592653589793 / 6.)) < 1e-6 &&
            std::abs(reverse.angleDegrees + 30.) < 1e-6 && reverse.surfaceNormal.dot(reverse.sprayDirection) <= 0.,
            "Z- tilt preserves signed incidence and nozzle-facing normal");
        const auto reversedWinding = scene.sprayMeasurement("gun", "flipped");
        require(reversedWinding.valid && std::abs(reversedWinding.angleDegrees - reverse.angleDegrees) < 1e-6,
            "Z- incidence is independent of STL winding");
        scene.setRobotJointValue("gun", "tilt", 0.);
        scene.setRobotBaseTransform("gun", simulation_project::TransformDesc{});
        scene.setSprayDirectionNegativeZ(false);
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 0, "Trace is disabled by default");
        {
            Eigen::Isometry3d explicitTcp = Eigen::Isometry3d::Identity();
            explicitTcp.translation().z() = .1;
            const auto direct = scene.sprayMeasurementAtPose(explicitTcp, "burnner", false);
            require(direct.valid && std::abs(direct.distanceMeters - .2) < 1.e-6,
                "Explicit TCP pose uses the same nearest surface kernel without moving the robot");
            explicitTcp.translation().z() = .4;
            const auto reverse = scene.sprayMeasurementAtPose(explicitTcp, "burnner", true);
            require(reverse.valid && std::abs(reverse.distanceMeters - .1) < 1.e-6,
                "Explicit TCP Z- measures the reverse ray independently of the playback direction");
            explicitTcp.linear()(0, 0) = 2.;
            require(!scene.sprayMeasurementAtPose(explicitTcp, "burnner", false).poseValid,
                "Explicit measurement rejects a nonrigid TCP pose");
        }
        scene.setEndEffectorTraceVisible("gun", true);
        scene.appendEndEffectorTraceSample();
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 1, "Stationary TCP samples are deduplicated");
        scene.setRobotJointValue("gun", "slide", 0.2);
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 2, "FK motion appends a trace point without cone or measurement");
        scene.setEndEffectorTraceVisible("gun", false);
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 0, "Disabling trace clears and stops sampling");
        scene.setEndEffectorTraceVisible("gun", true);
        scene.appendEndEffectorTraceSample();
        scene.setEndEffectorTraceVisible("missing", true);
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 0, "Robot change and missing TCP cannot connect stale trace");
        scene.setEndEffectorTraceVisible("gun", true);
        scene.appendEndEffectorTraceSample();
        scene.clearEndEffectorTrace();
        require(scene.endEffectorTracePointCount() == 0, "Playback reset clears trace independently");
        std::string matchedRobot;
        require(scene.setRobotJointValuesByDof({200., 30.}, &matchedRobot) && matchedRobot == "gun",
            "Telemetry matches the complete movable DOF group");
        double mappedSlide = 0., mappedTilt = 0.;
        require(scene.robotJointValue("gun", "slide", mappedSlide) && scene.robotJointValue("gun", "tilt", mappedTilt) &&
            std::abs(mappedSlide - .2) < 1e-12 && std::abs(mappedTilt - 3.141592653589793 / 6.) < 1e-12,
            "Telemetry converts controller millimetres/degrees to scene metres/radians");
        require(!scene.setRobotJointValuesByDof({300., std::numeric_limits<double>::quiet_NaN()}) &&
            !scene.setRobotJointValuesByDof({1., 2., 3.}) &&
            scene.robotJointValue("gun", "slide", mappedSlide) && std::abs(mappedSlide - .2) < 1e-12,
            "Invalid or mismatched telemetry never partially changes the robot");
        scene.appendTelemetryEndEffectorTraceSample("gun");
        scene.appendTelemetryEndEffectorTraceSample("gun");
        require(scene.telemetryEndEffectorTracePointCount() == 1, "Telemetry trace deduplicates stationary samples");
        scene.setTelemetryEndEffectorTraceVisible(false);
        scene.setRobotJointValuesByDof({300., 30.});
        scene.appendTelemetryEndEffectorTraceSample("gun");
        scene.setTelemetryEndEffectorTraceVisible(true);
        require(scene.telemetryEndEffectorTracePointCount() == 2 && scene.endEffectorTracePointCount() == 0,
            "Hidden telemetry trace keeps collecting independently of planning trace");
        scene.clearEndEffectorTrace();
        require(scene.telemetryEndEffectorTracePointCount() == 2, "Planning trace reset preserves digital twin trace");
        scene.clearTelemetryEndEffectorTrace();
        scene.setRobotJointValue("gun", "tilt", 0.);
        scene.setRobotJointValue("gun", "slide", 0.0);
        scene.setSprayRangeVisible("gun", true);
        const auto visible = scene.sprayMeasurement("gun", "burnner");
        require(visible.valid && visible.distanceMeters == value.distanceMeters,
            "Measurement is independent of cone visibility");
        constexpr double pi = 3.14159265358979323846;
        scene.setRobotJointValue("gun", "tilt", pi / 6.0);
        value = scene.sprayMeasurement("gun", "burnner");
        require(value.valid && std::abs(value.distanceMeters - 0.3 / std::cos(pi / 6.0)) < 1.0e-6,
            "FK tilt updates ray distance immediately");
        require(value.valid && std::abs(value.angleDegrees + 30.0) < 1.0e-6,
            "Local-X sign gives -30 degrees for positive gun tilt");
        const auto flipped = scene.sprayMeasurement("gun", "flipped");
        require(flipped.valid && std::abs(flipped.angleDegrees - value.angleDegrees) < 1.0e-6,
            "Reversed STL winding preserves signed angle");
        scene.setRobotJointValue("gun", "tilt", -pi / 6.0);
        value = scene.sprayMeasurement("gun", "burnner");
        require(value.valid && std::abs(value.angleDegrees - 30.0) < 1.0e-6,
            "Opposite tilt gives +30 degrees");
        scene.setRobotJointValue("gun", "tilt", 0.0);
        simulation_project::TransformDesc gunPose;
        gunPose.z = 0.1;
        scene.setRobotBaseTransform("gun", gunPose);
        value = scene.sprayMeasurement("gun", "burnner");
        require(value.valid && std::abs(value.distanceMeters - 0.2) < 1.0e-6,
            "Nozzle translation changes distance to 0.2 m");
        simulation_project::TransformDesc targetPose;
        targetPose.z = 0.4;
        scene.setRobotBaseTransform("burnner", targetPose);
        value = scene.sprayMeasurement("gun", "burnner");
        require(value.valid && std::abs(value.distanceMeters - 0.3) < 1.0e-6,
            "Target transform updates despite cached local mesh");
        scene.setRobotJointValue("gun", "tilt", pi);
        require(!scene.sprayMeasurement("gun", "burnner").valid, "Backward-only intersections are invalid");
        scene.setRobotJointValue("gun", "tilt", pi / 2.0);
        require(!scene.sprayMeasurement("gun", "burnner").valid, "Parallel ray is invalid");
        require(!scene.sprayMeasurement("missing").valid, "Missing nozzle is invalid");
        require(!scene.sprayMeasurement("gun", "missing").valid, "Missing target is invalid");
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 1, "Trace still samples when spray ray misses target");
        scene.setProjectDocument(document, folder);
        require(scene.endEffectorTracePointCount() == 0, "Project reload clears trace");
        require(scene.telemetryEndEffectorTracePointCount() == 0, "Project reload clears telemetry trace");
        auto duplicate = gun;
        duplicate.id = "another_gun";
        document.robots.push_back(duplicate);
        ProjectScene ambiguousScene;
        ambiguousScene.setProjectDocument(document, folder);
        require(ambiguousScene.initialize() && !ambiguousScene.setRobotJointValuesByDof({200., 30.}),
            "Multiple robots with the same DOF do not cause an arbitrary telemetry assignment");
    }

    void verifyAWorkpiece(const std::filesystem::path& folder, const std::filesystem::path& projectPath)
    {
        using namespace simulation_project;
        using namespace motion_planning;
        ProjectDocument document;
        RobotDesc gun;
        gun.id="ABB4600_urdf"; gun.name="ABB4600"; gun.sourceType="urdf";
        gun.sourcePath=(folder/"gun.urdf").generic_string();
        document.robots.push_back(gun);
        RobotDesc decoy; decoy.id="burnner"; decoy.name="burnner"; decoy.sourceType="urdf";
        decoy.sourcePath=(folder/"target.urdf").generic_string();
        decoy.baseTransform.z=0.1;
        document.robots.push_back(decoy);
        SceneObjectDesc workpiece;
        workpiece.id="A"; workpiece.name="A.STL"; workpiece.objectType="workpiece";
        workpiece.sourcePath=(folder/"surface.stl").generic_string();
        workpiece.visualScale=workpiece.collisionScale=0.5;
        workpiece.transform.z=0.6;
        document.objects.push_back(workpiece);
        ProjectScene scene;
        scene.setProjectDocument(document,folder);
        require(scene.initialize(),"A.STL workpiece initializes alongside a burnner URDF decoy");
        auto measured=scene.sprayMeasurement(gun.id);
        require(measured.valid && std::abs(measured.distanceMeters-0.6)<1e-6 && std::abs(measured.angleDegrees)<1e-6,
            "Default spray distance/angle use the STL object, ignoring the closer URDF");
        auto pose=workpiece.transform; pose.z=0.8; pose.roll=3.141592653589793/6.0;
        require(scene.setSceneObjectTransform("A",pose),"Workpiece can move after mesh caching");
        measured=scene.sprayMeasurement(gun.id);
        require(measured.valid && std::abs(measured.distanceMeters-0.8)<1e-6 && std::abs(std::abs(measured.angleDegrees)-30.0)<1e-5,
            "Cached STL follows the workpiece's current translation and rotation");
        ProjectCdfQpRepairOptions options;
        std::string error, detector;
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,gun.id,options,&detector),
            "Default CDF detector configures ABB4600 versus A.STL");
        const auto& configured=document.collision.detectors.back();
        require(configured.type=="RobotObject" && configured.pairGenerators.size()==1 &&
            configured.pairGenerators.front().robotId==gun.id && configured.pairGenerators.front().objectId=="A",
            "Collision query contains exactly the robot/A workpiece pair");
        // A closer unrelated object must not change the dedicated distance result.
        auto unrelated=workpiece; unrelated.id="closer_unrelated"; unrelated.name="Unrelated"; unrelated.transform.z=0.1;
        document.objects.push_back(unrelated);
        ProjectPlanningRequest request; request.robotId=gun.id; request.jointNames={"tilt","slide"};
        request.start=request.goal={0.0,0.0}; request.collisionDetectorIds={detector};
        auto collisionScene=ProjectPlanningSceneBuilder::build(document,folder,request,&error);
        require(bool(collisionScene),"Dedicated nearest-distance snapshot builds");
        if(collisionScene) {
            const auto sample=collisionScene->nearestDistance({0.0,0.0});
            require(sample.valid && std::abs(sample.distanceMeters-0.59)<1e-5 && sample.detectorId==detector,
                "Nearest distance is robot/A clearance, excluding closer unrelated geometry");
        }
        auto absent=document; absent.objects.clear();
        scene.setProjectDocument(absent,folder);
        require(scene.initialize() && !scene.sprayMeasurement(gun.id).valid,
            "Missing A.STL reports invalid instead of measuring burnner");
        for(bool keepA : {false,true}) {
            auto saved=document;
            ProjectDocumentService service(saved);
            ProjectReferenceCleanupReport cleanup;
            if(!keepA) {
                require(service.removeObject("A",cleanup,&error) && service.removeObject("closer_unrelated",cleanup,&error),
                    "Scene commands remove workpieces and clean their detector references");
            }
            require(service.removeRobot("burnner",cleanup,&error),"Scene command deletes the burnner robot");
            const auto path=folder/(keepA ? "deleted-with-A.sys.json" : "deleted-no-A.sys.json");
            const bool saveOk=saveProjectDocumentV3(path,service.document(),&error);
            require(saveOk,"Deleted scene saves as a new system project");
            if(!saveOk) { std::cerr << "Deletion fixture save: " << error << '\n'; continue; }
            ProjectDocument reopened;
            require(loadProjectDocument(path,reopened,&error),"Saved scene reopens");
            robot_qt_viewer::RobotQtViewerEventHub hub;
            ProjectSession session; session.setDocument(reopened,path,false,false);
            robot_qt_viewer::RobotQtViewerDocumentController documents(session,hub);
            robot_qt_viewer::RobotQtViewerSelectionModel selection(hub);
            robot_qt_viewer::RobotQtViewerViewportPreviewState preview(hub);
            robot_qt_viewer::RobotQtViewerOperationStatusStore status(hub);
            robot_qt_viewer::RobotQtViewerDocumentContext context(session,documents,selection,preview,hub,status);
            MotionPlanningEditorWidget widget;
            robot_qt_viewer::MotionPlanningModuleController controller(widget,context);
            robot_qt_viewer::RobotQtViewerEvent opened;
            opened.kind=robot_qt_viewer::RobotQtViewerEventKind::ProjectOpened;
            controller.handleEvent(opened);
            require(context.document().robots.size()==1 && context.document().robots.front().id==gun.id,
                "Opening Motion Planning and reloading never resurrect the deleted URDF");
            require(saveProjectDocumentV3(path,context.document(),&error) && loadProjectDocument(path,reopened,&error) && reopened.robots.size()==1,
                "Deletion remains stable after another save/reopen");
        }
        auto missing=document; missing.objects.clear(); missing.robots.resize(1);
        missing.collision.query.enabled=false;
        require(!ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(missing,gun.id,options) &&
            missing.robots.size()==1 && missing.objects.empty() && !missing.collision.query.enabled,
            "Missing collision target fails without inserting entities or enabling queries");
        ProjectDocument actual;
        require(loadProjectDocument(projectPath,actual,&error),"Requested default ABB4600-A scene loads");
        require(actual.robots.size()==1 && actual.robots.front().id=="ABB4600_urdf" &&
            actual.objects.size()==1 && actual.objects.front().id=="A",
            "Requested scene contains ABB4600 and A.STL, without burnner");
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(actual,"ABB4600_urdf",options),
            "Requested default scene accepts the robot/A collision setup");
        ProjectScene actualScene; actualScene.setProjectDocument(actual,projectPath.parent_path());
        require(actualScene.initialize(),"Actual ABB4600 URDF and scaled A.STL initialize successfully");
    }

    class CollisionConfigTestServices final : public robot_qt_viewer::CollisionWorkbenchServices
    {
    public:
        explicit CollisionConfigTestServices(robot_qt_viewer::RobotQtViewerDocumentContext& context) : context(context) {}
        const simulation_project::ProjectDocument& document() const override { return context.document(); }
        const simulation_project::ProjectSession& session() const override { return context.projectSession(); }
        robot_qt_viewer::ProjectMutationResult mutateProject(const QString& source,
            robot_qt_viewer::ProjectDirtyPolicy policy, const robot_qt_viewer::ProjectMutation& mutation) override
        { return context.documentController().mutateProject(source, policy, mutation); }
        const QString& selectedRobotId() const override { return empty; }
        const QString& selectedLinkName() const override { return empty; }
        const QString& selectedObjectId() const override { return empty; }
        const QString& selectedToolAttachmentId() const override { return empty; }
        const QString& collisionPairRobotA() const override { return empty; }
        const QString& collisionPairLinkA() const override { return empty; }
        void setActiveCollisionDetectorContext(const QString&) override {}
        void setMarkedCollisionPairAContext(const QString&, const QString&) override {}
        robot_qt_viewer::ProjectSessionWorkflowResult saveProject(const std::filesystem::path&, bool, const QString&) override { return {}; }
        void reloadViewport(const QString&) override {}
        bool refreshViewportCollisionConfiguration(const QString&) override { return false; }
        bool saveRobotCollisionOverride(const std::filesystem::path&, const simulation_project::RobotCollisionOverrideDesc&, std::string*) override { return false; }
        bool exportRobotCollisionOverrideToUrdf(const std::filesystem::path&, const std::filesystem::path&,
            const simulation_project::RobotCollisionOverrideDesc&, std::string*) override { return false; }
    private:
        robot_qt_viewer::RobotQtViewerDocumentContext& context;
        QString empty;
    };

    void verifyCollisionConfigNearest(QApplication& application, const std::filesystem::path& folder)
    {
        using namespace simulation_project;
        using namespace robot_qt_viewer;
        {
            ProjectDocument draft;
            CollisionDetectorDesc scoped; scoped.id="draft"; scoped.type="SceneAll"; scoped.queryPolicy="AllEnabled";
            CollisionPairGeneratorDesc all; all.type="SceneAll"; scoped.pairGenerators.push_back(all);
            draft.collision.detectors.push_back(scoped);
            CollisionDetectorDraftMemberView link; link.robotId="gun"; link.linkName="Link6";
            CollisionDetectorDraftMemberView target; target.objectId="A";
            auto bound=CollisionDetectorCommandController::bindDetectorPairGenerators(draft,nullptr,"draft",{link},{target});
            require(bound.success && draft.collision.detectors.front().pairGenerators.size()==1 &&
                draft.collision.detectors.front().pairGenerators.front().type=="LinkObject",
                "Binding explicit pairs replaces the default SceneAll fallback");
            auto secondLink=link; secondLink.linkName="carriage";
            bound=CollisionDetectorCommandController::bindDetectorPairGenerators(draft,nullptr,"draft",{secondLink},{target});
            require(bound.success && draft.collision.detectors.front().pairGenerators.size()==2,
                "Adding explicit pairs preserves previously selected pairs");
            draft.collision.detectors.front().pairGenerators.insert(draft.collision.detectors.front().pairGenerators.begin(),all);
            bound=CollisionDetectorCommandController::bindDetectorPairGenerators(draft,nullptr,"draft",{link},{target});
            require(bound.success && draft.collision.detectors.front().pairGenerators.size()==2 &&
                std::none_of(draft.collision.detectors.front().pairGenerators.begin(),draft.collision.detectors.front().pairGenerators.end(),
                    [](const auto& rule) { return rule.type=="SceneAll"; }),
                "Rebinding existing pairs repairs a legacy mixed SceneAll detector without duplicating pairs");
        }
        ProjectDocument document;
        RobotDesc gun; gun.id="gun"; gun.name="gun"; gun.sourceType="urdf";
        gun.sourcePath=(folder/"gun.urdf").generic_string(); document.robots.push_back(gun);
        SceneObjectDesc workpiece; workpiece.id="A"; workpiece.name="A.STL";
        workpiece.sourcePath=(folder/"surface.stl").generic_string(); workpiece.transform.z=0.6;
        document.objects.push_back(workpiece);
        motion_planning::ProjectCdfQpRepairOptions options;
        std::string detectorId;
        require(motion_planning::ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,"gun",options,&detectorId),
            "Configuration fixture creates robot/A detector");
        auto& detector=document.collision.detectors.back();
        detector.visualization.visible=false;
        detector.visualization.showNearestPoints=false;
        auto disabled=detector; disabled.id="disabled"; disabled.enabled=false;
        document.collision.detectors.push_back(disabled);
        ProjectSession session; session.setDocument(document,folder/"config.sys.json",false,false);
        RobotQtViewerEventHub hub;
        RobotQtViewerDocumentController documents(session,hub);
        RobotQtViewerSelectionModel selection(hub);
        RobotQtViewerViewportPreviewState preview(hub);
        RobotQtViewerOperationStatusStore status(hub);
        RobotQtViewerDocumentContext context(session,documents,selection,preview,hub,status);
        RobotViewport viewport; viewport.resize(640,480);
        viewport.loadProjectDocument(document,folder); viewport.show(); application.processEvents();
        RobotQtViewerViewportProjectState projectState;
        RobotQtViewerCollisionViewportAdapter collisionViewport(viewport,projectState);
        context.setCollisionViewport(&collisionViewport);
        CollisionConfigTestServices services(context);
        CollisionWorkbenchPanel panel;
        CollisionWorkbenchModuleController controller(panel,context,services);
        controller.refreshInspector({}); panel.selectDetector(QString::fromStdString(detectorId));
        controller.setWorkbenchActive(true);
        auto* result=panel.findChild<QLabel*>(QStringLiteral("collisionConfigNearestResult"));
        require(result && result->text().contains("paused"),"Collision configuration explains paused queries without displaying an old distance");
        require(viewport.setCollisionQueriesEnabled(true),"Starting collision detection builds the lazy collision runtime");
        auto waitFor=[&](const QString& text) {
            QElapsedTimer timer; timer.start();
            while(result && !result->text().contains(text) && timer.elapsed()<5000) {
                application.processEvents(QEventLoop::AllEvents,20);
            }
            return result && result->text().contains(text);
        };
        require(waitFor("590.000 mm"),"Starting detection automatically displays nearest distance even with the overlay hidden");
        require(viewport.refreshCollisionConfiguration(document,folder) && viewport.collisionQueriesEnabled(),
            "Editing collision configuration preserves running detection");
        const auto refreshed=viewport.collisionDetectors();
        const auto refreshedIt=std::find_if(refreshed.begin(),refreshed.end(),[&](const auto& item) { return item.id==detectorId; });
        require(refreshedIt!=refreshed.end() && refreshedIt->valid && refreshedIt->includePairCount>0,
            "Live configuration refresh rebuilds collision geometry and pair endpoints");
        application.processEvents();
        require(waitFor("590.000 mm"),"Live configuration refresh continues to display the measured distance");
        auto pose=workpiece.transform; pose.z=0.8;
        require(viewport.setSceneObjectTransform("A",pose) && waitFor("790.000 mm"),
            "Configuration panel automatically follows moved workpiece geometry");
        pose.z=0.0;
        require(viewport.setSceneObjectTransform("A",pose) && waitFor("Nearest distance: 0.000 mm") &&
            result->text().contains("Collision: Yes"),
            "Contact reports zero clearance even when the backend suppresses nearest-point coordinates");
        pose.z=0.8;
        require(viewport.setSceneObjectTransform("A",pose) && waitFor("790.000 mm"),
            "Separating the workpiece replaces collision clearance with the measured distance");
        panel.selectDetector(QStringLiteral("disabled"));
        require(waitFor("detector disabled"),"Selecting a disabled detector clears the previous distance");
        panel.selectDetector(QString::fromStdString(detectorId));
        require(waitFor("790.000 mm"),"Switching back restores the selected detector's current distance");
        viewport.setCollisionQueriesEnabled(false);
        require(waitFor("paused") && !result->text().contains("790.000"),"Pausing detection clears stale distance in configuration panel");
        controller.setWorkbenchActive(false);
        viewport.close();
    }

    void verifyImportedCollisionModels(QApplication& application, const std::filesystem::path& folder,
        const std::filesystem::path& robotPath, const std::filesystem::path& objectPath)
    {
        using namespace simulation_project;
        using namespace robot_qt_viewer;
        for(const double scale : {1.0,0.001}) {
            ProjectSession session; session.setDocument(ProjectDocument{},folder/"new-project.sys.json",false,false);
            RobotQtViewerEventHub hub;
            RobotQtViewerDocumentController documents(session,hub);
            RobotQtViewerSelectionModel selection(hub);
            RobotQtViewerViewportPreviewState preview(hub);
            RobotQtViewerOperationStatusStore status(hub);
            RobotQtViewerDocumentContext context(session,documents,selection,preview,hub,status);
            SceneEntityWorkflowController importer(context);
            const auto robot=importer.importRobotFromPath(robotPath,"urdf");
            const auto object=importer.importSceneObjectFromPath(objectPath,"workpiece",scale);
            require(robot.success && object.success,"New project imports the supplied URDF and Unicode STL through scene commands");
            if(!robot.success || !object.success) {
                std::cerr << robot.message.toStdString() << ' ' << object.message.toStdString() << '\n';
                continue;
            }
            auto document=context.document();
            CollisionSelectionSetDesc robots; robots.id="robot-set";
            CollisionSelectionSetMemberDesc robotMember; robotMember.robotId=robot.entityId.toStdString();
            robots.members.push_back(robotMember);
            CollisionSelectionSetDesc objects; objects.id="object-set";
            CollisionSelectionSetMemberDesc objectMember; objectMember.objectId=object.entityId.toStdString();
            objects.members.push_back(objectMember);
            document.collision.selectionSets={robots,objects};
            CollisionDetectorDesc detector; detector.id="new-project-pair"; detector.name="ABB / imported STL";
            detector.queryPolicy="BetweenSets"; detector.selectionSetAId=robots.id; detector.selectionSetBId=objects.id;
            detector.visualization.visible=false; detector.visualization.showNearestPoints=false;
            document.collision.detectors={detector}; document.collision.query.enabled=true;
            document.objects.front().transform.x=5.0;
            session.setDocument(document,folder/"new-project.sys.json",false,false);
            RobotViewport viewport; viewport.resize(640,480);
            viewport.loadProjectDocument(document,folder); viewport.show(); application.processEvents();
            RobotQtViewerViewportProjectState projectState;
            RobotQtViewerCollisionViewportAdapter collisionViewport(viewport,projectState);
            context.setCollisionViewport(&collisionViewport);
            CollisionConfigTestServices services(context);
            CollisionWorkbenchPanel panel;
            CollisionWorkbenchModuleController controller(panel,context,services);
            controller.refreshInspector({}); panel.selectDetector(QString::fromStdString(detector.id));
            controller.setWorkbenchActive(true);
            require(viewport.setCollisionQueriesEnabled(true),"New project starts collision queries");
            auto* label=panel.findChild<QLabel*>(QStringLiteral("collisionConfigNearestResult"));
            QElapsedTimer timer; timer.start();
            while(label && !label->text().contains("Collision:") && timer.elapsed()<15000)
                application.processEvents(QEventLoop::AllEvents,20);
            const auto results=viewport.collisionDetectors();
            require(results.size()==1 && results.front().valid && results.front().hasResult &&
                results.front().effectiveIncludePairCount>0 && results.front().nearest.valid &&
                results.front().nearest.distance>0 && label && label->text().contains("Collision: No"),
                "Actual ABB and imported STL return positive clearance and clear status in configuration panel");
            if(label) std::cout << "Import scale=" << scale << "\n" << label->text().toStdString() << '\n';
            if(!results.empty()) std::cout << "Configured pairs=" << results.front().includePairCount
                << " effective pairs=" << results.front().effectiveIncludePairCount
                << " nearest state=" << results.front().nearest.state
                << " error=" << results.front().errorMessage << '\n';
            controller.setWorkbenchActive(false); viewport.close();
        }
    }

    void inspectProjectCollision(QApplication& application, const std::filesystem::path& projectPath,
        const std::string& detectorId)
    {
        using namespace simulation_project;
        ProjectDocument original;
        std::string error;
        require(loadProjectDocument(projectPath,original,&error),"Saved collision project loads");
        for(const bool selectedOnly : {false,true}) {
            auto document=original;
            if(selectedOnly) for(auto& detector : document.collision.detectors) {
                if(detector.id==detectorId) {
                    detector.pairGenerators.erase(std::remove_if(detector.pairGenerators.begin(),detector.pairGenerators.end(),
                        [](const auto& generator) { return generator.type=="SceneAll"; }),detector.pairGenerators.end());
                }
            }
            ProjectScene scene; scene.setProjectDocument(document,projectPath.parent_path());
            require(scene.initialize(),"Saved collision project initializes");
            require(scene.setActiveCollisionDetector(detectorId),"Requested detector exists");
            require(scene.setCollisionQueriesEnabled(true),"Saved collision project starts queries");
            scene.update(0.0);
            const auto results=scene.collisionDetectors();
            for(const auto& result : results) {
                if(result.id!=detectorId) continue;
                std::cout << "INSPECT selectedOnly=" << selectedOnly << " detector=" << result.id
                    << " valid=" << result.valid << " hasResult=" << result.hasResult
                    << " pairs=" << result.includePairCount << " effective=" << result.effectiveIncludePairCount
                    << " collision=" << result.inCollision << " contacts=" << result.contactCount
                    << " nearest=" << result.nearest.valid << " distance=" << result.nearest.distance
                    << " state=" << result.nearest.state << " reason=" << result.nearest.reason
                    << " error=" << result.errorMessage << '\n';
                if(selectedOnly) require(result.valid && result.hasResult && result.effectiveIncludePairCount==7 &&
                    (result.nearest.valid || result.inCollision),"Restricted saved detector returns a valid distance or collision");
            }
        }
    }

    void verifyPlot(QApplication& application)
    {
        MotionPlanningEditorWidget widget;
        auto* exportButton = widget.findChild<QPushButton*>(QStringLiteral("exportSprayMeasurements"));
        auto* plotButton = widget.findChild<QPushButton*>(QStringLiteral("plotSprayMeasurements"));
        require(exportButton && plotButton, "Both spray action buttons exist");
        if(!exportButton || !plotButton) { return; }
        widget.setSprayRecordingState(false, false, false);
        require(!plotButton->isEnabled() && exportButton->isEnabled(), "Empty recording permits arming export only");
        widget.setSprayRecordingState(true, true, true);
        require(!plotButton->isEnabled() && !exportButton->isEnabled(), "Pending playback action states");
        widget.setSprayRecordingState(true, false, false);
        require(plotButton->isEnabled() && exportButton->isEnabled(), "Completed recording action states");
        QVector<SprayMeasurementPlotSample> samples;
        for(int i = 0; i < 5; ++i) {
            SprayMeasurementPlotSample sample;
            sample.pointIndex = 10 + i * 2;
            sample.timeSeconds = i * .25;
            sample.poseValid = true;
            sample.valid = i != 2;
            sample.distanceMm = 200 + i * 25;
            sample.angleDegrees = (i - 2) * 15;
            sample.tcpPosition = Eigen::Vector3d(i * .05, .01 * i * i, 0.);
            sample.sprayDirection = Eigen::Vector3d::UnitZ();
            sample.hitPosition = sample.tcpPosition + sample.sprayDirection * sample.distanceMm * .001;
            sample.surfaceNormal = Eigen::AngleAxisd(sample.angleDegrees * 3.141592653589793 / 180.,
                Eigen::Vector3d::UnitX()) * -Eigen::Vector3d::UnitZ();
            sample.errorMessage = QStringLiteral("No forward intersection with fixture");
            samples.push_back(sample);
        }
        widget.showSprayMeasurementPlot(samples, QStringLiteral("Spray regression - TCP Z+"));
        auto* dialog = widget.findChild<QDialog*>();
        require(dialog != nullptr, "Independent plot dialog exists");
        if(!dialog) { return; }
        auto* index = dialog->findChild<QSpinBox*>(QStringLiteral("sprayPlotSampleIndex"));
        auto* info = dialog->findChild<QLabel*>(QStringLiteral("sprayPlotSelectedInfo"));
        auto* distanceCurve = dialog->findChild<QWidget*>(QStringLiteral("sprayDistanceCurve"));
        auto* angleCurve = dialog->findChild<QWidget*>(QStringLiteral("sprayAngleCurve"));
        auto* geometry = dialog->findChild<QWidget*>(QStringLiteral("sprayPlotGeometryView"));
        require(index && info && distanceCurve && angleCurve && geometry, "Interactive curves and geometry inspector exist");
        if(!index || !info || !distanceCurve || !angleCurve || !geometry) { return; }
        index->setValue(4);
        require(info->text().contains("#17") && info->text().contains("275.000000") && info->text().contains("15.000000"),
            "Selection maps sample to original trajectory identity and exact recorded metrics");
        index->setValue(3);
        require(info->text().contains("#15") && info->text().contains("No forward intersection"),
            "Invalid sample remains selectable without fabricating distance or angle");
        samples.clear();
        index->setValue(5);
        require(info->text().contains("#19"), "Dialog owns an immutable snapshot independent of caller lifetime");
        application.processEvents();
        const QPointF click(72, 75);
        QMouseEvent press(QEvent::MouseButtonPress, click, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(distanceCurve, &press);
        require(index->value() == 1 && info->text().contains("#11"), "Distance chart click synchronizes selection and inspector");
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QApplication::sendEvent(angleCurve, &right);
        require(index->value() == 2 && info->text().contains("#13"), "Angle chart keyboard navigation shares selection");
        for(const auto& size : { QSize(1160, 760), QSize(820, 650) }) {
            dialog->resize(size);
            application.processEvents();
            const QImage image = dialog->grab().toImage();
            int distancePixels = 0, anglePixels = 0;
            for(int y = 0; y < image.height(); ++y) {
                for(int x = 0; x < image.width(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if(color.green() > 100 && color.blue() > 100 && color.red() < 30) { ++distancePixels; }
                    if(color.red() > 150 && color.green() < 110 && color.blue() < 120) { ++anglePixels; }
                }
            }
            require(distancePixels > 20 && anglePixels > 20 && !geometry->grab().isNull(),
                "Curves and geometry preview render at tested window sizes");
            image.save(QStringLiteral("spray_plot_%1x%2.png").arg(size.width()).arg(size.height()));
        }
        dialog->close();
    }

    class SaveDialogAcceptor final : public QObject
    {
    public:
        QString path;
    protected:
        bool eventFilter(QObject* object, QEvent* event) override
        {
            if(event->type() == QEvent::Show) {
                if(auto* dialog = qobject_cast<QFileDialog*>(object)) {
                    QTimer::singleShot(0, dialog, [this, dialog]() {
                        dialog->selectFile(path);
                        static_cast<QDialog*>(dialog)->accept();
                    });
                }
            }
            return QObject::eventFilter(object, event);
        }
    };

    class PresentationObservingServices : public robot_qt_viewer::RobotQtViewerMotionPlanningViewportAdapter
    {
    public:
        using RobotQtViewerMotionPlanningViewportAdapter::RobotQtViewerMotionPlanningViewportAdapter;
        quint64 lastTicket = 0;
        int frameRequests = 0;
        int unpresentedOverwrites = 0;
        quint64 requestFramePresentation() override
        {
            if(lastTicket && !RobotQtViewerMotionPlanningViewportAdapter::isFramePresented(lastTicket)) { ++unpresentedOverwrites; }
            lastTicket = RobotQtViewerMotionPlanningViewportAdapter::requestFramePresentation();
            ++frameRequests;
            return lastTicket;
        }
    };

    class TraceObservingServices : public PresentationObservingServices
    {
    public:
        using PresentationObservingServices::PresentationObservingServices;
        int holdAtSamples = 0;
        bool isFramePresented(quint64 ticket) const override
        {
            return !(holdAtSamples > 0 && samples >= holdAtSamples) &&
                RobotQtViewerMotionPlanningViewportAdapter::isFramePresented(ticket);
        }
        int jointUpdates = 0;
        int samples = 0;
        int resets = 0;
        bool enabled = false;
        bool completeGroups = true;
        bool setRobotJointValues(const QString& robotId, const std::vector<std::string>& names,
            const std::vector<double>& values) override
        {
            const bool applied = RobotQtViewerMotionPlanningViewportAdapter::setRobotJointValues(robotId, names, values);
            if(applied) { jointUpdates += static_cast<int>(names.size()); }
            return applied;
        }
        void setEndEffectorTraceVisible(const QString& robotId, bool visible) override
        {
            enabled = visible;
            RobotQtViewerMotionPlanningViewportAdapter::setEndEffectorTraceVisible(robotId, visible);
        }
        void clearEndEffectorTrace() override
        {
            ++resets;
            RobotQtViewerMotionPlanningViewportAdapter::clearEndEffectorTrace();
        }
        void appendEndEffectorTraceSample() override
        {
            completeGroups = completeGroups && jointUpdates >= 2 && jointUpdates % 2 == 0;
            jointUpdates = 0;
            ++samples;
            RobotQtViewerMotionPlanningViewportAdapter::appendEndEffectorTraceSample();
        }
    };

    class OverlayObservingServices : public PresentationObservingServices
    {
    public:
        using PresentationObservingServices::PresentationObservingServices;
        std::function<void(const std::vector<double>&)> jointObserver;
        std::function<void(const std::vector<double>&)> presentationJointObserver;
        std::vector<double> lastAppliedJoints;
        quint64 requestFramePresentation() override {
            if(presentationJointObserver)presentationJointObserver(lastAppliedJoints);
            return PresentationObservingServices::requestFramePresentation();
        }
        bool setRobotJointValues(const QString& robotId, const std::vector<std::string>& names,
            const std::vector<double>& values) override
        {
            const bool applied = RobotQtViewerMotionPlanningViewportAdapter::setRobotJointValues(robotId, names, values);
            if(applied)lastAppliedJoints=values;
            if(applied && jointObserver) jointObserver(values);
            return applied;
        }
        int samples = 0;
        void appendEndEffectorTraceSample() override
        {
            ++samples;
            RobotQtViewerMotionPlanningViewportAdapter::appendEndEffectorTraceSample();
        }
        bool pointsVisible = true;
        std::size_t pointCount = 0;
        void setTrajectoryControlPointOverlay(const QString& id,
            const std::vector<simulation_project::TransformDesc>& points, bool showPoints, bool showOrientations = false) override
        {
            pointsVisible = showPoints;
            pointCount = points.size();
            RobotQtViewerMotionPlanningViewportAdapter::setTrajectoryControlPointOverlay(id, points, showPoints, showOrientations);
        }
        void clearTrajectoryControlPointOverlay(const QString& id = QString()) override
        {
            pointCount = 0;
            RobotQtViewerMotionPlanningViewportAdapter::clearTrajectoryControlPointOverlay(id);
        }
    };

    void verifyCdfAnalysisControls()
    {
        QVector<CdfStageViewData> stages(2);
        stages[0].name = QStringLiteral("Input");
        stages[1].name = QStringLiteral("Final"); stages[1].timingValid = true;
        CdfTrajectoryAnalysisDialog analysis(stages, {QStringLiteral("J1")}, {}, nullptr);
        auto* tabs = analysis.findChild<QTabWidget*>(QStringLiteral("cdfAnalysisTabs"));
        for(int index : {2, 3, 6}) {
            const auto checks = tabs->widget(index)->findChildren<QCheckBox*>();
            require(checks.size() == 2 && !checks[0]->isEnabled() && !checks[0]->isChecked() &&
                checks[1]->isEnabled() && checks[1]->isChecked(),
                "Invalid timestamps cannot contribute misleading velocity/acceleration curves");
        }
        MotionPlanningEditorWidget widget;
        auto* equivalent=widget.findChild<QCheckBox*>(QStringLiteral("cdfEquivalentConfigurations"));
        require(equivalent && widget.cdfQpRepairSettings().allowEquivalentConfigurations,"Equivalent endpoint pose option is visible");
        if(equivalent)equivalent->setChecked(false);
        require(!widget.cdfQpRepairSettings().allowEquivalentConfigurations,"Fixed joint configuration option remains selectable");
        widget.setCdfAnalysisStages({QStringLiteral("Input"), QStringLiteral("Final")});
        auto* play = widget.findChild<QPushButton*>(QStringLiteral("cdfStagePlay"));
        const auto idle = play->text();
        widget.setPlaybackActive(true);
        require(play->text() != idle && !widget.findChild<QComboBox*>(QStringLiteral("cdfStageSelection"))->isEnabled(),
            "Stage playback updates stop-button state and locks stage switching");
        widget.setPlaybackActive(false);
        require(play->text() == idle, "Stage playback restores its idle button state");
    }

    void verifyPlaybackTimeline()
    {
        using motion_planning::JointPlaybackTimeline;
        robottrajectory::JointTrajectory path;
        path.points = { {0.0, {0.0}, {}, {}}, {5.0, {0.01}, {}, {}}, {10.0, {1.0}, {}, {}} };
        JointPlaybackTimeline timeline;
        require(timeline.reset(path, 2.0, true) && std::abs(timeline.pointTime(1) - 0.02) < 1e-12 &&
            std::abs(timeline.sample(path, 1.0)[0] - 0.5) < 1e-12,
            "CDF preview speed is independent of uneven waypoint density");
        require(timeline.reset(path, 2.0, false) && std::abs(timeline.pointTime(1) - 1.0) < 1e-12 &&
            std::abs(timeline.sample(path, 0.5)[0] - 0.005) < 1e-12,
            "Ordinary trajectories retain relative source timing and interpolate between knots");
        path.points = { {0.0, {3.0}, {}, {}}, {1.0, {-3.0}, {}, {}} };
        require(timeline.reset(path, 2.0, true) && std::abs(timeline.sample(path, 1.0)[0]) < 1e-12,
            "Playback preserves full turn displacement without wrapping angles");
        path.points = { {0.0, {0.0}, {}, {}}, {0.0, {0.2}, {}, {}},
            {0.0, {0.2}, {}, {}}, {1.0, {1.0}, {}, {}} };
        require(timeline.reset(path, 2.0, false) && std::abs(timeline.sample(path, 0.4)[0] - 0.2) < 1e-12 &&
            timeline.sample(path, 5.0)[0] == 1.0, "Duplicate timestamps/poses remain finite and reach the exact endpoint");
        path.points.resize(1);
        require(timeline.reset(path, 2.0, true) && timeline.sample(path, 1.0) == path.points.front().q,
            "One-point preview remains stationary");
        path.points.front().q[0] = std::numeric_limits<double>::quiet_NaN();
        require(!timeline.reset(path, 2.0, true), "Invalid playback data is rejected before applying joints");
    }

    void verifyModelIk()
    {
        using namespace motion_planning;
        simulation_project::ProjectDocument document;
        simulation_project::RobotDesc robot;
        robot.id = "model";
        document.robots.push_back(robot);
        CartesianIkOptions options;
        options.robotId = robot.id;
        options.stepSize = 1.0;
        options.damping = 0.001;
        options.worldForwardKinematics = [](const std::vector<double>& q) {
            Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
            pose.translation() = Eigen::Vector3d(q[0], q[1], q[2]);
            pose.linear() = (Eigen::AngleAxisd(q[3], Eigen::Vector3d::UnitX()) *
                Eigen::AngleAxisd(q[4], Eigen::Vector3d::UnitY()) *
                Eigen::AngleAxisd(q[5], Eigen::Vector3d::UnitZ())).toRotationMatrix();
            return pose;
        };
        StoredMotionPlan plan;
        plan.robotId = robot.id;
        robottrajectory::TimedCartesianPoint point;
        point.tcpPose = options.worldForwardKinematics({ 0.2, -0.3, 0.4, 3.141592653589793, 0, 0 });
        plan.cartesianControlPoints.points.push_back(point);
        auto result = ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(document, plan, options);
        require(result.success, "Actual-model IK converges at a 180-degree orientation error");
        plan.cartesianControlPoints.points[0].tcpPose.linear() *= 1.0001;
        result = ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(document, plan, options);
        require(result.success, "Rounded non-orthogonal input rotations are projected onto SO(3)");
        plan.cartesianControlPoints.points[0].tcpPose.linear()(0, 0) = std::numeric_limits<double>::quiet_NaN();
        result = ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(document, plan, options);
        require(!result.success, "Non-finite IK target is rejected");
        require(ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues({ 1,2,3,4,5,6 }) ==
            std::vector<double>({ -1,2,3,-4,-5,-6 }), "Existing IRB4600 joint signs are preserved");
    }

    void verifyMultiIkDomain()
    {
        using namespace motion_planning;
        constexpr double pi = 3.14159265358979323846;
        CartesianMultiIkOptions options;
        options.model.jointNames = ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        options.model.seedJoints = {0.3, 0.4, 0.5, 0.2, 0.3, 0.4};
        options.model.worldForwardKinematics = [](const std::vector<double>& q) {
            Eigen::Isometry3d pose = Eigen::Isometry3d::Identity();
            pose.translation() = Eigen::Vector3d(std::sin(q[0]), std::sin(q[1]), std::sin(q[2]));
            pose.linear() = (Eigen::AngleAxisd(q[3], Eigen::Vector3d::UnitX()) *
                Eigen::AngleAxisd(q[4], Eigen::Vector3d::UnitY()) *
                Eigen::AngleAxisd(q[5], Eigen::Vector3d::UnitZ())).toRotationMatrix();
            return pose;
        };
        options.lower.assign(6, -pi); options.upper.assign(6, pi);
        options.lower[0] = -3 * pi; options.upper[0] = 3 * pi;
        StoredMotionPlan input;
        robottrajectory::TimedCartesianPoint point;
        point.tcpPose = options.model.worldForwardKinematics(options.model.seedJoints);
        input.cartesianControlPoints.points.push_back(point);
        auto result = ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(input, options);
        require(result.success && result.layers[0].candidates.size() >= 6, "Multi-start IK retains multiple geometric roots and turns");
        bool positiveTurn = false, negativeTurn = false, verified = true;
        for(const auto& candidate : result.layers[0].candidates) {
            positiveTurn |= candidate.turns[0] > 0; negativeTurn |= candidate.turns[0] < 0;
            const auto actual = options.model.worldForwardKinematics(candidate.joints);
            verified &= (actual.translation() - point.tcpPose.translation()).norm() <= options.positionTolerance;
            verified &= Eigen::AngleAxisd(actual.linear().transpose() * point.tcpPose.linear()).angle() <= options.orientationTolerance;
            for(int j = 0; j < 6; ++j) { verified &= candidate.joints[j] >= options.lower[j] && candidate.joints[j] <= options.upper[j]; }
        }
        require(verified && positiveTurn && negativeTurn, "All candidates satisfy actual FK, bounds, and retain both turn signs");
        StoredMotionPlan selected;
        std::string error;
        require(ProjectTrajectoryInverseKinematics::selectMultiIkTrajectory(result, {1}, selected, error) &&
            selected.trajectory.points.size() == 1 && selected.trajectory.points[0].q == result.layers[0].candidates[1].joints,
            "Chosen trajectory contains exactly one selected solution per target");
        require(!ProjectTrajectoryInverseKinematics::selectMultiIkTrajectory(result, {999999}, selected, error),
            "Invalid selection cannot create a playback trajectory");
        options.maxCandidatesPerPoint = 1;
        auto capped = ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(input, options);
        require(capped.layers[0].truncated && capped.layers[0].candidates.size() == 1, "Candidate cap explicitly marks truncated enumeration");
        options.maxCandidatesPerPoint = 512;
        auto unreachable = input;
        unreachable.cartesianControlPoints.points[0].tcpPose.translation().x() = 10;
        auto failed = ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(unreachable, options);
        require(!failed.success && failed.layers[0].candidates.empty() &&
            !ProjectTrajectoryInverseKinematics::selectMultiIkTrajectory(failed, {0}, selected, error), "No-solution layer blocks playback");
        options.cancelled = []() { return true; };
        auto cancelled = ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(input, options);
        require(cancelled.cancelled && !cancelled.success && cancelled.layers.empty(), "Cancellation returns no complete trajectory");
        options.cancelled = {};
        options.lower[0] = 2; options.upper[0] = 1;
        require(!ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(input, options).success, "Invalid bounds are rejected");
        options.lower[0] = -pi; options.upper[0] = pi;
        options.model.worldForwardKinematics = {};
        require(!ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(input, options).success, "Multi IK never falls back to legacy nominal DH");
    }

    void verifyMultiIkImported(const std::filesystem::path& projectPath,
        const std::filesystem::path& trajectoryPath, const std::filesystem::path& reportPath, int seeds = 64)
    {
        using namespace motion_planning;
        simulation_project::ProjectDocument document;
        std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "Multi IK project loads");
        document.collision.query.enabled = false;
        ProjectScene scene;
        scene.setProjectDocument(document, projectPath.parent_path());
        require(scene.initialize(), "Multi IK actual robot scene initializes");
        TrajectoryImportOptions importOptions;
        importOptions.robotId = "ABB4600_urdf";
        auto imported = ProjectTrajectoryImporter::importFile(trajectoryPath, importOptions);
        require(imported.success, "Multi IK directly imports cartesian targets");
        if(!imported.success) { return; }
        CartesianMultiIkOptions options;
        options.model.robotId = importOptions.robotId;
        options.model.jointNames = ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto fk = scene.robotForwardKinematics(importOptions.robotId, options.model.jointNames, true);
        require(static_cast<bool>(fk), "Multi IK uses actual world/TCP snapshot");
        if(!fk) { return; }
        options.model.worldForwardKinematics = [fk](const auto& q) {
            return fk(ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q));
        };
        constexpr double pi = 3.14159265358979323846;
        options.lower.assign(6, -pi); options.upper.assign(6, pi);
        std::vector<double> modelLower, modelUpper;
        require(ProjectTrajectoryInverseKinematics::readRevoluteJointLimits(document, projectPath.parent_path(),
            options.model.robotId, options.model.jointNames, modelLower, modelUpper, error), "Actual URDF limits load without collision detectors");
        options.classifyConfiguration = ProjectTrajectoryInverseKinematics::createIrb4600ConfigurationClassifier(
            document, projectPath.parent_path(), options.model.robotId, options.model.jointNames, true, error);
        require(static_cast<bool>(options.classifyConfiguration), "Actual ABB geometry classifier loads");
        if(!options.classifyConfiguration) { return; }
        options.seedCount = seeds;
        options.progress = [](std::size_t done, std::size_t total) {
            if(done % 25 == 0 || done == total) { std::cout << "Multi IK progress " << done << '/' << total << '\n'; }
        };
        const auto start = std::chrono::steady_clock::now();
        const auto result = ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(imported.plan, options);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << result.message << " elapsed_seconds=" << seconds << '\n';
        require(result.success, "Every imported target has valid multi IK candidates");
        std::size_t minCount = 999999, maxCount = 0, total = 0;
        double maxPosition = 0, maxOrientation = 0;
        bool branchesValid = true, periodicBranches = true, orderedBranches = true, distinctBranches = true;
        std::set<int> allBranches;
        ConfigurationSelectionCatalog catalog;
        for(int i = 0; i < 8; ++i) {
            catalog.categoryLabels << QStringLiteral("B%1 \u80a9%2 / \u8098%3 / \u8155%4").arg(i + 1)
                .arg(i & 4 ? "-" : "+").arg(i & 2 ? "-" : "+").arg(i & 1 ? "-" : "+");
        }
        catalog.categoryLabels << QStringLiteral("Boundary");
        std::ofstream report(reportPath);
        report << "point,candidate,j1_deg,j2_deg,j3_deg,j4_deg,j5_deg,j6_deg,position_error_mm,orientation_error_deg,branch,shoulder,elbow,wrist\n" << std::setprecision(12);
        for(const auto& layer : result.layers) {
            minCount = std::min(minCount, layer.candidates.size()); maxCount = std::max(maxCount, layer.candidates.size());
            std::set<int> layerBranches;
            QVector<int> categories;
            QStringList details;
            int previousBranch = 0;
            const auto& target = imported.plan.cartesianControlPoints.points[layer.pointIndex].tcpPose;
            const Eigen::JacobiSVD<Eigen::Matrix3d> svd(target.linear(), Eigen::ComputeFullU | Eigen::ComputeFullV);
            const Eigen::Matrix3d desiredRotation = svd.matrixU() * svd.matrixV().transpose();
            for(std::size_t c = 0; c < layer.candidates.size(); ++c) {
                const auto& candidate = layer.candidates[c];
                const auto& config = candidate.configuration;
                const int branch = config.stableId();
                branchesValid &= config.available && branch >= 1 && branch <= 8;
                layerBranches.insert(branch); allBranches.insert(branch);
                orderedBranches &= branch >= previousBranch;
                previousBranch = branch;
                categories << (branch ? branch : 9);
                details << QStringLiteral("B%1 candidate %2").arg(branch).arg(c + 1);
                auto lifted = candidate.joints;
                for(int j = 0; j < 6; ++j) { lifted[j] += (j % 2 ? -2 : 2) * pi; }
                periodicBranches &= options.classifyConfiguration(lifted).stableId() == branch;
                const auto actual = options.model.worldForwardKinematics(candidate.joints);
                const double pe = (actual.translation() - target.translation()).norm();
                const double re = Eigen::AngleAxisd(actual.linear().transpose() * desiredRotation).angle();
                maxPosition = std::max(maxPosition, pe); maxOrientation = std::max(maxOrientation, re);
                ++total;
                report << layer.pointIndex + 1 << ',' << c + 1;
                for(double q : candidate.joints) { report << ',' << q * 180 / pi; }
                report << ',' << pe * 1000 << ',' << re * 180 / pi << ',' << branch << ','
                    << config.shoulder << ',' << config.elbow << ',' << config.wrist << '\n';
            }
            if(layer.candidates.size() == 8) {
                distinctBranches &= layerBranches.size() == 8;
            }
            catalog.categories << categories;
            catalog.candidateDetails << details;
        }
        require(distinctBranches, "Eight-root layers have eight distinct shoulder/elbow/wrist branches");
        require(branchesValid && periodicBranches && orderedBranches,
            "Actual geometry branch labels are available, sorted, and invariant under every joint turn");
        std::cout << "Distinct geometric branches=" << allBranches.size() << '\n';
        std::cout << "Multi IK min=" << minCount << " max=" << maxCount << " total=" << total
            << " max_mm=" << maxPosition * 1000 << " max_deg=" << maxOrientation * 180 / pi << '\n';
        require(minCount >= 2 && maxPosition <= options.positionTolerance && maxOrientation <= options.orientationTolerance,
            "Independent actual FK verifies every candidate and multiple configurations at every point");
        const auto graphStart = std::chrono::steady_clock::now();
        const auto ranked = ProjectLayeredIkGraph::filter(result);
        std::cout << "Layered graph " << ranked.message << " elapsed_seconds="
            << std::chrono::duration<double>(std::chrono::steady_clock::now() - graphStart).count() << '\n';
        bool pathsValid = ranked.success && !ranked.paths.empty();
        std::set<std::vector<std::size_t>> sequences;
        double previousCost = -1;
        auto graphReportPath = reportPath; graphReportPath.replace_extension(".topm.csv");
        std::ofstream graphReport(graphReportPath);
        graphReport << "rank,cost,point,time,candidate,j1_deg,j2_deg,j3_deg,j4_deg,j5_deg,j6_deg\n" << std::setprecision(15);
        for(std::size_t rank = 0; rank < ranked.paths.size(); ++rank) {
            const auto& path = ranked.paths[rank];
            StoredMotionPlan seed;
            pathsValid &= ProjectTrajectoryInverseKinematics::selectMultiIkTrajectory(result, path.selections, seed, error);
            pathsValid &= sequences.insert(path.selections).second && path.cost >= previousCost &&
                seed.trajectory.points.size() == imported.plan.cartesianControlPoints.points.size();
            previousCost = path.cost;
            double independentCost = 0;
            for(std::size_t i = 0; i < seed.trajectory.points.size(); ++i) {
                const auto& q = seed.trajectory.points[i].q;
                pathsValid &= seed.trajectory.points[i].time == imported.plan.cartesianControlPoints.points[i].time;
                if(i) {
                    for(std::size_t j = 0; j < q.size(); ++j) { independentCost += std::pow(q[j] - seed.trajectory.points[i - 1].q[j], 2); }
                }
                graphReport << rank + 1 << ',' << path.cost << ',' << i + 1 << ',' << seed.trajectory.points[i].time << ',' << path.selections[i] + 1;
                for(double value : q) { graphReport << ',' << value * 180 / pi; }
                graphReport << '\n';
            }
            pathsValid &= std::abs(independentCost - path.cost) < 1.0e-8 * std::max(1.0, path.cost);
        }
        require(pathsValid, "Real Top-M candidates are unique, sorted and preserve every original point/time with independently checked cost");
        LayeredIkGraphOptions groupedOptions; groupedOptions.maxPaths = 3;
        const auto groupedStart = std::chrono::steady_clock::now();
        const auto grouped = ProjectLayeredIkGraph::filterByStart(result, groupedOptions);
        std::cout << "Fixed-start graph " << grouped.message << " elapsed_seconds="
            << std::chrono::duration<double>(std::chrono::steady_clock::now() - groupedStart).count() << '\n';
        bool groupedValid = grouped.success && !grouped.paths.empty();
        std::set<std::size_t> coveredStarts;
        QVector<QVector<int>> globalPlot, startPlot;
        QStringList startLabels;
        for(const auto& path : ranked.paths) {
            QVector<int> indices;
            for(auto index : path.selections) { indices.push_back(static_cast<int>(index + 1)); }
            globalPlot.push_back(indices);
        }
        auto groupedReportPath = reportPath; groupedReportPath.replace_extension(".start-topk.csv");
        std::ofstream groupedReport(groupedReportPath);
        groupedReport << "start,group_rank,global_rank,cost,point,time,candidate,j1_deg,j2_deg,j3_deg,j4_deg,j5_deg,j6_deg\n" << std::setprecision(15);
        std::size_t previousStart = std::numeric_limits<std::size_t>::max(), groupRank = 0;
        double lastCost = -1;
        for(const auto& path : grouped.paths) {
            const auto start = path.selections.front(); coveredStarts.insert(start);
            if(start != previousStart) { groupRank = 0; lastCost = -1; previousStart = start; }
            ++groupRank;
            StoredMotionPlan seed;
            groupedValid &= ProjectTrajectoryInverseKinematics::selectMultiIkTrajectory(result, path.selections, seed, error);
            groupedValid &= seed.trajectory.points.size() == result.layers.size() && path.cost >= lastCost && groupRank <= 3;
            lastCost = path.cost;
            QString globalRank = QStringLiteral(">%1").arg(ranked.paths.size());
            for(std::size_t rank = 0; rank < ranked.paths.size(); ++rank) {
                if(ranked.paths[rank].selections == path.selections) { globalRank = QString::number(rank + 1); break; }
            }
            startLabels << QStringLiteral("Start #%1 / within #%2 / global %3").arg(start + 1).arg(groupRank).arg(globalRank);
            QVector<int> indices;
            double independentCost = 0;
            for(std::size_t i = 0; i < seed.trajectory.points.size(); ++i) {
                const auto& point = seed.trajectory.points[i]; indices.push_back(static_cast<int>(path.selections[i] + 1));
                groupedValid &= point.time == imported.plan.cartesianControlPoints.points[i].time;
                if(i) {
                    for(std::size_t j = 0; j < point.q.size(); ++j) { independentCost += std::pow(point.q[j] - seed.trajectory.points[i - 1].q[j], 2); }
                }
                groupedReport << start + 1 << ',' << groupRank << ',' << globalRank.toStdString() << ',' << path.cost << ',' << i + 1 << ',' << point.time << ',' << path.selections[i] + 1;
                for(double value : point.q) { groupedReport << ',' << value * 180 / pi; }
                groupedReport << '\n';
            }
            groupedValid &= std::abs(independentCost - path.cost) < 1.0e-8 * std::max(1.0, path.cost);
            startPlot.push_back(indices);
        }
        require(groupedValid && coveredStarts.size() == result.layers.front().candidates.size(),
            "Real conditional paths cover every start, preserve all points/turns/times, and have independently verified costs");
        // A rigid base displacement must never change a geometric branch identity.
        auto relocated = document;
        for(auto& robot : relocated.robots) {
            if(robot.id == options.model.robotId) {
                robot.baseTransform.x += 2.0; robot.baseTransform.y -= 0.7;
                robot.baseTransform.roll += 0.31; robot.baseTransform.yaw -= 1.2;
            }
        }
        const auto relocatedClassifier = ProjectTrajectoryInverseKinematics::createIrb4600ConfigurationClassifier(
            relocated, projectPath.parent_path(), options.model.robotId, options.model.jointNames, true, error);
        bool baseInvariant = bool(relocatedClassifier);
        for(const auto& candidate : result.layers.front().candidates) {
            baseInvariant &= relocatedClassifier &&
                relocatedClassifier(candidate.joints).stableId() == candidate.configuration.stableId();
        }
        require(baseInvariant, "Geometric classification is independent of world base rotation/translation");
        require(!options.classifyConfiguration({}).available, "Invalid joints stay unclassified");

        auto boundaryJoints = result.layers.front().candidates.front().joints;
        bool foundBoundary = false;
        double left = -pi;
        boundaryJoints[4] = left;
        int leftSign = options.classifyConfiguration(boundaryJoints).wrist;
        for(int step = 1; step <= 100 && !foundBoundary; ++step) {
            double right = -pi + step * 2 * pi / 100;
            boundaryJoints[4] = right;
            const auto rightConfig = options.classifyConfiguration(boundaryJoints);
            if(rightConfig.wrist == 0) { foundBoundary = rightConfig.stableId() == 0; break; }
            if(rightConfig.wrist != leftSign) {
                for(int iteration = 0; iteration < 50; ++iteration) {
                    const double middle = (left + right) / 2;
                    boundaryJoints[4] = middle;
                    const auto middleConfig = options.classifyConfiguration(boundaryJoints);
                    if(middleConfig.wrist == 0) { foundBoundary = middleConfig.stableId() == 0; break; }
                    if(middleConfig.wrist == leftSign) { left = middle; } else { right = middle; }
                }
                break;
            }
            left = right; leftSign = rightConfig.wrist;
        }
        require(foundBoundary, "Wrist branch boundary is explicit instead of being forced into B1..B8");
        auto* plot = new ConfigurationSelectionDialog(globalPlot, startPlot, startLabels, 0, true, nullptr, catalog);
        plot->show(); QApplication::processEvents();
        auto plotPath = reportPath; plotPath.replace_extension(".start-topk.png");
        require(plot->grab().save(QString::fromStdWString(plotPath.wstring())), "Save real fixed-start comparison plot");
        plot->close();


    }

    void verifyImportedIk(const std::filesystem::path& projectPath,
        const std::filesystem::path& trajectoryPath, const std::filesystem::path& reportPath)
    {
        using namespace motion_planning;
        simulation_project::ProjectDocument document;
        std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "IK project loads");
        if(!error.empty()) { std::cout << error << '\n'; }
        document.collision.query.enabled = false;
        for(auto& robot : document.robots) { robot.collisionEnabled = false; }
        ProjectScene scene;
        scene.setProjectDocument(document, projectPath.parent_path());
        require(scene.initialize(), "Actual URDF scene initializes for IK round trip");
        TrajectoryImportOptions importOptions;
        importOptions.robotId = "ABB4600_urdf";
        const auto imported = ProjectTrajectoryImporter::importFile(trajectoryPath, importOptions);
        require(imported.success, "User-format TCP matrix file imports");
        if(!imported.success) { return; }
        CartesianIkOptions options;
        options.robotId = importOptions.robotId;
        options.jointNames = ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        options.maxIterations = 5000;
        options.tolerance = 1.0e-6;
        options.stepSize = 0.01;
        options.damping = 0.001;
        std::cout << "IK points: " << imported.plan.cartesianControlPoints.points.size() << '\n';
        const auto oldResult = ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(
            document, imported.plan, options);
        const auto runtimeFk = scene.robotForwardKinematics(options.robotId, options.jointNames, true);
        require(static_cast<bool>(runtimeFk), "Model snapshot uses actual nozzle TCP");
        if(!runtimeFk) { return; }
        Eigen::Isometry3d before;
        scene.endEffectorWorldTransform(options.robotId, before);
        options.worldForwardKinematics = [runtimeFk](const std::vector<double>& joints) {
            return runtimeFk(ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(joints));
        };
        options.stepSize = 1.0;
        const auto result = ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(
            document, imported.plan, options);
        require(result.success, "All imported TCP targets solve with actual-model IK");
        for(const auto& diagnostic : result.diagnostics) { std::cout << diagnostic.message << '\n'; }
        Eigen::Isometry3d after;
        scene.endEffectorWorldTransform(options.robotId, after);
        require((before.matrix() - after.matrix()).norm() < 1.0e-12, "IK iterations do not move the displayed robot");
        std::ofstream report(reportPath);
        report << std::setprecision(12)
            << "index,target_x,target_y,target_z,old_x,old_y,old_z,actual_x,actual_y,actual_z,old_error_mm,error_mm,angle_error_deg\n";
        double oldMaximum = 0, maximum = 0, angleMaximum = 0, squaredSum = 0;
        scene.setEndEffectorTraceVisible(options.robotId, true);
        std::vector<simulation_project::TransformDesc> overlay;
        for(std::size_t index = 0; index < result.points.size(); ++index) {
            const auto& input = imported.plan.cartesianControlPoints.points[index].tcpPose;
            const auto& solved = result.points[index];
            if(!solved.success || oldResult.points[index].joints.size() != 6) { continue; }
            const Eigen::Vector3d oldPosition = options.worldForwardKinematics(oldResult.points[index].joints).translation();
            const auto joints = ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(solved.joints);
            for(std::size_t j = 0; j < joints.size(); ++j) {
                scene.setRobotJointValue(options.robotId, options.jointNames[j], joints[j]);
            }
            Eigen::Isometry3d actual;
            if(!scene.endEffectorWorldTransform(options.robotId, actual)) {
                require(false, "Playback must have an actual TCP pose");
                return;
            }
            scene.appendEndEffectorTraceSample();
            const Eigen::JacobiSVD<Eigen::Matrix3d> svd(input.linear(), Eigen::ComputeFullU | Eigen::ComputeFullV);
            const Eigen::Matrix3d desiredRotation = svd.matrixU() * svd.matrixV().transpose();
            const double distance = (actual.translation() - input.translation()).norm() * 1000;
            const double oldDistance = (oldPosition - input.translation()).norm() * 1000;
            const double angle = Eigen::AngleAxisd(desiredRotation * actual.linear().transpose()).angle() * 180 / 3.141592653589793;
            oldMaximum = std::max(oldMaximum, oldDistance);
            maximum = std::max(maximum, distance);
            angleMaximum = std::max(angleMaximum, angle);
            squaredSum += distance * distance;
            report << index;
            for(const Eigen::Vector3d& v : { Eigen::Vector3d(input.translation()), oldPosition, Eigen::Vector3d(actual.translation()) }) {
                report << ',' << v.x() << ',' << v.y() << ',' << v.z();
            }
            report << ',' << oldDistance << ',' << distance << ',' << angle << '\n';
            simulation_project::TransformDesc p;
            p.x = input.translation().x(); p.y = input.translation().y(); p.z = input.translation().z();
            overlay.push_back(p);
        }
        std::cout << "IK legacy solved=" << oldResult.solvedPointCount() << '/' << oldResult.points.size()
            << " old_max_mm=" << oldMaximum << " new_max_mm=" << maximum
            << " new_rms_mm=" << std::sqrt(squaredSum / std::max<std::size_t>(1, result.points.size()))
            << " new_max_deg=" << angleMaximum << '\n';
        require(maximum < 0.002 && angleMaximum < 0.0001, "Actual playback TCP matches imported positions and orientations");
        require(scene.endEffectorTracePointCount() > 1, "Round trip records actual endpoint trace");
        // Verify snapshot base/tool frames survive scene lifetime independently.
        scene.setTrajectoryControlPointOverlay("imported", overlay);
        QOpenGLFramebufferObject framebuffer(1200, 900, QOpenGLFramebufferObject::CombinedDepthStencil);
        require(framebuffer.bind(), "IK comparison framebuffer binds");
        scene.resize(1200, 900);
        scene.setCameraView(ProjectSceneCameraView::Isometric);
        scene.update(0.0);
        scene.render();
        auto imagePath = reportPath;
        imagePath.replace_extension(".png");
        require(framebuffer.toImage().save(QString::fromStdWString(imagePath.wstring())),
            "Actual robot, imported control points and playback trace render to comparison image");
        const QImage markersOn = framebuffer.toImage();
        scene.setTrajectoryControlPointOverlay("imported", overlay, false);
        scene.update(0.0);
        scene.render();
        const QImage markersOff = framebuffer.toImage();
        int changedPixels = 0;
        for(int y = 0; y < markersOn.height(); ++y) {
            for(int x = 0; x < markersOn.width(); ++x) {
                if(markersOn.pixel(x, y) != markersOff.pixel(x, y)) { ++changedPixels; }
            }
        }
        require(changedPixels > 10, "Hiding sphere markers changes the actual rendered overlay");
        scene.setTrajectoryControlPointOverlay("imported", overlay, true);
        scene.update(0.0);
        scene.render();
        require(framebuffer.toImage() == markersOn, "Re-enabling sphere markers restores the same rendered overlay");
        framebuffer.release();
        scene.setEndEffectorTraceVisible(options.robotId, false);
        scene.setProjectDocument(document, projectPath.parent_path());
        require(options.worldForwardKinematics(result.points.front().joints).matrix().allFinite(),
            "FK snapshot stays valid after project replacement");
    }

    void verifyCdfProgress(QApplication& application, const std::filesystem::path& projectPath,
        const std::filesystem::path& folder)
    {
        using namespace robot_qt_viewer;
        simulation_project::ProjectDocument document;
        std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "CDF GUI project loads");
        RobotViewport viewport; viewport.resize(640, 480);
        require(viewport.loadProjectDocument(document, projectPath.parent_path()), "CDF progress viewport loads actual TCP");
        viewport.show(); application.processEvents();
        simulation_project::ProjectSession session;
        session.setDocument(document, projectPath, false, false);
        RobotQtViewerEventHub hub;
        RobotQtViewerDocumentController documentController(session, hub);
        RobotQtViewerSelectionModel selection(hub);
        RobotQtViewerViewportPreviewState preview(hub);
        RobotQtViewerOperationStatusStore status(hub);
        RobotQtViewerDocumentContext context(session, documentController, selection, preview, hub, status);
        OverlayObservingServices services(viewport); context.setMotionPlanningViewport(&services);
        selection.selectRobotLink(QStringLiteral("ABB4600_urdf"), QStringLiteral("Link6"));
        MotionPlanningEditorWidget widget;
        MotionPlanningModuleController controller(widget, context);
        const auto input = folder / "cdf_ui_seed.txt";
        {
            std::ofstream file(input);
            file << "time_s J1_deg J2_deg J3_deg J4_deg J5_deg J6_deg\n"
                 << "0 97.4408658925731 1.19975930231299 33.9729339352994 97.9832197903536 -105.415134110182 -24.3060644844148\n"
                 << "1 97.4408658925731 1.19975930231299 33.9729339352994 97.9832197903536 -105.415134110182 -24.3060644844148\n";
        }
        QTimer chooseFile;
        QObject::connect(&chooseFile, &QTimer::timeout, &widget, [&]() {
            for(auto* window : application.topLevelWidgets()) {
                if(auto* file = qobject_cast<QFileDialog*>(window)) {
                    file->selectFile(QString::fromStdWString(input.wstring()));
                    QMetaObject::invokeMethod(file, "accept", Qt::DirectConnection);
                }
            }
        });
        chooseFile.start(10);
        widget.importCdfJointAnglesRequested();
        chooseFile.stop();
        auto* initial = widget.findChild<QTableWidget*>(QStringLiteral("cdfInitialJointAngles"));
        require(initial && initial->rowCount() == 2, "CDF GUI imports two real seed points");
        if(!initial || initial->rowCount() != 2) return;
        for(bool invalidate : {false, true}) {
            int ticks = 0;
            bool checkedClose = false;
            const auto plansBefore = motion_planning::MotionPlanningProjectStore::plans(context.document());
            QTimer heartbeat;
            QObject::connect(&heartbeat, &QTimer::timeout, &widget, [&]() {
                auto* dialog = widget.findChild<QDialog*>(QStringLiteral("cdfRepairProgress"));
                if(!dialog || !dialog->isVisible()) return;
                ++ticks;
                if(ticks == 20 && !invalidate) dialog->grab().save(QStringLiteral("cdf-progress-preview.png"));
                if(!checkedClose) {
                    checkedClose = true;
                    dialog->reject();
                    require(dialog->isVisible(), "CDF task cannot destroy widgets before worker completion");
                    if(invalidate) documentController.publishDocumentChanged(QStringLiteral("cdfStaleRegression"), false);
                }
            });
            heartbeat.start(10);
            widget.repairImportedCdfTrajectoryRequested();
            heartbeat.stop();
            require(ticks > 0 && checkedClose, "CDF worker leaves the GUI event loop responsive");
            require(!widget.findChild<QDialog*>(QStringLiteral("cdfRepairProgress")), "CDF task window is released after worker join");
            QString resultText;
            for(auto* label : widget.findChildren<QLabel*>()) {
                if(label->text().contains(QStringLiteral(" | Log: "))) resultText = label->text();
            }
            require(!resultText.isEmpty(), "CDF final result shows measured duration and log path");
            const QString logPath = resultText.section(QStringLiteral(" | Log: "), 1);
            QFile log(logPath);
            const bool opened = log.open(QIODevice::ReadOnly);
            const auto logText = opened ? log.readAll() : QByteArray{};
            require(opened && logText.contains("CDF performance v3") && logText.contains("CDF query workers:") &&
                logText.contains("Finished:"), "CDF log contains executable, worker count, stages and final status");
            const auto plansAfter = motion_planning::MotionPlanningProjectStore::plans(context.document());
            if(invalidate) {
                require(resultText.contains(QStringLiteral("Project changed")) && plansAfter.size() == plansBefore.size(),
                    "Changed project rejects stale CDF result");
            } else {
                require(plansAfter.size() > plansBefore.size(), "Completed CDF snapshot commits through the document service");
                auto* stages = widget.findChild<QComboBox*>(QStringLiteral("cdfStageSelection"));
                auto* stageTable = widget.findChild<QTableWidget*>(QStringLiteral("cdfStageJointAngles"));
                require(stages && stages->count() == 3 && stageTable && stageTable->rowCount() == 3 && initial->rowCount() == 2,
                    "CDF stores input/APF/final reports while retaining the original two input rows");
                widget.cdfAnalysisRequested(); application.processEvents();
                auto* analysis = widget.findChild<QDialog*>(QStringLiteral("cdfTrajectoryAnalysis"));
                auto* quality = analysis ? analysis->findChild<QTableWidget*>(QStringLiteral("cdfQualityComparison")) : nullptr;
                require(quality && quality->rowCount() == 19 && quality->columnCount() == 4,
                    "CDF quality report exposes three stages and timing/collision/smoothness/limit metrics");
                if(analysis) analysis->close();
            }
        }
    }

    void verifyApfWindow(const std::filesystem::path& projectPath,
        const std::filesystem::path& inputPath, const std::filesystem::path& folder,
        std::size_t first, std::size_t last, std::size_t padding, const std::filesystem::path& partialPath)
    {
        using namespace motion_planning;
        using namespace motion_planning::detail;
        std::filesystem::create_directories(folder);
        simulation_project::ProjectDocument document; std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "APF window project loads");
        ProjectScene actual; actual.setProjectDocument(document,projectPath.parent_path()); actual.initialize();
        const auto names=ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto fk=actual.robotForwardKinematics("ABB4600_urdf",names,true);
        const auto imported=ProjectCdfJointAngleImporter::importFile(inputPath);
        require(fk && imported.success && imported.points.size()>1,"APF window actual FK and original seed available");
        if(!fk || !imported.success || imported.points.size()<2) return;
        const auto runtime=[](const auto& point) {
            auto q=point.jointAnglesDegrees; for(auto& v:q) v*=3.14159265358979323846/180.0;
            return ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q);
        };
        ApfPath dense{runtime(imported.points.front())};
        ApfGuidance guide; guide.maxDeviation=0.10;
        guide.position=[fk](const auto& q) -> ApfState { const auto p=fk(q).translation().eval(); return {p.x(),p.y(),p.z()}; };
        guide.positions.push_back(guide.position(dense.front()));
        for(std::size_t i=1;i<imported.points.size();++i) {
            const auto a=runtime(imported.points[i-1]), b=runtime(imported.points[i]);
            const auto pa=guide.position(a),pb=guide.position(b);
            double delta=0; for(int j=0;j<6;++j) delta=std::max(delta,std::abs(b[j]-a[j]));
            const int steps=std::max(2,static_cast<int>(std::ceil(delta/0.04)));
            for(int k=1;k<=steps;++k) {
                const double t=static_cast<double>(k)/steps; auto q=a,p=pa;
                for(int j=0;j<6;++j) q[j]+=t*(b[j]-a[j]); for(int j=0;j<3;++j) p[j]+=t*(pb[j]-pa[j]);
                dense.push_back(q);guide.positions.push_back(p);
            }
        }
        if(!partialPath.empty()) {
            const auto partial=ProjectCdfJointAngleImporter::importFile(partialPath);
            require(partial.success && partial.points.size()==dense.size(),"Partial APF checkpoint retains original station count");
            if(!partial.success || partial.points.size()!=dense.size()) return;
            for(std::size_t i=0;i<dense.size();++i) dense[i]=runtime(partial.points[i]);
        }
        ProjectCdfQpRepairOptions options;
        std::string detector;
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,"ABB4600_urdf",options,&detector),"Window collision setup matches production");
        ProjectPlanningRequest request; request.robotId="ABB4600_urdf";request.jointNames=names;
        request.start=dense.front();request.goal=dense.back();request.collisionDetectorIds={detector};request.validation.maxJointStep=0.001;
        // Production CDF uses only its selected full robot/obstacle detector.
        document.collision.detectors.erase(std::remove_if(document.collision.detectors.begin(),document.collision.detectors.end(),
            [&](const auto& d){return d.id!=detector;}),document.collision.detectors.end());
        auto scene=ProjectPlanningSceneBuilder::build(document,projectPath.parent_path(),request,&error);
        require(bool(scene),"APF window private collision scene builds"); if(!scene) return;
        CdfQueryBatch queries(*scene,document,projectPath.parent_path(),request,4);
        ProjectCdfQpRepairStatistics stats;
        const auto safe=[&](std::size_t i){return scene->validateState(dense[i]).valid &&
            followsApfGuide(dense[i],dense[i],guide.positions[i],guide.positions[i],guide);};
        std::size_t left=first-1,right=last-1;
        while(left>0 && !safe(left)) --left;while(right+1<dense.size() && !safe(right)) ++right;
        require(safe(left)&&safe(right),"Window has safe exact boundary anchors");if(!safe(left)||!safe(right))return;
        std::size_t begin=left>padding?left-padding:0,end=std::min(dense.size()-1,right+padding);
        while(begin<left&&!safe(begin))++begin;while(end>right&&!safe(end))--end;
        ApfPath reference(dense.begin()+begin,dense.begin()+end+1);
        ApfGuidance local=guide;local.positions.assign(guide.positions.begin()+begin,guide.positions.begin()+end+1);
        ApfState lower=reference.front(),upper=lower;
        for(int j=0;j<6;++j) {
            for(const auto& q:reference){lower[j]=std::min(lower[j],q[j]);upper[j]=std::max(upper[j],q[j]);}
            lower[j]-=0.75;upper[j]+=0.75;
            if(!scene->jointBounds()[j].continuous){lower[j]=padding>=120?scene->jointBounds()[j].lower:std::max(lower[j],scene->jointBounds()[j].lower);upper[j]=padding>=120?scene->jointBounds()[j].upper:std::min(upper[j],scene->jointBounds()[j].upper);}
        }
        std::cout<<"APF window anchors "<<begin+1<<" -> "<<end+1<<", nodes="<<reference.size()<<'\n';
        const auto started=std::chrono::steady_clock::now();
        ApfOracle oracle;
        oracle.distance=[&](const auto& q){const auto d=queries.distances({q},0,5,stats).front();return d.valid?d.rawDistance:std::numeric_limits<double>::quiet_NaN();};
        oracle.distances=[&](const auto& states){const auto ds=queries.distances(states,0,5,stats);std::vector<double> values;for(const auto& d:ds) values.push_back(d.valid?d.rawDistance:std::numeric_limits<double>::quiet_NaN());return values;};
        oracle.motionValid=[&](const auto& a,const auto& b){return queries.pathValid({a,b},request.validation);};
        oracle.progress=[&](int f,int i){std::cout<<"field="<<f<<" iteration="<<i<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<'\n';};
        oracle.failedStation=[&](int f,std::size_t i,const auto& q){
            const auto p=local.position(q),&target=local.positions[i];double err=0;for(int j=0;j<3;++j)err+=std::pow(p[j]-target[j],2);
            std::cout<<"FAILED field="<<f<<" node="<<begin+i+1<<" TCP(mm)="<<std::sqrt(err)*1000<<" distance="<<oracle.distance(q)<<" q=";
            for(double v:q)std::cout<<v<<' ';std::cout<<" targetQ=";for(double v:reference[i])std::cout<<v<<' ';std::cout<<'\n';
        };
        ApfPath output;const bool solved=planApfPath(reference,lower,upper,oracle,0.01,&output,&local);
        std::cout<<queries.summary()<<'\n';require(solved,"Actual failing APF interval now solves under 100 mm");if(!solved)return;
        require(output.size()==reference.size()&&followsApfPath(output,local),"Solved window passes independent TCP correspondence check");
        const auto fresh=queries.motions(output,request.validation,false);
        require(std::all_of(fresh.begin(),fresh.end(),[](const auto& v){return v.valid;}),"Solved window passes fresh dense collision validation");
        std::ofstream file(folder/"window.txt");file<<"time_s J1_deg J2_deg J3_deg J4_deg J5_deg J6_deg\n"<<std::setprecision(15);
        std::ofstream csv(folder/"window.csv");csv<<"node,targetX,targetY,targetZ,tcpX,tcpY,tcpZ\n"<<std::setprecision(15);
        for(std::size_t i=0;i<output.size();++i){file<<i*0.1;for(double v:ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(output[i]))file<<' '<<v*180.0/3.14159265358979323846;file<<'\n';
            csv<<begin+i+1;for(double v:local.positions[i])csv<<','<<v;for(double v:local.position(output[i]))csv<<','<<v;csv<<'\n';}
    }

    void inspectApfCandidates(const std::filesystem::path& projectPath,
        const std::filesystem::path& cartesianPath, const std::filesystem::path& folder)
    {
        using namespace motion_planning;
        using namespace motion_planning::detail;
        std::filesystem::create_directories(folder);
        simulation_project::ProjectDocument document; std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "Candidate project loads");
        ProjectScene actual; actual.setProjectDocument(document,projectPath.parent_path());
        require(actual.initialize(),"Candidate actual scene initializes");
        const auto names=ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto fk=actual.robotForwardKinematics("ABB4600_urdf",names,true);
        TrajectoryImportOptions importOptions;importOptions.robotId="ABB4600_urdf";
        const auto targets=ProjectTrajectoryImporter::importFile(cartesianPath,importOptions);
        require(fk && targets.success,"Candidate original Cartesian targets and actual FK available");
        if(!fk || !targets.success)return;
        CartesianMultiIkOptions ikOptions;ikOptions.model.robotId=importOptions.robotId;ikOptions.model.jointNames=names;
        require(ProjectTrajectoryInverseKinematics::readRevoluteJointLimits(document,projectPath.parent_path(),
            importOptions.robotId,names,ikOptions.lower,ikOptions.upper,error),"Candidate actual bounds available");
        ikOptions.lower=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(ikOptions.lower);
        ikOptions.upper=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(ikOptions.upper);
        for(int j=0;j<6;++j) {
            if(ikOptions.lower[j]>ikOptions.upper[j])std::swap(ikOptions.lower[j],ikOptions.upper[j]);
            if(!std::isfinite(ikOptions.lower[j]))ikOptions.lower[j]=-3.14159265358979323846;
            if(!std::isfinite(ikOptions.upper[j]))ikOptions.upper[j]=3.14159265358979323846;
        }
        ikOptions.model.worldForwardKinematics=[fk](const auto& q){return fk(ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q));};
        ikOptions.classifyConfiguration=ProjectTrajectoryInverseKinematics::createIrb4600ConfigurationClassifier(
            document,projectPath.parent_path(),importOptions.robotId,names,true,error);
        auto endpoints=targets.plan;
        endpoints.cartesianControlPoints.points={targets.plan.cartesianControlPoints.points.front(),targets.plan.cartesianControlPoints.points.back()};
        const auto ik=ProjectTrajectoryInverseKinematics::solveAllCartesianControlPoints(endpoints,ikOptions);
        std::cout<<ik.message<<'\n';
        require(ik.success,"Exact original endpoint full-pose IK enumerates under actual limits");if(!ik.success)return;
        ProjectCdfQpRepairOptions options; std::string detector;
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,importOptions.robotId,options,&detector),"Candidate production collision setup");
        document.collision.detectors.erase(std::remove_if(document.collision.detectors.begin(),document.collision.detectors.end(),
            [&](const auto& d){return d.id!=detector;}),document.collision.detectors.end());
        ProjectPlanningRequest request;request.robotId=importOptions.robotId;request.jointNames=names;
        request.start=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(ik.layers.front().candidates.front().joints);
        request.goal=request.start;request.collisionDetectorIds={detector};request.validation.maxJointStep=0.02;
        auto scene=ProjectPlanningSceneBuilder::build(document,projectPath.parent_path(),request,&error);
        require(bool(scene),"Candidate production collision scene builds");if(!scene)return;
        CdfQueryBatch queries(*scene,document,projectPath.parent_path(),request,4);
        ApfGuidance guide;guide.maxDeviation=0.10;
        guide.position=[fk](const auto& q)->ApfState{const auto p=fk(q).translation().eval();return {p.x(),p.y(),p.z()};};
        const auto& controls=targets.plan.cartesianControlPoints.points;
        guide.positions.push_back({controls.front().tcpPose.translation().x(),controls.front().tcpPose.translation().y(),controls.front().tcpPose.translation().z()});
        std::ofstream report(folder/"candidates.csv");report<<"startBranch,endBranch,nodes,invalidSegments,minDistance,maxJumpRad,orderedCorridorValid,seed\n"<<std::setprecision(15);
        auto transported=targets.plan;
        std::vector<Eigen::Matrix3d> transportRotations;
        for(const auto& point:controls){const Eigen::JacobiSVD<Eigen::Matrix3d> svd(point.tcpPose.linear(),Eigen::ComputeFullU|Eigen::ComputeFullV);
            const Eigen::Matrix3d targetRotation=svd.matrixU()*svd.matrixV().transpose();
            const Eigen::Matrix3d next=transportRotations.empty()?targetRotation:
                Eigen::Quaterniond::FromTwoVectors(transportRotations.back().col(2),targetRotation.col(2)).toRotationMatrix()*transportRotations.back();
            transportRotations.push_back(next);}
        const Eigen::JacobiSVD<Eigen::Matrix3d> endSvd(controls.back().tcpPose.linear(),Eigen::ComputeFullU|Eigen::ComputeFullV);
        const Eigen::Matrix3d endRotation=endSvd.matrixU()*endSvd.matrixV().transpose();
        const Eigen::Matrix3d twist=transportRotations.back().transpose()*endRotation;
        const double twistAngle=std::atan2(twist(1,0),twist(0,0));
        for(std::size_t i=0;i<controls.size();++i)transported.cartesianControlPoints.points[i].tcpPose.linear()=
            transportRotations[i]*Eigen::AngleAxisd(twistAngle*static_cast<double>(i)/(controls.size()-1),Eigen::Vector3d::UnitZ()).toRotationMatrix();
        for(int variant=0;variant<2;++variant)for(const auto& a:ik.layers.front().candidates) {
            auto single=ikOptions.model;single.seedJoints=a.joints;single.stepSize=1.0;single.damping=0.001;single.maxIterations=300;
            const auto continuous=ProjectTrajectoryInverseKinematics::solveCartesianControlPoints(document,variant?transported:targets.plan,single);
            if(!continuous.success){std::cout<<"fullpose continuation failed B"<<a.configuration.stableId()<<'\n';continue;}
            ApfPath nodes;
            for(const auto& point:continuous.plan.trajectory.points){auto q=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(point.q);
                if(!nodes.empty())for(std::size_t j=0;j<q.size();++j)if(scene->jointBounds()[j].continuous)q[j]=nodes.back()[j]+std::remainder(q[j]-nodes.back()[j],2*3.14159265358979323846);
                nodes.push_back(q);}
            ApfPath path{nodes.front()};ApfGuidance poseGuide=guide;poseGuide.positions={guide.positions.front()};
            double maxJump=0;
            for(std::size_t i=1;i<nodes.size();++i){double delta=0;for(int j=0;j<6;++j)delta=std::max(delta,std::abs(nodes[i][j]-nodes[i-1][j]));maxJump=std::max(maxJump,delta);
                const int intervals=std::max(2,static_cast<int>(std::ceil(delta/0.04)));
                for(int k=1;k<=intervals;++k){const double t=static_cast<double>(k)/intervals;auto q=nodes[i-1];for(int j=0;j<6;++j)q[j]+=t*(nodes[i][j]-q[j]);
                    const Eigen::Vector3d pos=controls[i-1].tcpPose.translation()+t*(controls[i].tcpPose.translation()-controls[i-1].tcpPose.translation());
                    path.push_back(q);poseGuide.positions.push_back({pos.x(),pos.y(),pos.z()});}}
            const auto motions=queries.motions(path,request.validation);int invalid=0;for(const auto& m:motions)if(!m.valid)++invalid;
            ProjectCdfQpRepairStatistics stats;const auto distances=queries.distances(path,0,5,stats);double minimum=5;
            for(const auto& d:distances)minimum=std::min(minimum,d.valid?d.rawDistance:-100.0);
            const bool shape=followsApfPath(path,poseGuide);
            const std::string filename=(variant?"transport-B":"fullpose-B")+std::to_string(a.configuration.stableId())+".txt";
            std::ofstream file(folder/filename);file<<"time_s J1_deg J2_deg J3_deg J4_deg J5_deg J6_deg\n"<<std::setprecision(15);
            for(std::size_t i=0;i<nodes.size();++i){file<<controls[i].time;for(double q:ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(nodes[i]))file<<' '<<q*180.0/3.14159265358979323846;file<<'\n';}
            const int endBranch=ikOptions.classifyConfiguration(ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(nodes.back())).stableId();
            report<<a.configuration.stableId()<<','<<endBranch<<','<<path.size()<<','<<invalid<<','<<minimum<<','<<maxJump<<','<<shape<<','<<filename<<'\n';report.flush();
            std::cout<<filename<<" invalid="<<invalid<<" min="<<minimum<<" maxControlJump="<<maxJump<<" shape="<<shape<<'\n';
        }
        std::cout<<queries.summary()<<'\n';
    }

    void verifyCdfResult(const std::filesystem::path& projectPath,
        const std::filesystem::path& targetPath, const std::filesystem::path& seedPath,
        const std::filesystem::path& resultPath, const std::filesystem::path& reportPath, bool validateDynamics = true)
    {
        using namespace motion_planning;
        simulation_project::ProjectDocument document;std::string error;
        require(simulation_project::loadProjectDocument(projectPath,document,&error),"Independent validation project loads");
        ProjectScene actual;actual.setProjectDocument(document,projectPath.parent_path());require(actual.initialize(),"Independent actual TCP scene initializes");
        const auto names=ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto fk=actual.robotForwardKinematics("ABB4600_urdf",names,true);
        TrajectoryImportOptions importOptions;importOptions.robotId="ABB4600_urdf";
        const auto targets=ProjectTrajectoryImporter::importFile(targetPath,importOptions);
        const auto seed=ProjectCdfJointAngleImporter::importFile(seedPath),output=ProjectCdfJointAngleImporter::importFile(resultPath);
        require(fk && targets.success && seed.success && output.success && seed.points.size()==targets.plan.cartesianControlPoints.points.size(),
            "Independent original Cartesian targets and exported trajectories import");
        if(!fk || !targets.success || !seed.success || !output.success || seed.points.size()!=targets.plan.cartesianControlPoints.points.size())return;
        const auto runtime=[](const auto& point){auto q=point.jointAnglesDegrees;for(auto& v:q)v*=3.14159265358979323846/180.0;
            return ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q);};
        ProjectCdfQpRepairOptions options;std::string detector;
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,importOptions.robotId,options,&detector),"Independent production collision setup");
        document.collision.detectors.erase(std::remove_if(document.collision.detectors.begin(),document.collision.detectors.end(),
            [&](const auto& d){return d.id!=detector;}),document.collision.detectors.end());
        ProjectPlanningRequest request;request.robotId=importOptions.robotId;request.jointNames=names;request.start=runtime(output.points.front());
        request.goal=runtime(output.points.back());request.collisionDetectorIds={detector};request.validation.maxJointStep=0.00025;
        auto scene=ProjectPlanningSceneBuilder::build(document,projectPath.parent_path(),request,&error);
        require(bool(scene),"Independent fresh full collision scene builds");if(!scene)return;
        std::vector<Eigen::Vector3d> guide{targets.plan.cartesianControlPoints.points.front().tcpPose.translation()};
        const auto& controls=targets.plan.cartesianControlPoints.points;
        for(std::size_t i=1;i<seed.points.size();++i){const auto a=runtime(seed.points[i-1]),b=runtime(seed.points[i]);double delta=0;
            for(int j=0;j<6;++j){const double d=scene->jointBounds()[j].continuous?std::remainder(b[j]-a[j],2*3.14159265358979323846):b[j]-a[j];delta=std::max(delta,std::abs(d));}
            const int steps=std::max(2,static_cast<int>(std::ceil(std::max(0.0,delta/0.04)-1.0))+1);
            for(int k=1;k<=steps;++k){const double t=static_cast<double>(k)/steps;guide.push_back(controls[i-1].tcpPose.translation()+t*(controls[i].tcpPose.translation()-controls[i-1].tcpPose.translation()));}}
        require(guide.size()==output.points.size(),"Independent ordered Cartesian correspondence matches every exported knot");if(guide.size()!=output.points.size())return;
        robottrajectory::JointTrajectory trajectory;trajectory.interpolation=robottrajectory::TrajectoryInterpolation::Linear;
        for(const auto& p:output.points)trajectory.points.push_back({p.timeSeconds,runtime(p),{}, {}});
        const auto quality=evaluateTrajectoryQuality(trajectory,scene->jointBounds(),options.fallbackMaxVelocity,options.fallbackMaxAcceleration,{},fk);
        auto tcpReportPath=reportPath;tcpReportPath.replace_extension(".tcp.csv");
        std::ofstream tcpReport(tcpReportPath);tcpReport<<std::setprecision(15)<<"node,time,referenceX,referenceY,referenceZ,tcpX,tcpY,tcpZ\n";
        for(std::size_t i=0;i<trajectory.points.size();++i){tcpReport<<i+1<<','<<trajectory.points[i].time;
            for(double v:guide[i])tcpReport<<','<<v;for(double v:fk(trajectory.points[i].q).translation().eval())tcpReport<<','<<v;tcpReport<<'\n';}
        std::size_t samples=0,collisions=0,limits=0;double maximum=0.0;
        for(std::size_t i=1;i<trajectory.points.size();++i){const auto& a=trajectory.points[i-1].q;const auto& b=trajectory.points[i].q;double delta=0;
            for(int j=0;j<6;++j)delta=std::max(delta,std::abs(b[j]-a[j]));
            const int count=std::max(1,static_cast<int>(std::ceil(delta/0.00025)));
            for(int k=0;k<=count;++k){const double t=static_cast<double>(k)/count;auto q=a;for(int j=0;j<6;++j){q[j]+=t*(b[j]-a[j]);const auto& bound=scene->jointBounds()[j];
                    if(!bound.continuous && (q[j]<bound.lower-1e-9 || q[j]>bound.upper+1e-9))++limits;}
                if(!scene->validateState(q).valid){++collisions;if(collisions==1)std::cout<<"First colliding raw-linear segment (1-based)="<<i<<" sample="<<k<<"/"<<count<<std::endl;}
                const Eigen::Vector3d target=guide[i-1]+t*(guide[i]-guide[i-1]);
                maximum=std::max(maximum,(fk(q).translation()-target).norm());++samples;}
            if(i%500==0)std::cout<<"Independent raw-linear validation "<<i<<'/'<<trajectory.points.size()-1<<" collisions="<<collisions<<" maxMm="<<maximum*1000<<'\n';}
        double endpointPosition=0,endpointRotation=0;
        for(std::size_t i:{std::size_t(0),trajectory.points.size()-1}){const auto actualPose=fk(trajectory.points[i].q);const auto& target=controls[i==0?0:controls.size()-1].tcpPose;
            const Eigen::JacobiSVD<Eigen::Matrix3d> svd(target.linear(),Eigen::ComputeFullU|Eigen::ComputeFullV);const Eigen::Matrix3d rotation=svd.matrixU()*svd.matrixV().transpose();
            endpointPosition=std::max(endpointPosition,(actualPose.translation()-target.translation()).norm());endpointRotation=std::max(endpointRotation,Eigen::AngleAxisd(actualPose.linear().transpose()*rotation).angle());}
        std::ofstream report(reportPath);report<<std::setprecision(15)<<"{\n  \"nodes\": "<<trajectory.points.size()<<",\n  \"rawLinearSamples\": "<<samples
            <<",\n  \"jointStepRad\": 0.00025,\n  \"collidingSamples\": "<<collisions<<",\n  \"jointLimitViolations\": "<<limits
            <<",\n  \"maxOrderedTcpDeviationMm\": "<<maximum*1000<<",\n  \"maxEndpointPositionErrorMm\": "<<endpointPosition*1000
            <<",\n  \"maxEndpointOrientationErrorDeg\": "<<endpointRotation*180.0/3.14159265358979323846
            <<",\n  \"timingValid\": "<<(quality.timingValid?"true":"false")<<",\n  \"velocityViolations\": "<<quality.velocityLimitViolations
            <<",\n  \"accelerationViolations\": "<<quality.accelerationLimitViolations<<",\n  \"durationSeconds\": "<<quality.duration
            <<",\n  \"peakJointVelocityDegPerSec\": "<<(std::isfinite(quality.peakVelocity)?std::to_string(quality.peakVelocity*180/3.14159265358979323846):"null")
            <<",\n  \"peakSampledJointAccelerationDegPerSec2\": "<<(std::isfinite(quality.peakAcceleration)?std::to_string(quality.peakAcceleration*180/3.14159265358979323846):"null")<<"\n}\n";
        std::cout<<"Independent final raw-linear samples="<<samples<<" collisions="<<collisions<<" maxMm="<<maximum*1000
            <<" endpointMm="<<endpointPosition*1000<<" endpointDeg="<<endpointRotation*180/3.14159265358979323846<<'\n';
        require(collisions==0 && limits==0 && maximum<=0.10+1e-9,"Exported raw-linear path is collision-free and obeys actual bounds and 100 mm ordered corridor");
        require(endpointPosition<1e-5 && endpointRotation<1e-5,"Exported start and end preserve original full TCP poses");
        if(validateDynamics)require(quality.timingValid && quality.velocityLimitViolations==0 && quality.accelerationLimitViolations==0,"Exported timing passes sampled velocity and acceleration limits");
    }

    void runTop1Repair(const std::filesystem::path& projectPath, const std::filesystem::path& targetsPath,
        const std::filesystem::path& seedPath, const std::filesystem::path& folder)
    {
        using namespace motion_planning;
        std::filesystem::create_directories(folder);
        simulation_project::ProjectDocument document;std::string error;
        require(simulation_project::loadProjectDocument(projectPath,document,&error),"Top-1 repair project loads");
        ProjectScene actual;actual.setProjectDocument(document,projectPath.parent_path());require(actual.initialize(),"Top-1 actual FK scene");
        const auto names=ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto fk=actual.robotForwardKinematics("ABB4600_urdf",names,true);
        TrajectoryImportOptions importOptions;importOptions.robotId="ABB4600_urdf";
        const auto targets=ProjectTrajectoryImporter::importFile(targetsPath,importOptions);
        const auto input=ProjectCdfJointAngleImporter::importFile(seedPath);
        require(fk && targets.success && input.success,"Top-1 original Cartesian and joint input load");if(!fk || !targets.success || !input.success)return;
        robottrajectory::JointTrajectory seed;
        for(const auto& p:input.points){auto q=p.jointAnglesDegrees;for(auto& v:q)v*=3.14159265358979323846/180;
            seed.points.push_back({p.timeSeconds,ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q),{}, {}});}
        ProjectCdfQpRepairOptions options;options.worldForwardKinematics=fk;options.allowEquivalentEndpointConfigurations=true;
        options.trustRegion=0.02;options.seedCorridor=0.10;options.seedTrackingWeight=0.40;
        for(const auto& p:targets.plan.cartesianControlPoints.points)options.cartesianTargets.push_back(p.tcpPose);
        options.progress=[](const std::string& message){std::cout<<message<<std::endl;};
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,importOptions.robotId,options),"Top-1 production collision setup");
        ProjectCdfQpTrajectoryRepairService service;
        const auto started=std::chrono::steady_clock::now();
        auto result=service.repair(document,projectPath.parent_path(),importOptions.robotId,names,seed,options);
        const auto save=[&](const robottrajectory::JointTrajectory& trajectory,const std::filesystem::path& path){
            std::ofstream out(path);out<<std::setprecision(15)<<"time_s J1_deg J2_deg J3_deg J4_deg J5_deg J6_deg\n";
            for(const auto& p:trajectory.points){out<<p.time;for(double v:ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(p.q))out<<' '<<v*180/3.14159265358979323846;out<<'\n';}};
        save(result.referenceSeedTrajectory,folder/"chosen_seed.txt");
        for(std::size_t i=0;i<result.stages.size();++i)save(result.stages[i].plan.trajectory,folder/("stage-"+std::to_string(i)+".txt"));
        std::ofstream quality(folder/"quality.csv");quality<<"stage,jointLength,bending,maxCornerDeg,tcpLength,maxTcpDeviationMm,minPhiMm,invalidSegments,elapsedSeconds\n"<<std::setprecision(15);
        for(const auto& stage:result.stages)quality<<'"'<<stage.name<<'"'<<','<<stage.quality.jointLength<<','<<stage.quality.bendingCost<<','<<stage.quality.maximumCornerRadians*180/3.14159265358979323846
            <<','<<stage.quality.tcpLength<<','<<stage.quality.maximumTcpDeviation*1000<<','<<stage.minimumPhi*1000<<','<<stage.invalidSegments<<','<<stage.elapsedSeconds<<'\n';
        for(const auto& d:result.diagnostics)std::cout<<d.code<<": "<<d.message<<'\n';
        std::cout<<"Top-1 complete: seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()
            <<" successIncludingMargin="<<result.success<<" QP rounds="<<result.statistics.iterations<<" invalid="<<result.statistics.invalidSegmentCount<<std::endl;
        require(result.stages.size()==3 && !result.plan.trajectory.empty() && result.statistics.invalidSegmentCount==0,"Top-1 produces a complete collision-validated final trajectory");
        require(result.statistics.iterations==1,"Top-1 runs exactly one main QP round");
        if(result.stages.size()!=3 || result.plan.trajectory.empty())return;
        verifyCdfResult(projectPath,targetsPath,folder/"chosen_seed.txt",folder/"stage-1.txt",folder/"apf-validation.json",false);
        verifyCdfResult(projectPath,targetsPath,folder/"chosen_seed.txt",folder/"stage-2.txt",folder/"final-validation.json",true);
    }

    void refineCdfCheckpoint(const std::filesystem::path& projectPath, const std::filesystem::path& inputPath,
        const std::filesystem::path& tcpCsv, const std::filesystem::path& outputPath)
    {
        using namespace motion_planning;
        using namespace motion_planning::detail;
        simulation_project::ProjectDocument document; std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "Refinement project loads");
        ProjectScene actual; actual.setProjectDocument(document, projectPath.parent_path()); actual.initialize();
        const auto names=ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto fk=actual.robotForwardKinematics("ABB4600_urdf",names,true);
        const auto imported=ProjectCdfJointAngleImporter::importFile(inputPath);
        require(fk && imported.success,"Refinement FK and checkpoint import"); if(!fk || !imported.success)return;
        ApfPath path; robottrajectory::JointTrajectory trajectory;
        for(const auto& p:imported.points){auto q=p.jointAnglesDegrees;for(auto& v:q)v*=3.14159265358979323846/180;
            q=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q);path.push_back(q);trajectory.points.push_back({p.timeSeconds,q,{}, {}});}
        ApfGuidance guide;guide.maxDeviation=0.10;
        guide.position=[fk](const auto& q)->ApfState{const auto p=fk(q).translation().eval();return {p.x(),p.y(),p.z()};};
        std::ifstream csv(tcpCsv);std::string line;std::getline(csv,line);
        while(std::getline(csv,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);double node,time,x,y,z;
            if(row>>node>>time>>x>>y>>z)guide.positions.push_back({x,y,z});}
        require(path.size()>2 && guide.positions.size()==path.size(),"Checkpoint retains explicit ordered Cartesian correspondence");
        if(path.size()<3 || guide.positions.size()!=path.size())return;
        ProjectCdfQpRepairOptions options;std::string detector;
        require(ProjectCdfQpTrajectoryRepairService::ensureCollisionSetup(document,"ABB4600_urdf",options,&detector),"Refinement production collision setup");
        document.collision.detectors.erase(std::remove_if(document.collision.detectors.begin(),document.collision.detectors.end(),
            [&](const auto& d){return d.id!=detector;}),document.collision.detectors.end());
        ProjectPlanningRequest request;request.robotId="ABB4600_urdf";request.jointNames=names;request.start=path.front();request.goal=path.back();
        request.collisionDetectorIds={detector};request.validation.maxJointStep=0.00025;
        auto scene=ProjectPlanningSceneBuilder::build(document,projectPath.parent_path(),request,&error);
        require(bool(scene),"Refinement fresh scene builds");if(!scene)return;
        CdfQueryBatch queries(*scene,document,projectPath.parent_path(),request,0);ProjectCdfQpRepairStatistics stats;
        const auto observations=queries.distances(path,0.0,0.1,stats);double floor=0.01;
        for(const auto& d:observations){if(!d.valid){require(false,"Refinement distance available");return;}floor=std::min(floor,d.phi);}
        const auto parameters=jointPathParameters(guide.positions,0.01);
        ApfOracle oracle;oracle.motionValid=[&](const auto& a,const auto& b){return scene->validateMotion(a,b,request.validation).valid;};
        oracle.pathValid=[&](const auto& candidate,std::size_t begin){
            if(!followsApfPath(candidate,guide,begin))return false;
            const auto samples=queries.distances(candidate,0.0,0.1,stats);
            for(const auto& d:samples)if(!d.valid || d.phi<floor-1e-9)return false;
            return queries.pathValid(candidate,request.validation);};
        oracle.progress=[&](int pass,int total){std::cout<<"Refinement "<<pass<<'/'<<total<<" length="<<jointPathLength(path)<<" bending="<<jointPathBending(path,parameters)<<std::endl;};
        ApfState lower,upper;
        for(const auto& b:scene->jointBounds()) {
            lower.push_back(b.continuous?-std::numeric_limits<double>::infinity():b.lower);
            upper.push_back(b.continuous?std::numeric_limits<double>::infinity():b.upper);
        }
        oracle.distances=[&](const auto& states){const auto observations=queries.distances(states,0.0,0.1,stats);
            std::vector<double> values;for(const auto& v:observations)values.push_back(v.valid?v.phi:-1.0);return values;};
        oracle.motionsValid=[&](const auto& states){const auto observations=queries.motions(states,request.validation);
            std::vector<bool> values;for(const auto& v:observations)values.push_back(v.valid);return values;};
        require(refineApfTcpShape(&path,parameters,lower,upper,oracle,guide,3,1.0),
            "Restored TCP reference shape and projected joint fairing within collision constraints");
        for(std::size_t i=0;i<path.size();++i)trajectory.points[i].q=path[i];
        require(retimeJointTrajectory(trajectory,scene->jointBounds(),options.fallbackMaxVelocity,options.fallbackMaxAcceleration),"Refinement retimes output");
        require(followsApfPath(path,guide) && queries.pathValid(path,request.validation),"Refinement complete path collision and ordered corridor check");
        std::ofstream out(outputPath);out<<std::setprecision(15)<<"time_s J1_deg J2_deg J3_deg J4_deg J5_deg J6_deg\n";
        for(const auto& p:trajectory.points){out<<p.time;const auto q=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(p.q);for(double v:q)out<<' '<<v*180/3.14159265358979323846;out<<'\n';}
        std::cout<<"Refinement final length="<<jointPathLength(path)<<" bending="<<jointPathBending(path,parameters)<<" floor="<<floor<<'\n'<<queries.summary()<<std::endl;
    }

    void verifyCdfPipeline(QApplication& application, const std::filesystem::path& projectPath,
        const std::filesystem::path& jointPath, const std::filesystem::path& folder,
        const std::filesystem::path& targetPath = {}, bool referenceOnly = false, bool shapeCheck = false, bool endpointOrientationOnly = false)
    {
        using namespace robot_qt_viewer;
        std::filesystem::create_directories(folder);
        simulation_project::ProjectDocument document; std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "CDF pipeline project loads");
        RobotViewport viewport; viewport.resize(820, 640);
        require(viewport.loadProjectDocument(document, projectPath.parent_path()), "CDF analysis actual viewport loads");
        viewport.show(); application.processEvents();
        simulation_project::ProjectSession session; session.setDocument(document, projectPath, false, false);
        RobotQtViewerEventHub hub; RobotQtViewerDocumentController documents(session, hub);
        RobotQtViewerSelectionModel selection(hub); RobotQtViewerViewportPreviewState preview(hub);
        RobotQtViewerOperationStatusStore status(hub);
        RobotQtViewerDocumentContext context(session, documents, selection, preview, hub, status);
        OverlayObservingServices services(viewport); context.setMotionPlanningViewport(&services);
        selection.selectRobotLink("ABB4600_urdf", "Link6");
        MotionPlanningEditorWidget widget; MotionPlanningModuleController controller(widget, context);
        hub.subscribe(RobotQtViewerEventKind::ProjectDocumentChanged, &controller, [&](const auto& event) { controller.handleEvent(event); });
        widget.resize(490, 760); widget.showCdfPage(); widget.show();
        qputenv("SMROBOT_FILE_DIALOG_BACKEND", "qt");
        QTimer chooseFile;
        QObject::connect(&chooseFile, &QTimer::timeout, &widget, [&]() {
            for(auto* window : application.topLevelWidgets()) if(auto* dialog = qobject_cast<QFileDialog*>(window)) {
                dialog->selectFile(QString::fromStdWString(jointPath.wstring())); QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            }
        });
        chooseFile.start(10); widget.importCdfJointAnglesRequested(); chooseFile.stop();
        const int inputRows = widget.findChild<QTableWidget*>(QStringLiteral("cdfInitialJointAngles"))->rowCount();
        require(inputRows > 1, "Real Top-1 input imports into CDF");
        const auto shapeInput = motion_planning::ProjectCdfJointAngleImporter::importFile(jointPath);
        const auto shapeNames = motion_planning::ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        const auto actualFk = viewport.robotForwardKinematics("ABB4600_urdf", shapeNames, true);
        std::vector<Eigen::Vector3d> orderedTcpReference;
        std::vector<std::vector<double>> denseOriginalJoints;
        if(actualFk && shapeInput.success) {
            const auto runtimeAngles = [](const auto& point) {
                auto q = point.jointAnglesDegrees;
                for(auto& angle:q) angle *= 3.14159265358979323846/180.0;
                return motion_planning::ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(q);
            };
            denseOriginalJoints.push_back(runtimeAngles(shapeInput.points.front()));
            orderedTcpReference.push_back(actualFk(denseOriginalJoints.front()).translation());
            for(std::size_t i=1;i<shapeInput.points.size();++i) {
                const auto a=runtimeAngles(shapeInput.points[i-1]), b=runtimeAngles(shapeInput.points[i]);
                const Eigen::Vector3d pa=actualFk(a).translation(), pb=actualFk(b).translation();
                double maxDelta=0.0; for(int j=0;j<6;++j) maxDelta=std::max(maxDelta,std::abs(b[j]-a[j]));
                const int intervals=std::max(2,static_cast<int>(std::ceil(maxDelta/0.04)));
                for(int k=1;k<=intervals;++k) {
                    const double t=static_cast<double>(k)/intervals;
                    auto q=a; for(int j=0;j<6;++j) q[j]+=t*(b[j]-a[j]);
                    denseOriginalJoints.push_back(q); orderedTcpReference.push_back(pa+t*(pb-pa));
                }
            }
            double originalMaximum=0.0;
            for(std::size_t i=0;i<denseOriginalJoints.size();++i)
                originalMaximum=std::max(originalMaximum,(actualFk(denseOriginalJoints[i]).translation()-orderedTcpReference[i]).norm());
            std::cout << "Original joint interpolation vs control-point TCP polyline: knots=" << denseOriginalJoints.size()
                << " maximumMm=" << originalMaximum*1000.0 << '\n';
        }
        if(!targetPath.empty()) {
            motion_planning::TrajectoryImportOptions importOptions;
            importOptions.robotId = "ABB4600_urdf";
            const auto targets = motion_planning::ProjectTrajectoryImporter::importFile(targetPath, importOptions);
            const auto input = motion_planning::ProjectCdfJointAngleImporter::importFile(jointPath);
            const auto names = motion_planning::ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
            const auto fk = viewport.robotForwardKinematics("ABB4600_urdf", names, true);
            require(targets.success && input.success && fk && targets.plan.cartesianControlPoints.points.size() == input.points.size(),
                "Reused Top-1 seed corresponds to the imported Cartesian point count");
            if(!targets.success || !input.success || !fk || targets.plan.cartesianControlPoints.points.size() != input.points.size()) return;
            double maximumPosition = 0.0, maximumOrientation = 0.0;
            for(std::size_t i = 0; fk && i < input.points.size() && i < targets.plan.cartesianControlPoints.points.size(); ++i) {
                auto radians = input.points[i].jointAnglesDegrees;
                for(auto& angle : radians) angle *= 3.141592653589793 / 180.0;
                const auto pose = fk(motion_planning::ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(radians));
                const auto& target = targets.plan.cartesianControlPoints.points[i].tcpPose;
                maximumPosition = std::max(maximumPosition, (pose.translation() - target.translation()).norm());
                const Eigen::JacobiSVD<Eigen::Matrix3d> svd(target.linear(), Eigen::ComputeFullU | Eigen::ComputeFullV);
                const Eigen::Matrix3d rotation = svd.matrixU() * svd.matrixV().transpose();
                if(!endpointOrientationOnly || i==0 || i+1==input.points.size())
                    maximumOrientation = std::max(maximumOrientation, std::acos(std::clamp(0.5 * (rotation.transpose() * pose.linear()).trace() - 0.5, -1.0, 1.0)));
            }
            std::cout << "Input Cartesian round trip: maxPositionMm=" << maximumPosition * 1000.0 << " maxOrientationDeg=" << maximumOrientation * 180.0 / 3.141592653589793 << '\n';
            require(maximumPosition < 0.00001 && maximumOrientation < 0.00001, "Top-1 input matches 11111 Cartesian poses in the actual TCP model");
            if(maximumPosition >= 0.00001 || maximumOrientation >= 0.00001) return;
        }
        if(referenceOnly) return;
        const double shapeLimit = widget.findChild<QDoubleSpinBox*>(QStringLiteral("cdfApfTcpDeviation"))->value()/1000.0;
        const auto plansBeforeShape = motion_planning::MotionPlanningProjectStore::plans(context.document());
        require(widget.findChild<QDoubleSpinBox*>(QStringLiteral("cdfApfTcpDeviation"))->value() == 100.0,
            "Production APF TCP maximum deviation defaults to 100 mm");
        QElapsedTimer timer; timer.start(); widget.repairImportedCdfTrajectoryRequested();
        std::cout << "GUI CDF complete after " << timer.elapsed() << " ms\n";
        auto* stages = widget.findChild<QComboBox*>(QStringLiteral("cdfStageSelection"));
        if(shapeCheck && stages && stages->count() != 3) {
            const auto plansAfter = motion_planning::MotionPlanningProjectStore::plans(context.document());
            require(plansAfter.size() == plansBeforeShape.size(), "Failed corridor-constrained APF run publishes no optimization plan");
            QString resultText;
            for(auto* label:widget.findChildren<QLabel*>()) if(label->text().contains(" | Log: ")) resultText=label->text();
            std::cout << "Shape-constrained result: " << resultText.toStdString() << '\n';
            require(resultText.contains(QStringLiteral("%1 mm").arg(shapeLimit*1000.0,0,'f',6)) && resultText.contains("QP/CDF was not started"),
                "Configured corridor failure is explicit; no unconstrained QP fallback");
            widget.cdfAnalysisRequested(); application.processEvents();
            auto* analysis=widget.findChild<QDialog*>(QStringLiteral("cdfTrajectoryAnalysis"));
            auto* table=analysis ? analysis->findChild<QTableWidget*>(QStringLiteral("cdfQualityComparison")) : nullptr;
            require(table && stages->count()==2, "Rejected APF still exposes input and failed APF diagnostics");
            if(table) for(int row=0;row<table->rowCount();++row)
                std::cout << "quality[" << row << "]=" << table->item(row,1)->text().toStdString() << " | " << table->item(row,2)->text().toStdString() << '\n';
            SaveDialogAcceptor acceptor; application.installEventFilter(&acceptor);
            acceptor.path=QString::fromStdWString((folder/"quality.csv").wstring()); widget.exportCdfQualityRequested();
            for(int i=0;i<stages->count();++i) {
                acceptor.path=QString::fromStdWString((folder/("rejected-stage-"+std::to_string(i)+".txt")).wstring()); widget.exportCdfStageRequested(i);
            }
            application.removeEventFilter(&acceptor);
            if(analysis) {
                analysis->resize(1150,950); application.processEvents();
                analysis->grab().save(QString::fromStdWString((folder/"rejected-quality.png").wstring()));
                auto* tabs=analysis->findChild<QTabWidget*>(QStringLiteral("cdfAnalysisTabs")); tabs->setCurrentIndex(4); application.processEvents();
                analysis->grab().save(QString::fromStdWString((folder/"rejected-tcp-projection.png").wstring())); analysis->close();
            }
            return;
        }
        require(stages && stages->count() == 3, "Production CDF exposes all three real stages");
        if(shapeCheck) {
            const auto plans=motion_planning::MotionPlanningProjectStore::plans(context.document());
            const motion_planning::StoredMotionPlan* final=nullptr;
            for(const auto& plan:plans) if(plan.id.find("_cdf_qp_") != std::string::npos) final=&plan;
            require(final && final->trajectory.points.size()==orderedTcpReference.size(), "Shape-constrained output retains reference station correspondence");
            if(final && final->trajectory.points.size()==orderedTcpReference.size()) {
                double maximum=0.0;
                for(std::size_t i=1;i<final->trajectory.points.size();++i) {
                    const auto a=motion_planning::ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(final->trajectory.points[i-1].q);
                    const auto b=motion_planning::ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(final->trajectory.points[i].q);
                    double maxDelta=0.0; for(int j=0;j<6;++j) maxDelta=std::max(maxDelta,std::abs(b[j]-a[j]));
                    const int count=std::max(1,static_cast<int>(std::ceil(maxDelta/0.001)));
                    for(int k=0;k<=count;++k) {
                        const double t=static_cast<double>(k)/count; auto q=a;
                        for(int j=0;j<6;++j) q[j]+=t*(b[j]-a[j]);
                        const Eigen::Vector3d target=orderedTcpReference[i-1]+t*(orderedTcpReference[i]-orderedTcpReference[i-1]);
                        maximum=std::max(maximum,(actualFk(q).translation()-target).norm());
                    }
                }
                std::cout << "Independent full path shape validation: maximumMm=" << maximum*1000.0 << " jointStep=0.001 rad\n";
                require(maximum<=shapeLimit+1e-9, "Independent dense FK validation respects the configured hard TCP corridor");
            }
        }
        require(widget.findChild<QTableWidget*>(QStringLiteral("cdfInitialJointAngles"))->rowCount() == inputRows,
            "Real input table remains unchanged after optimization");
        if(!stages || stages->count() != 3) return;
        widget.cdfAnalysisRequested(); application.processEvents();
        auto* analysis = widget.findChild<QDialog*>(QStringLiteral("cdfTrajectoryAnalysis"));
        auto* quality = analysis ? analysis->findChild<QTableWidget*>(QStringLiteral("cdfQualityComparison")) : nullptr;
        require(quality && quality->columnCount() == 4, "Real CDF comparison opens without blocking");
        if(!quality) return;
        for(int row = 0; row < quality->rowCount(); ++row)
            std::cout << "quality[" << row << "]=" << quality->item(row, 1)->text().toStdString() << " | "
                << quality->item(row, 2)->text().toStdString() << " | " << quality->item(row, 3)->text().toStdString() << '\n';
        require(quality->item(8, 3)->text() == "0" && quality->item(9, 3)->text() == "0" && quality->item(10, 3)->text() == "0" && quality->item(11, 3)->text() == "0",
            "Final real trajectory has zero position/velocity/discrete-acceleration/motion violations");
        require(quality->item(13, 3)->text().toDouble() > 0.0 && quality->item(14, 3)->text().toDouble() >= 0.0,
            "Quality uses actual calibrated world TCP instead of nominal DH");
        analysis->resize(1150, 950); application.processEvents(); analysis->grab().save(QString::fromStdWString((folder / "quality.png").wstring()));
        auto* tabs = analysis->findChild<QTabWidget*>(QStringLiteral("cdfAnalysisTabs"));
        for(int i = 0; i < tabs->count(); ++i) { tabs->setCurrentIndex(i); application.processEvents(); }
        tabs->setCurrentIndex(4); application.processEvents(); analysis->grab().save(QString::fromStdWString((folder / "tcp-projection.png").wstring()));
        tabs->setCurrentIndex(2); application.processEvents(); analysis->grab().save(QString::fromStdWString((folder / "joint-velocity.png").wstring()));
        SaveDialogAcceptor acceptor; application.installEventFilter(&acceptor);
        acceptor.path = QString::fromStdWString((folder / "quality.csv").wstring()); widget.exportCdfQualityRequested();
        for(int i = 0; i < 3; ++i) {
            stages->setCurrentIndex(i); application.processEvents();
            auto* table = widget.findChild<QTableWidget*>(QStringLiteral("cdfStageJointAngles"));
            require(table && table->columnCount() == 8, "Each stage shows six original-sign joint columns");
            widget.applyCdfStagePointRequested(i, 0); application.processEvents();
            require(stages->count() == 3, "Applying a stage point preserves all stage reports");
            acceptor.path = QString::fromStdWString((folder / ("stage-" + std::to_string(i) + ".txt")).wstring());
            widget.exportCdfStageRequested(i);
        }
        application.removeEventFilter(&acceptor);
        const int apfPointCount = quality->item(0, 2)->text().toInt();
        analysis->close();
        // Replay raw APF through the production button (automatically draws TCP).
        stages->setCurrentIndex(1); services.samples = 0;
        auto* play = widget.findChild<QPushButton*>(QStringLiteral("cdfStagePlay")); play->click();
        QElapsedTimer playback; playback.start();
        while(play->text() != QStringLiteral("\u52a8\u6001\u64ad\u653e\u9636\u6bb5\u8f68\u8ff9") && playback.elapsed() < 60000) application.processEvents();
        std::cout << "APF stage replay samples=" << services.samples << " elapsedMs=" << playback.elapsed() << '\n';
        require(services.samples == apfPointCount, "APF stage replay retains every source sample");
        stages->setCurrentIndex(2); widget.playCdfStageRequested(2, 0.1, false);
        QEventLoop loop; QTimer::singleShot(100, &loop, &QEventLoop::quit); loop.exec(); widget.playbackStopRequested();
        require(stages->count() == 3, "Stage playback can stop and restart without losing the report");
        widget.close(); viewport.close();
    }

    void verifyOptimizedPlayback(QApplication& application, const std::filesystem::path& projectPath,
        const std::filesystem::path& trajectoryPath, double previewDuration)
    {
        using namespace robot_qt_viewer;
        using namespace motion_planning;
        simulation_project::ProjectDocument document;
        std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "Playback project loads");
        const auto imported = ProjectCdfJointAngleImporter::importFile(trajectoryPath);
        require(imported.success && !imported.points.empty(), "Optimized joint trajectory imports");
        if(!imported.success || imported.points.empty() || document.robots.empty()) { return; }
        const auto& robot=document.robots.front();
        StoredMotionPlan plan=imported.hasMotionMetadata?imported.plan:StoredMotionPlan{};
        plan.id = "playback_cdf_qp_regression";
        plan.name = plan.id;
        plan.robotId = robot.id;
        plan.jointNames.clear();for(const auto& joint:robot.initialJoints)plan.jointNames.push_back(joint.jointName);
        const bool abb=robot.sourcePath.find("4600")!=std::string::npos || robot.id.find("4600")!=std::string::npos;
        if(abb && imported.hasMotionMetadata) {
            auto metadata=nlohmann::json::parse(plan.optimizationMetadata,nullptr,false);
            if(metadata.is_object() && metadata.value("joint_convention",std::string{})=="runtime") {
                for(auto& p:plan.trajectory.points)p.q=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(p.q);
                metadata["joint_convention"]="irb4600_stored";plan.optimizationMetadata=metadata.dump();
            }
        }
        if(!imported.hasMotionMetadata)for(const auto& point : imported.points) {
            auto q = point.jointAnglesDegrees;
            for(auto& value : q) { value *= 3.141592653589793 / 180.0; }
            plan.trajectory.points.push_back({ point.timeSeconds, std::move(q), {}, {} });
        }
        RobotViewport viewport;
        viewport.resize(800, 600);
        RobotQtViewerViewportProjectState projectState;
        RobotQtViewerDocumentViewportAdapter documentViewport(viewport, projectState);
        require(documentViewport.loadProjectDocument(document, projectPath.parent_path()).success,
            "Real robot viewport loads before committing optimized trajectory");
        viewport.setAttribute(Qt::WA_ShowWithoutActivating);
        viewport.show(); application.processEvents();
        std::cout<<"Playback viewport initially exposed="<<(viewport.windowHandle() && viewport.windowHandle()->isExposed())<<'\n';
        require(viewport.isVisible() && viewport.windowHandle() && viewport.windowHandle()->isExposed(),
            "Actual playback viewport is exposed before frame-handshake checks");
        require(MotionPlanningProjectStore::upsertPlan(document, plan, &error), "Optimized playback plan stores");
        simulation_project::ProjectSession session;
        session.setDocument(document, projectPath, false, false);
        RobotQtViewerEventHub hub;
        RobotQtViewerDocumentController documentController(session, hub);
        RobotQtViewerSelectionModel selection(hub);
        RobotQtViewerViewportPreviewState preview(hub);
        RobotQtViewerOperationStatusStore status(hub);
        RobotQtViewerDocumentContext context(session, documentController, selection, preview, hub, status);
        RobotQtViewerAssemblyViewportAdapter assemblyViewport(viewport, projectState);
        RobotQtViewerSelectionViewportAdapter selectionViewport(viewport);
        RobotQtViewerCollisionViewportAdapter collisionViewport(viewport, projectState);
        RobotQtViewerViewportEventController viewportEvents(selectionViewport, assemblyViewport, collisionViewport, preview, {});
        hub.subscribe(RobotQtViewerEventKind::ViewportPreviewChanged, &viewportEvents,
            [&](const auto& event) { viewportEvents.handleEvent(event); });
        OverlayObservingServices services(viewport);
        context.setMotionPlanningViewport(&services);
        context.setCollisionViewport(&collisionViewport);
        std::string tcpLink;Eigen::Isometry3d linkFromTcp;
        require(resolvePlanningTcp(document,robot.id,tcpLink,linkFromTcp,error),"Playback resolves actual mounted TCP");
        selection.selectRobotLink(QString::fromStdString(robot.id),QString::fromStdString(tcpLink));
        SceneExplorerWidget explorer;
        SceneExplorerModuleController explorerController(explorer, context);
        hub.subscribe(RobotQtViewerEventKind::RobotRuntimeChanged, &explorerController,
            [&](const auto& event) { explorerController.handleEvent(event); });
        explorerController.refreshViewModel();
        explorer.resize(350, 700); explorer.show();
        MotionPlanningEditorWidget widget;
        MotionPlanningModuleController controller(widget, context);
        widget.trajectorySelectionChanged(QString::fromStdString(plan.id));
        widget.resize(480, 700); widget.show();
        auto* continuityPage=widget.findChild<QWidget*>(QStringLiteral("sprayContinuityPage"));
        auto* continuityTabs=widget.findChild<QTabWidget*>();
        require(continuityPage && continuityTabs,"Independent spray continuity page exists");
        if(continuityPage && continuityTabs) {
            for(int i=0;i<continuityTabs->count();++i)
                if(continuityTabs->widget(i)->isAncestorOf(continuityPage))continuityTabs->setCurrentIndex(i);
            require(continuityPage->findChild<QPushButton*>(QStringLiteral("sprayContinuityRun")) &&
                continuityPage->findChild<QPushButton*>(QStringLiteral("sprayContinuityExportJoints")),"Continuity page owns compute and export controls");
            auto* selector=continuityPage->findChild<QComboBox*>(QStringLiteral("sprayContinuityTrajectory"));
            require(selector && selector->currentData().toString()==QString::fromStdString(plan.id),"Continuity page shares the selected document trajectory");
            auto* constraint=continuityPage->findChild<QComboBox*>(QStringLiteral("sprayContinuityConstraint"));
            auto* adjustment=continuityPage->findChild<QDoubleSpinBox*>(QStringLiteral("sprayContinuityAdjustment"));
            require(constraint && constraint->count()==3 && constraint->currentIndex()==1,
                "Surface pivot with original standoff is the explicit default");
            if(constraint && adjustment) {
                constraint->setCurrentIndex(0);
                require(widget.sprayContinuitySettings().poseConstraint==0 && !adjustment->isEnabled(),
                    "Fixed TCP mode disables translation budget without changing geometry in the widget");
                constraint->setCurrentIndex(2);
                require(widget.sprayContinuitySettings().poseConstraint==2 && adjustment->isEnabled(),
                    "Legacy adjustable-standoff mode requires explicit selection");
                constraint->setCurrentIndex(1);
            }
            const auto settings=widget.sprayContinuitySettings();
            require(settings.poseConstraint==1 && settings.maxTiltDegrees==30 && settings.maxAdjustmentMm==30 &&
                std::abs(settings.validationStepDegrees-.057296)<1e-6 && settings.maximumOutputPoints==30000. && !settings.enforceJointLimits,"Continuity page exposes expected tolerances");
            application.processEvents();
            const auto screenshot=qEnvironmentVariable("SPRAY_CONTINUITY_SCREENSHOT");
            if(!screenshot.isEmpty())require(widget.grab().save(screenshot),"Continuity page screenshot saves");
        }
        hub.subscribe(RobotQtViewerEventKind::ViewportPreviewChanged, &controller,
            [&](const auto& event) { controller.handleEvent(event); });
        if(qEnvironmentVariableIntValue("SPRAY_CONTINUITY_RUN_UI")==1) {
            const auto sourceId=plan.id;const auto sourcePoints=plan.trajectory.points;
            QTimer configure;configure.setInterval(50);
            QObject::connect(&configure,&QTimer::timeout,&widget,[&] {
                for(auto* top:application.topLevelWidgets())if(top->objectName()==QStringLiteral("motionPlanningCollisionDialog")) {
                    auto* detectors=top->findChild<QListWidget*>(QStringLiteral("planningCollisionDetectors"));
                    if(detectors)for(int i=0;i<detectors->count();++i)if(detectors->item(i)->flags() & Qt::ItemIsEnabled)
                        detectors->item(i)->setSelected(true);
                    auto* target=top->findChild<QComboBox*>(QStringLiteral("planningCollisionTarget"));
                    if(target)for(int i=1;i<target->count();++i)if(target->itemText(i).startsWith(QStringLiteral("C [")))target->setCurrentIndex(i);
                    auto* enable=top->findChild<QCheckBox*>(QStringLiteral("enablePlanningCollisionQueries"));
                    if(enable)enable->setChecked(true);
                    auto* buttons=top->findChild<QDialogButtonBox*>();
                    if(buttons)buttons->button(QDialogButtonBox::Ok)->click();
                }
            });
            configure.start();widget.sprayContinuityRequested();configure.stop();
            const auto generated=MotionPlanningProjectStore::plans(context.document());
            bool preserved=false,computed=false;
            for(const auto& value:generated) {
                if(value.id==sourceId) {
                    preserved=value.trajectory.points.size()==sourcePoints.size();
                    for(std::size_t i=0;preserved && i<sourcePoints.size();++i)preserved &= value.trajectory.points[i].q==sourcePoints[i].q;
                }
                const auto metadata=nlohmann::json::parse(value.optimizationMetadata,nullptr,false);
                if(value.id!=sourceId && metadata.is_object() && metadata.contains("spray_axis_continuity") &&
                    metadata["spray_axis_continuity"].value("success",false)) {plan=value;computed=true;}
            }
            require(preserved,"GUI compute preserves source trajectory in the document");
            require(computed,"GUI compute commits a new continuously certified trajectory");
            if(!computed)return;
            widget.trajectorySelectionChanged(QString::fromStdString(plan.id));
        }
        widget.findChild<QCheckBox*>(QStringLiteral("endEffectorTraceVisible"))->setChecked(true);
        const auto executionMetadata=nlohmann::json::parse(plan.optimizationMetadata,nullptr,false);
        const bool jointLinear=executionMetadata.is_object() && executionMetadata.value("motion",std::string())=="joint_linear";
        std::size_t observedSegment=0;bool followsJointChords=true;int linearInteriorPoses=0;
        if(jointLinear && plan.trajectory.points.size()>1)services.presentationJointObserver=[&](const std::vector<double>& values) {
            bool onChord=false;
            for(;observedSegment+1<plan.trajectory.points.size();++observedSegment) {
                auto a=plan.trajectory.points[observedSegment].q,b=plan.trajectory.points[observedSegment+1].q;
                if(abb){a=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(a);b=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(b);}
                double denominator=0,numerator=0;
                for(std::size_t j=0;j<a.size();++j){denominator+=(b[j]-a[j])*(b[j]-a[j]);numerator+=(values[j]-a[j])*(b[j]-a[j]);}
                const double u=denominator>1e-24?numerator/denominator:0.;double error=0;
                for(std::size_t j=0;j<a.size();++j)error=std::max(error,std::abs(values[j]-a[j]-u*(b[j]-a[j])));
                if(u>=-1e-9 && u<=1+1e-9 && error<1e-9){onChord=true;if(u>1e-5 && u<1-1e-5)++linearInteriorPoses;break;}
            }
            if(!onChord && followsJointChords)std::cout<<"First display outside joint chord, search index="<<observedSegment<<'\n';
            followsJointChords &= onChord;
        };
        const bool cartesianLinear=CartesianMotionSampler::isMoveL(plan);
        bool followsMoveL=true;int moveLInteriorPoses=0;double maximumMoveLPosition=0,maximumMoveLAngle=0;
        const auto playbackFk=viewport.robotForwardKinematics(QString::fromStdString(robot.id),plan.jointNames,true);
        std::vector<Eigen::Isometry3d> moveLPoses;
        if(cartesianLinear && playbackFk) {
            for(const auto& point:plan.trajectory.points)moveLPoses.push_back(playbackFk(abb?ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(point.q):point.q));
            services.presentationJointObserver=[&](const std::vector<double>& values) {
                const auto pose=playbackFk(values);bool onMoveL=false;
                for(;observedSegment+1<moveLPoses.size();++observedSegment) {
                    const auto& a=moveLPoses[observedSegment];const auto& b=moveLPoses[observedSegment+1];
                    const Eigen::Vector3d dp=b.translation()-a.translation();
                    const Eigen::Quaterniond qa(a.linear()),qb(b.linear()),actual(pose.linear());
                    const double angle=qa.angularDistance(qb);
                    const double u=dp.squaredNorm()>1e-20?dp.dot(pose.translation()-a.translation())/dp.squaredNorm():
                        (angle>1e-10?qa.angularDistance(actual)/angle:0.);
                    if(u< -1e-8 || u>1+1e-8)continue;
                    const double position=(pose.translation()-a.translation()-u*dp).norm();
                    const double orientation=qa.slerp(std::clamp(u,0.,1.),qb).angularDistance(actual);
                    if(position<2e-7 && orientation<2e-6) {
                        maximumMoveLPosition=std::max(maximumMoveLPosition,position);maximumMoveLAngle=std::max(maximumMoveLAngle,orientation);
                        if(u>1e-5 && u<1-1e-5)++moveLInteriorPoses;onMoveL=true;break;
                    }
                }
                followsMoveL &= onMoveL;
            };
        }
        const bool queriesBefore = collisionViewport.collisionQueriesEnabled();
        int reloads = 0, clears = 0;
        QObject::connect(&viewport, &RobotViewport::robotLinksAvailable, &widget,
            [&](const auto&, const auto&, const auto&, const auto&, const auto&, const auto&) { ++reloads; });
        QElapsedTimer elapsed;
        elapsed.start();
        qint64 lastBeat = 0, maxBeatGap = 0, lastFrame = -1;
        std::vector<qint64> frameGaps;
        int presentedPoses = 0;
        quint64 lastPresentedTicket = 0;
        QObject::connect(&viewport, &QOpenGLWidget::frameSwapped, &widget, [&]() {
            if(services.lastTicket != lastPresentedTicket && services.isFramePresented(services.lastTicket)) {
                lastPresentedTicket = services.lastTicket;
                ++presentedPoses;
            }
            const auto now = elapsed.elapsed();
            if(services.samples > 0 && services.samples < static_cast<int>(plan.trajectory.points.size())) {
                if(lastFrame >= 0) { frameGaps.push_back(now - lastFrame); }
                lastFrame = now;
            }
        });
        QEventLoop loop;
        QTimer heartbeat;
        heartbeat.setInterval(20);
        QObject::connect(&heartbeat, &QTimer::timeout, &widget, [&]() {
            const auto now = elapsed.elapsed();
            maxBeatGap = std::max(maxBeatGap, now - lastBeat);
            lastBeat = now;
            if(clears < 20 && services.samples > 0) {
                preview.clearTaskPreview(QStringLiteral("optimizedPlaybackSwitch"));
                ++clears;
            }
            if((services.samples >= static_cast<int>(plan.trajectory.points.size()) &&
                services.isFramePresented(services.lastTicket) &&
                widget.findChild<QPushButton*>(QStringLiteral("plotSprayMeasurements"))->isEnabled()) || now > 600000) { loop.quit(); }
        });
        // Exclude independent collision scene construction from the playback heartbeat metric.
        widget.playbackRequested(previewDuration);
        elapsed.restart();
        heartbeat.start();
        loop.exec();
        heartbeat.stop();
        QString summary;
        for(auto* label : widget.findChildren<QLabel*>()) {
            if(label->text().contains("Collision states:")) { summary = label->text(); }
        }
        std::sort(frameGaps.begin(), frameGaps.end());
        const auto p95FrameMs = frameGaps.empty() ? 0 : frameGaps[(frameGaps.size() - 1) * 95 / 100];
        std::cout << "Presented frames=" << frameGaps.size() + 1 << " p95FrameMs=" << p95FrameMs
            << " submittedPoses=" << services.frameRequests << " presentedPoses=" << presentedPoses
            << " overwrittenBeforePresentation=" << services.unpresentedOverwrites << '\n';
        require(services.unpresentedOverwrites == 0 && presentedPoses == services.frameRequests,
            "Every submitted display pose including the endpoint is presented without being overwritten");
        require(frameGaps.size() > 20 && p95FrameMs < 100, "Actual viewport continues rendering during optimized playback");
        std::cout << "Optimized playback: points=" << services.samples << " elapsedMs=" << elapsed.elapsed()
            << " maxHeartbeatGapMs=" << maxBeatGap << " reloads=" << reloads << " clears=" << clears
            << " summary=" << summary.toStdString() << '\n';
        require(services.samples == static_cast<int>(plan.trajectory.points.size()), "Every optimized trajectory point is played");
        if(jointLinear) {
            require(followsJointChords,"Every presented pose lies on the certified joint chords");
            if(plan.trajectory.points.size()<1000)require(linearInteriorPoses>0,"Joint-linear playback presents interpolated poses");
            std::cout<<"Presented joint-linear interior poses="<<linearInteriorPoses<<'\n';
        }
        if(cartesianLinear) {
            require(bool(playbackFk) && followsMoveL,"Every displayed pose follows the actual TCP MoveL line and orientation slerp");
            if(plan.trajectory.points.size()<1000)require(moveLInteriorPoses>0,"MoveL playback presents non-node Cartesian poses");
            std::cout<<"MoveL displayed interior poses="<<moveLInteriorPoses<<" positionErrorM="<<maximumMoveLPosition<<" angleErrorRad="<<maximumMoveLAngle<<'\n';
        }
        const auto collisionCounts = QRegularExpression(QStringLiteral("Collision states: (\\d+) / (\\d+)")).match(summary);
        require(collisionCounts.hasMatch() && collisionCounts.captured(2).toInt() >= services.samples,
            "Every optimized point and execution interior retains independent collision validation");
          require(collisionCounts.hasMatch() && collisionCounts.captured(1).toInt()==0,
              "Actual optimized playback has zero robot/tool collision samples");
        require(reloads == 0 && clears == 20, "Real optimized playback survives twenty mode preview clears");
        require(collisionViewport.collisionQueriesEnabled() == queriesBefore, "Playback preserves user viewport query settings");
          const auto expected = abb?ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(plan.trajectory.points.back().q):plan.trajectory.points.back().q;
        bool finalPoseMatches = true;
        for(std::size_t i = 0; i < expected.size(); ++i) {
              finalPoseMatches = finalPoseMatches && std::abs(viewport.robotJointValue(QString::fromStdString(robot.id),
                QString::fromStdString(plan.jointNames[i])) - expected[i]) < 1e-9;
        }
          require(finalPoseMatches, "Optimized playback reaches the actual robot's final encoder pose");
        widget.playbackStopRequested();
        viewport.close();
    }

    void verifyConfigurationPlot(QApplication& application, const QString& screenshot = {})
    {
        QVector<QVector<int>> sequences(3, QVector<int>(749, 4));
        sequences[1][275] = 7;
        for(int i = 276; i < 325; ++i) { sequences[2][i] = 2; }
        QPointer<ConfigurationSelectionDialog> dialog = new ConfigurationSelectionDialog(sequences, 1);
        dialog->show(); application.processEvents();
        auto* page = dialog->findChild<QWidget*>(QStringLiteral("configurationGlobalPage"));
        auto* ranks = page->findChild<QListWidget*>(QStringLiteral("configurationRanks"));
        auto* first = page->findChild<QSpinBox*>(QStringLiteral("configurationFirstPoint"));
        auto* last = page->findChild<QSpinBox*>(QStringLiteral("configurationLastPoint"));
        auto* separate = page->findChild<QCheckBox*>(QStringLiteral("configurationSeparate"));
        auto* summary = page->findChild<QLabel*>(QStringLiteral("configurationSummary"));
        require(dialog->sequences() == sequences && dialog->selectedRanks() == QVector<int>({0, 1}),
            "Plot retains all 749 samples including an isolated one-point difference");
        page->findChild<QPushButton*>(QStringLiteral("configurationSelectNone"))->click();
        require(dialog->selectedRanks().isEmpty(), "All ranks can be unchecked");
        ranks->item(1)->setCheckState(Qt::Checked); ranks->item(2)->setCheckState(Qt::Checked);
        require(dialog->selectedRanks() == QVector<int>({1, 2}), "Arbitrary nonadjacent-to-first ranks can be compared");
        page->findChild<QPushButton*>(QStringLiteral("configurationSelectAll"))->click();
        require(dialog->selectedRanks().size() == 3 && summary->text().contains(QStringLiteral("50")),
            "Full-range comparison finds isolated and 49-point differences");
        first->setValue(276); last->setValue(276);
        require(summary->text().contains(QStringLiteral("1 ")) && first->value() == last->value(),
            "Single-point range remains inspectable");
        auto* plot = page->findChild<QWidget*>(QStringLiteral("configurationSelectionPlot"));
        require(!plot->grab().isNull(), "Single-point plot renders");
        first->setValue(300);
        require(last->value() == 300, "Range endpoints stay ordered when start moves beyond end");
        last->setValue(100);
        require(first->value() == 100, "Range endpoints stay ordered when end moves before start");
        page->findChild<QPushButton*>(QStringLiteral("configurationFullRange"))->click();
        separate->setChecked(true); application.processEvents();
        require(first->value() == 1 && last->value() == 749 && plot->minimumHeight() == 540,
            "Full range and separate rows retain the complete source point numbering");
        if(!screenshot.isEmpty()) { require(dialog->grab().save(screenshot), "Save configuration comparison screenshot"); }
        separate->setChecked(false);
        require(plot->minimumHeight() == 360 && !plot->grab().isNull(), "Overlay mode renders all selected series");
        dialog->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        require(dialog.isNull(), "Plot dialog is released on close");
        QPointer<ConfigurationSelectionDialog> single = new ConfigurationSelectionDialog({{8}}, 0);
        single->show(); application.processEvents();
        require(!single->grab().isNull() && single->sequences()[0][0] == 8, "One-point trajectory renders its original IK number");
        single->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    void verifyGraphResultTabs(QApplication& application, const QString& screenshot = {})
    {
        robot_qt_viewer::ThemeManager::apply(application, robot_qt_viewer::ThemeKind::Modern);
        MotionPlanningEditorWidget widget;
        QVector<QStringList> globalRows, startRows;
        QVector<QVector<QStringList>> paths(8);
        for(int branch = 1; branch <= 8; ++branch) {
            const auto label = QStringLiteral("B%1 \u80a9%2 / \u8098%3 / \u8155%4").arg(branch)
                .arg(branch <= 4 ? "+" : "-").arg((branch - 1) % 4 < 2 ? "+" : "-")
                .arg(branch % 2 ? "+" : "-");
            startRows.push_back({QString::number(branch), "1", branch == 1 ? "1" : ">30",
                QString::number(148.219446481662 + branch), "749", "1", label, label});
            for(int point = 1; point <= 749; ++point) {
                paths[branch - 1].push_back({QString::number(point), QString::number(point * 0.02, 'f', 6),
                    QString::number(branch), QStringLiteral("97.440865, 1.199759, 33.972933, 97.983219, -105.415134, -24.306064"),
                    QStringLiteral("0, 0, 0, 0, 0, 0"), label});
            }
        }
        for(int rank = 1; rank <= 30; ++rank) {
            globalRows.push_back({QString::number(rank), QString::number(148.219446481662 + rank),
                "749", "1", "1", startRows.front()[6], startRows.front()[7]});
        }
        widget.setMultiIkPoints({}, QStringLiteral("Complete multi IK fixture"), true);
        widget.setLayeredGraphResults(globalRows, QStringLiteral("Top-M=30, per-start K=1"), startRows);
        auto* tabs = widget.findChild<QTabWidget*>(QStringLiteral("layeredGraphResultTabs"));
        auto* starts = widget.findChild<QTableWidget*>(QStringLiteral("layeredGraphStartResults"));
        auto* details = widget.findChild<QTableWidget*>(QStringLiteral("layeredGraphPath"));
        widget.resize(440, 800); widget.show(); application.processEvents();
        int notifications = 0;
        QObject::connect(&widget, &MotionPlanningEditorWidget::layeredGraphSelectionChanged, &widget,
            [&](int row, bool byStart) {
                ++notifications;
                std::cout << "Populate 749-point details: page=" << byStart << ", row=" << row << '\n';
                QElapsedTimer timer; timer.start();
                widget.setLayeredGraphPath(paths[byStart ? row : 0]);
                std::cout << "Detail update: " << timer.elapsed() << " ms\n";
                require(timer.elapsed() < 5000, "Full trajectory detail update stays responsive");
            });
        tabs->setCurrentIndex(1); application.processEvents();
        require(notifications == 1 && details->rowCount() == 749 && starts->rowCount() == 8,
            "Switch to per-start K=1 fills all 749 detail rows exactly once");
        for(int row = 1; row < 8; ++row) {
            starts->selectRow(row); application.processEvents();
            require(details->item(748, 2)->text() == QString::number(row + 1) &&
                details->item(748, 5)->text() == startRows[row][6], "Selected start retains candidate and branch through last point");
        }
        for(int width : {380, 540, 440}) {
            widget.resize(width, 800);
            tabs->setCurrentIndex(0); application.processEvents();
            tabs->setCurrentIndex(1); application.processEvents();
        }
        require(notifications == 14 && starts->currentRow() == 7 && details->item(748, 2)->text() == "8",
            "Repeated tab switching retains the selected starting configuration");
        bool used = false;
        QObject::connect(&widget, &MotionPlanningEditorWidget::useLayeredGraphResultRequested, &widget,
            [&](int row, bool byStart) { used = row == 7 && byStart; });
        widget.findChild<QPushButton*>(QStringLiteral("useLayeredGraphResult"))->click();
        require(used, "Use-as-initial-result still refers to the selected per-start trajectory");
        if(!screenshot.isEmpty()) { require(tabs->grab().save(screenshot), "Save per-start results table screenshot"); }
        widget.close();
    }

    void verifyIkController(QApplication& application, const std::filesystem::path& projectPath,
        const std::filesystem::path& trajectoryPath)
    {
        using namespace motion_planning;
        simulation_project::ProjectDocument document;
        std::string error;
        require(simulation_project::loadProjectDocument(projectPath, document, &error), "UI IK project loads");
        TrajectoryImportOptions importOptions;
        importOptions.robotId = "ABB4600_urdf";
        auto imported = ProjectTrajectoryImporter::importFile(trajectoryPath, importOptions);
        if(!imported.success) { require(false, "UI IK fixture imports"); return; }
        if(imported.plan.cartesianControlPoints.points.size() > 6) {
            imported.plan.cartesianControlPoints.points.resize(6);
        }
        require(MotionPlanningProjectStore::upsertPlan(document, imported.plan, &error), "UI stores imported plan");
        simulation_project::ProjectSession session;
        session.setDocument(document, projectPath, false, false);
        robot_qt_viewer::RobotQtViewerEventHub hub;
        robot_qt_viewer::RobotQtViewerDocumentController documentController(session, hub);
        robot_qt_viewer::RobotQtViewerSelectionModel selection(hub);
        robot_qt_viewer::RobotQtViewerViewportPreviewState preview(hub);
        robot_qt_viewer::RobotQtViewerOperationStatusStore status(hub);
        robot_qt_viewer::RobotQtViewerDocumentContext context(session, documentController, selection, preview, hub, status);
        RobotViewport viewport;
        viewport.resize(640, 480);
        viewport.loadProjectDocument(document, projectPath.parent_path());
        viewport.show();
        application.processEvents();
        const auto names = ProjectTrajectoryInverseKinematics::defaultIrb4600JointNames();
        for(std::size_t i = 0; i < names.size(); ++i) {
            viewport.setRobotJointValue(QStringLiteral("ABB4600_urdf"), QString::fromStdString(names[i]), 0.05 * (i + 1));
        }
        const auto actualFk = viewport.robotForwardKinematics(QStringLiteral("ABB4600_urdf"), names, true);
        require(static_cast<bool>(actualFk), "UI exposes independent actual-model FK");
        if(!actualFk) { return; }
        OverlayObservingServices services(viewport);
        robot_qt_viewer::RobotQtViewerViewportProjectState projectState;
        robot_qt_viewer::RobotQtViewerCollisionViewportAdapter collisionViewport(viewport, projectState);
        context.setCollisionViewport(&collisionViewport);
        collisionViewport.setCollisionQueriesEnabled(false);
        context.setMotionPlanningViewport(&services);
        selection.selectRobotLink(QStringLiteral("ABB4600_urdf"), QStringLiteral("Link6"));
        MotionPlanningEditorWidget widget;
        robot_qt_viewer::MotionPlanningModuleController controller(widget, context);
        widget.trajectorySelectionChanged(QString::fromStdString(imported.plan.id));
        auto* pointsToggle = widget.findChild<QCheckBox*>(QStringLiteral("trajectoryPointsVisible"));
        auto* exportButton = widget.findChild<QPushButton*>(QStringLiteral("exportJointTrajectory"));
        auto* tabs = widget.findChild<QTabWidget*>();
        require(pointsToggle && exportButton && tabs && tabs->widget(0)->isAncestorOf(pointsToggle) &&
            tabs->widget(0)->isAncestorOf(exportButton), "Both new controls belong to Basic Planning");
        if(!pointsToggle || !exportButton) { return; }
        require(!pointsToggle->isChecked() && !services.pointsVisible && services.pointCount > 0,
            "Imported trajectory sphere markers are off by default");
        require(!exportButton->isEnabled(), "Joint export is disabled before IK has joint values");
        pointsToggle->setChecked(true);
        require(services.pointsVisible, "Checkbox enables imported sphere markers");
        auto* allButton = widget.findChild<QPushButton*>(QStringLiteral("solveAllIk"));
        auto* multiPoints = widget.findChild<QComboBox*>(QStringLiteral("multiIkPoint"));
        auto* multiTable = widget.findChild<QTableWidget*>(QStringLiteral("multiIkCandidates"));
        auto* multiPlay = widget.findChild<QPushButton*>(QStringLiteral("multiIkPlay"));
        require(allButton && multiPoints && multiTable && multiPlay && tabs->widget(0)->isAncestorOf(allButton),
            "All IK and multi-solution controls belong to Basic Planning");
        if(!allButton || !multiPoints || !multiTable || !multiPlay) { return; }
        hub.subscribe(robot_qt_viewer::RobotQtViewerEventKind::ProjectDocumentChanged, &controller,
            [&](const auto& event) { controller.handleEvent(event); });
        hub.subscribe(robot_qt_viewer::RobotQtViewerEventKind::ViewportPreviewChanged, &controller,
            [&](const auto& event) { controller.handleEvent(event); });
        // ToolSetup::refresh synchronizes pinned frames after every document change.
        // Reproduce that nested notification as well as the direct apply notification.
        QObject toolSetupRefresh;
        hub.subscribe(robot_qt_viewer::RobotQtViewerEventKind::ProjectDocumentChanged, &toolSetupRefresh,
            [&](const auto&) {
                robot_qt_viewer::RobotQtViewerViewportPreviewPayload frames;
                frames.setPinnedRobotMountFrames = true;
                preview.mutate(frames, QStringLiteral("toolSetupPinnedMountFrames"));
            });
        const auto waitMulti = [&]() {
#ifdef NDEBUG
            constexpr int timeoutMs = 60000;
#else
            constexpr int timeoutMs = 300000;
#endif
            QElapsedTimer timer; timer.start();
            while(allButton->text() != QStringLiteral("\u5168\u9006\u89e3") && timer.elapsed() < timeoutMs) {
                application.processEvents(QEventLoop::AllEvents, 20);
            }
            require(timer.elapsed() < timeoutMs, "Background multi IK finishes without blocking GUI");
        };
        const auto plansBeforeMulti = MotionPlanningProjectStore::plans(session.document());
        widget.multiIkRequested(true, QVector<double>(6, -180), QVector<double>(6, 180), 64);
        waitMulti();
        require(multiPoints->count() == static_cast<int>(imported.plan.cartesianControlPoints.points.size()) &&
            multiTable->rowCount() >= 2 && multiPlay->isEnabled(), "Multi-result shows every point and its candidates");
        if(multiTable->rowCount() >= 2) {
            const int candidate = multiTable->rowCount() - 1;
            std::vector<double> expected;
            for(int j = 0; j < 6; ++j) { expected.push_back(multiTable->item(candidate, j + 1)->text().toDouble()); }
            widget.multiIkApplyRequested(0, candidate);
            bool appliedCorrectly = true;
            for(int j = 0; j < 6; ++j) {
                bool ok = false;
                const double actual = services.robotJointValue(QStringLiteral("ABB4600_urdf"), QString::fromStdString(names[j]), &ok);
                const double sign = j == 0 || j >= 3 ? -1 : 1;
                appliedCorrectly &= ok && std::abs(actual * 180 / 3.141592653589793 - sign * expected[j]) < 0.00001;
            }
            require(appliedCorrectly && multiTable->rowCount() >= 2, "Single candidate applies with original signs and retains multi results");
            if(multiTable->rowCount() < 2) { return; }
            const int candidateCount = multiTable->rowCount();
            const int pointCountBeforeApply = multiPoints->count();
            widget.multiIkApplyRequested(0, 0);
            widget.multiIkApplyRequested(0, candidate);
            require(multiTable->rowCount() == candidateCount && multiPoints->count() == pointCountBeforeApply && multiPlay->isEnabled(),
                "Repeated apply preserves all candidates and playback through nested ToolSetup refresh");
            robot_qt_viewer::RobotQtViewerViewportPreviewPayload displayOnly;
            displayOnly.setToolFrameVisibility = true;
            displayOnly.focusMountFrameLink = true;
            preview.mutate(displayOnly, QStringLiteral("multiIkDisplayRegression"));
            preview.clearTaskPreview(QStringLiteral("multiIkClearFocusRegression"));
            require(multiTable->rowCount() == candidateCount && multiPlay->isEnabled(),
                "Frame visibility and focus changes preserve multi IK results");
            if(multiPoints->count() > 1) {
                multiPoints->setCurrentIndex(1);
                require(multiTable->rowCount() > 0, "Other control points remain selectable after applying a candidate");
                multiPoints->setCurrentIndex(0);
            }
            widget.multiIkSelectRequested(0, candidate, true);
            require(multiTable->item(candidate, 0)->text().contains('*'), "Explicit selected candidate is marked for playback");
            services.samples = 0;
            auto* trace = widget.findChild<QCheckBox*>(QStringLiteral("endEffectorTraceVisible"));
            trace->setChecked(true);
            widget.multiIkPlaybackRequested(0.1);
            QElapsedTimer playbackWait; playbackWait.start();
            while(multiPlay->text() == QStringLiteral("Stop playback") && playbackWait.elapsed() < 10000) {
                application.processEvents(QEventLoop::AllEvents, 20);
            }
            bool missingSavedTcp=false;
            for(auto* label : widget.findChildren<QLabel*>())
                missingSavedTcp |= label->property("error").toBool() &&
                    label->text().contains(QStringLiteral("MoveL playback unavailable:")) &&
                    label->text().contains(QStringLiteral("saved TCP"));
            require(services.samples == multiPoints->count() || (services.samples==0 && missingSavedTcp),
                "Multi playback applies a valid MoveL sequence or explicitly rejects its missing saved TCP without a joint-interpolation fallback");
            const auto afterMulti = MotionPlanningProjectStore::plans(session.document());
            const auto importedAfterMulti=std::find_if(afterMulti.begin(),afterMulti.end(),
                [&](const auto& plan) { return plan.id==imported.plan.id; });
            require(afterMulti.size() == plansBeforeMulti.size() && importedAfterMulti!=afterMulti.end() &&
                importedAfterMulti->trajectory.empty(), "Multi debug playback does not overwrite imported trajectory");
            widget.multiIkPlaybackRequested(10);
            widget.playbackStopRequested();
            require(multiPlay->isEnabled(), "Multi playback can stop and restart");
            auto* graphButton = widget.findChild<QPushButton*>(QStringLiteral("layeredGraphFilter"));
            auto* graphResults = widget.findChild<QTableWidget*>(QStringLiteral("layeredGraphResults"));
            auto* graphPath = widget.findChild<QTableWidget*>(QStringLiteral("layeredGraphPath"));
            auto* graphUse = widget.findChild<QPushButton*>(QStringLiteral("useLayeredGraphResult"));
            auto* graphM = widget.findChild<QSpinBox*>(QStringLiteral("layeredGraphMaxPaths"));
            auto* perStartK = widget.findChild<QSpinBox*>(QStringLiteral("layeredGraphPerStartPaths"));
            auto* resultTabs = widget.findChild<QTabWidget*>(QStringLiteral("layeredGraphResultTabs"));
            auto* startResults = widget.findChild<QTableWidget*>(QStringLiteral("layeredGraphStartResults"));
            require(perStartK && resultTabs && startResults, "Fixed-start controls and second results tab exist");
            if(!perStartK || !resultTabs || !startResults) { return; }
            auto* cdfInitial = widget.findChild<QTableWidget*>(QStringLiteral("cdfInitialJointAngles"));
            require(graphButton && graphResults && graphPath && graphUse && graphM && cdfInitial &&
                tabs->widget(0)->isAncestorOf(graphButton), "Layered graph controls and CDF initial table are available");
            if(!graphButton || !graphResults || !graphPath || !graphUse || !graphM || !cdfInitial) { return; }
            const auto waitGraph = [&]() {
                QElapsedTimer timer; timer.start();
                while(graphButton->text() != QStringLiteral("\u5206\u5c42\u56fe\u7b5b\u9009") && timer.elapsed() < 30000) {
                    application.processEvents(QEventLoop::AllEvents, 20);
                }
                require(timer.elapsed() < 30000, "Background Top-M filtering completes");
            };
            graphButton->click(); waitGraph();
            int expectedPaths = 1;
            for(int i = 0; i < multiPoints->count(); ++i) {
                multiPoints->setCurrentIndex(i);
                expectedPaths = std::min(30, expectedPaths * multiTable->rowCount());
            }
            multiPoints->setCurrentIndex(0);
            require(graphResults->rowCount() == expectedPaths && graphUse->isEnabled(), "Default M=30 returns ranked full sequences");
            if(graphResults->rowCount() < 1) { return; }
            auto* graphView = widget.findChild<QPushButton*>(QStringLiteral("viewConfigurationSelection"));
            require(graphView && graphView->isEnabled(), "Configuration plot button is enabled for ranked results");
            if(!graphView) { return; }
            graphView->click(); application.processEvents();
            QPointer<ConfigurationSelectionDialog> comparison = widget.findChild<ConfigurationSelectionDialog*>();
            require(comparison && comparison->isVisible(), "Configuration plot opens from Basic Planning");
            if(!comparison) { return; }
            auto* branchView = comparison->findChild<QWidget*>(QStringLiteral("configurationGlobalPage"))
                ->findChild<QCheckBox*>(QStringLiteral("configurationByBranch"));
            require(branchView && branchView->isEnabled() && branchView->isChecked(),
                "Production graph comparison defaults to fixed geometric branch categories");
            require(graphPath->columnCount() == 6 && graphResults->columnCount() == 7 &&
                startResults->columnCount() == 8 && graphPath->item(0, 5)->text().startsWith(QStringLiteral("B")),
                "Both result tabs and per-point details show shoulder/elbow/wrist labels");
            branchView->setChecked(false);
            require(!comparison->grab().isNull(), "Candidate/turn view remains available");
            branchView->setChecked(true);
            const auto branchScreenshot = qEnvironmentVariable("SMROBOT_BRANCH_SCREENSHOT");
            if(!branchScreenshot.isEmpty()) {
                require(comparison->grab().save(branchScreenshot), "Save production branch comparison screenshot");
                require(multiTable->grab().save(branchScreenshot + QStringLiteral(".table.png")), "Save production IK table screenshot");
            }
            bool plotMatches = comparison->sequences().size() == graphResults->rowCount();
            for(int rank = 0; plotMatches && rank < graphResults->rowCount(); ++rank) {
                graphResults->selectRow(rank);
                const auto& sequence = comparison->sequences()[rank];
                plotMatches &= sequence.size() == graphPath->rowCount();
                for(int i = 0; plotMatches && i < sequence.size(); ++i) {
                    plotMatches &= sequence[i] == graphPath->item(i, 2)->text().toInt();
                }
            }
            require(plotMatches, "Every plotted rank/point equals the corresponding domain-backed path detail");
            const int startCount = multiTable->rowCount();
            require(startResults->rowCount() == startCount && perStartK->value() == 1,
                "Default K=1 computes one complete optimum for every actual start candidate");
            auto* plotTabs = comparison->findChild<QTabWidget*>(QStringLiteral("configurationSelectionTabs"));
            require(plotTabs && plotTabs->count() == 2 && comparison->sequences(1).size() == startCount &&
                comparison->selectedRanks(1).size() == startCount, "Plot has both categories and initially selects every start optimum");
            resultTabs->setCurrentIndex(1); graphView->click();
            require(plotTabs && plotTabs->currentIndex() == 1, "Plot follows the selected results category");
            bool startMatches = true, globalRankMatches = true, startCdfMatches = true;
            for(int row = 0; row < startResults->rowCount(); ++row) {
                startResults->selectRow(row);
                const auto& sequence = comparison->sequences(1)[row];
                startMatches &= sequence.size() == graphPath->rowCount() && sequence.front() == row + 1;
                for(int i = 0; startMatches && i < sequence.size(); ++i) {
                    startMatches &= sequence[i] == graphPath->item(i, 2)->text().toInt();
                }
                int globalRank = -1;
                for(int i = 0; i < comparison->sequences().size(); ++i) {
                    if(comparison->sequences()[i] == sequence) { globalRank = i + 1; break; }
                }
                const auto expectedRank = globalRank > 0 ? QString::number(globalRank) :
                    QStringLiteral(">%1").arg(graphResults->rowCount());
                globalRankMatches &= startResults->item(row, 2)->text() == expectedRank;
                graphUse->click();
                startCdfMatches &= cdfInitial->rowCount() == sequence.size();
                for(int i = 0; startCdfMatches && i < cdfInitial->rowCount(); ++i) {
                    startCdfMatches &= std::abs(cdfInitial->item(i, 1)->text().toDouble() - graphPath->item(i, 1)->text().toDouble()) < 1.0e-5;
                    const auto q = graphPath->item(i, 3)->text().split(',');
                    for(int j = 0; j < 6; ++j) {
                        startCdfMatches &= std::abs(cdfInitial->item(i, j + 2)->text().toDouble() - q[j].trimmed().toDouble()) < 1.0e-5;
                    }
                }
                tabs->setCurrentIndex(0);
            }
            require(startMatches, "Fixed-start plot and table show all original point selections for each distinct start");
            require(globalRankMatches, "Global rank is exact for matching sequences and explicitly bounded otherwise");
            require(startCdfMatches, "Every start optimum transfers original times and all joints to CDF from page two");
            resultTabs->setCurrentIndex(0);

            graphView->click();
            require(widget.findChildren<ConfigurationSelectionDialog*>().size() == 1,
                "Repeated plot button reuses the current result window");
            graphResults->selectRow(std::min(7, graphResults->rowCount() - 1));
            require(graphPath->rowCount() == pointCountBeforeApply, "Selected result shows every original control point");
            std::vector<std::vector<double>> selectedDegrees;
            std::vector<double> selectedTimes;
            for(int i = 0; i < graphPath->rowCount(); ++i) {
                selectedTimes.push_back(graphPath->item(i, 1)->text().toDouble());
                std::vector<double> q;
                for(const auto& value : graphPath->item(i, 3)->text().split(',')) { q.push_back(value.trimmed().toDouble()); }
                selectedDegrees.push_back(q);
            }
            graphUse->click();
            bool cdfMatches = tabs->currentIndex() == 1 && cdfInitial->rowCount() == pointCountBeforeApply;
            for(int i = 0; cdfMatches && i < cdfInitial->rowCount(); ++i) {
                cdfMatches &= std::abs(cdfInitial->item(i, 1)->text().toDouble() - selectedTimes[i]) < 1.0e-5;
                for(int j = 0; j < 6; ++j) {
                    cdfMatches &= std::abs(cdfInitial->item(i, j + 2)->text().toDouble() - selectedDegrees[i][j]) < 1.0e-5;
                }
            }
            require(cdfMatches, "Selected Top-M sequence fills CDF input with original times, degrees and stored IK signs");
            widget.applySelectedCdfJointAnglesRequested(pointCountBeforeApply - 1);
            bool appliedSeed = true;
            for(int j = 0; j < 6; ++j) {
                bool ok = false;
                const double actual = services.robotJointValue(QStringLiteral("ABB4600_urdf"), QString::fromStdString(names[j]), &ok);
                const double sign = j == 0 || j >= 3 ? -1 : 1;
                appliedSeed &= ok && std::abs(actual * 180 / 3.141592653589793 - sign * selectedDegrees.back()[j]) < 0.00001;
            }
            require(appliedSeed && graphUse->isEnabled() && graphResults->rowCount() == expectedPaths,
                "CDF seed applies through existing sign mapping and keeps ranked results available");
            graphM->setValue(3);
            perStartK->setValue(2);
            require(startResults->rowCount() == 0, "Changing either ranking parameter invalidates both categories");
            require(!graphView->isEnabled() && (!comparison || !comparison->isVisible()),
                "Invalidated results disable plotting and close the stale comparison window");
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            require(comparison.isNull(), "Invalidated comparison releases its old snapshot");
            require(graphResults->rowCount() == 0 && !graphUse->isEnabled(), "Changing graph settings invalidates old ranks");
            graphButton->click(); widget.layeredGraphCancelRequested(); waitGraph();
            require(graphResults->rowCount() == 0 && !graphUse->isEnabled(), "Cancelled graph task publishes no partial results");
            graphButton->click(); waitGraph();
            require(graphResults->rowCount() == 3, "Configured M=3 is honored");
            require(startResults->rowCount() == 2 * startCount, "Per-start K=2 is independent of global M=3");
            bool groupOrder = true;
            for(int i = 0; i < startResults->rowCount(); ++i) {
                groupOrder &= startResults->item(i, 0)->text().toInt() == i / 2 + 1 &&
                    startResults->item(i, 1)->text().toInt() == i % 2 + 1;
            }
            require(groupOrder, "Fixed-start results expose unambiguous start number and within-group rank");
            graphView->click(); application.processEvents();
            comparison = widget.findChild<ConfigurationSelectionDialog*>();
            require(comparison && comparison->sequences().size() == 3, "Recomputed results open a fresh plot snapshot");
            robot_qt_viewer::RobotQtViewerViewportPreviewPayload geometryChange;
            geometryChange.previewRobotBaseTransform = true;
            geometryChange.robotBaseRobotId = QStringLiteral("ABB4600_urdf");
            geometryChange.robotBaseTransform.x = 0.1;
            preview.mutate(geometryChange, QStringLiteral("multiIkBaseRegression"));
            require(multiPoints->count() == 0 && multiTable->rowCount() == 0 && !multiPlay->isEnabled(),
                "Actual base transform preview still invalidates multi IK results");
            require(graphResults->rowCount() == 0 && startResults->rowCount() == 0 && graphPath->rowCount() == 0 && !graphUse->isEnabled(),
                "Invalidating source IK also clears ranked paths and disables CDF transfer");
        }
        widget.multiIkRequested(true, QVector<double>(6, -180), QVector<double>(6, 180), 64);
        widget.multiIkCancelRequested(); waitMulti();
        require(multiPoints->count() == 0 && !multiPlay->isEnabled(), "Cancelled job does not publish stale/incomplete results");
        widget.multiIkRequested(true, QVector<double>(6, -180), QVector<double>(6, 180), 64);
        widget.trajectorySelectionChanged(QStringLiteral("missing")); waitMulti();
        require(multiPoints->count() == 0, "Changing source cancels background work and clears results");
        widget.trajectorySelectionChanged(QString::fromStdString(imported.plan.id));
        hub.unsubscribe(&controller);
        hub.unsubscribe(&toolSetupRefresh);
        widget.inverseKinematicsRequested(true);
        require(services.pointsVisible, "Marker preference survives IK and view refresh");
        const auto pointCount = services.pointCount;
        pointsToggle->setChecked(false);
        require(!services.pointsVisible && services.pointCount == pointCount,
            "Unchecking hides markers while preserving imported trajectory geometry");
        const auto plans = MotionPlanningProjectStore::plans(session.document());
        const auto plan = std::find_if(plans.begin(), plans.end(), [&](const StoredMotionPlan& candidate) {
            return candidate.id == imported.plan.id;
        });
        const bool solved = plan != plans.end() &&
            plan->trajectory.points.size() == imported.plan.cartesianControlPoints.points.size();
        if(!solved) for(auto* label : widget.findChildren<QLabel*>())
            if(label->property("error").toBool()) std::cout << "Basic IK diagnostic: " << label->text().toStdString() << std::endl;
        require(solved, "Basic Planning Solve IK and apply stores every solved point");
        if(solved) {
            std::vector<double> applied;
            for(const auto& name : names) {
                bool ok = false;
                applied.push_back(services.robotJointValue(QStringLiteral("ABB4600_urdf"), QString::fromStdString(name), &ok));
                require(ok, "UI applied joint is readable");
            }
            const auto expected = ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(plan->trajectory.points.front().q);
            bool signsPreserved = applied.size() == expected.size();
            for(std::size_t i = 0; i < applied.size(); ++i) { signsPreserved = signsPreserved && std::abs(applied[i] - expected[i]) < 1.0e-12; }
            require(signsPreserved, "UI applies stored IK angles using the unchanged sign mapping");
            require((actualFk(applied).translation() - imported.plan.cartesianControlPoints.points.front().tcpPose.translation()).norm() < 2.0e-6,
                "Basic Planning apply aligns actual TCP with the imported trajectory");
            // Observe actual runtime joints from the production MoveL playback button.
            auto linearPlan = *plan;
            linearPlan.id = "movel-playback-sign-regression";
            linearPlan.trajectory.points.resize(2);
            linearPlan.cartesianControlPoints.points.clear();
            auto runtimeEnd = expected;
            const double increments[] = {0.02, -0.02, 0.015, 0.03, -0.025, 0.04};
            for(std::size_t j = 0; j < runtimeEnd.size(); ++j) runtimeEnd[j] += increments[j];
            linearPlan.trajectory.points.front() = {0., plan->trajectory.points.front().q, {}, {}};
            linearPlan.trajectory.points.back() = {1., ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(runtimeEnd), {}, {}};
            linearPlan.optimizationMetadata = R"({"motion":"cartesian_linear","joint_convention":"irb4600_stored"})";
            // Play a real exported/reimported joint table, including its motion law and exact encoder branch.
            QTemporaryDir jointRoundtripFolder;
            require(jointRoundtripFolder.isValid(), "MoveL joint table roundtrip folder exists");
            const auto jointRoundtripPath = std::filesystem::path(jointRoundtripFolder.path().toStdWString()) / "movel_joint_table.txt";
            std::string jointRoundtripText;
            require(ProjectTrajectoryControlPointEditor::serializeJointTable(linearPlan, jointRoundtripText, &error),
                "MoveL joint table serializes for real GUI playback");
            { std::ofstream file(jointRoundtripPath); file << jointRoundtripText; }
            const auto jointRoundtrip = ProjectTrajectoryImporter::importFile(jointRoundtripPath, importOptions);
            require(jointRoundtrip.success && CartesianMotionSampler::isMoveL(jointRoundtrip.plan) &&
                jointRoundtrip.plan.trajectory.points.front().q == linearPlan.trajectory.points.front().q &&
                jointRoundtrip.plan.trajectory.points.back().q == linearPlan.trajectory.points.back().q,
                "Joint TXT reimport retains exact commands and MoveL before production GUI playback");
            if(jointRoundtrip.success) {
                linearPlan.trajectory = jointRoundtrip.plan.trajectory;
                linearPlan.optimizationMetadata = jointRoundtrip.plan.optimizationMetadata;
            }
            auto linearDocument = session.document();
            const auto flangeFk = viewport.robotForwardKinematics(QStringLiteral("ABB4600_urdf"), names, false);
            const Eigen::Isometry3d linkFromTcp = flangeFk(expected).inverse() * actualFk(expected);
            simulation_project::RobotMountDesc linearMount;
            linearMount.id = "movel_test_mount"; linearMount.robotId = linearPlan.robotId; linearMount.linkName = "Link6";
            linearDocument.robotMounts.push_back(linearMount);
            simulation_project::AttachmentAssetDesc linearTool;
            linearTool.id = "movel_test_tool"; linearTool.assetKind = "tool"; linearTool.visible = false;
            QTemporaryDir linearToolFolder;
            require(linearToolFolder.isValid(), "MoveL test tool folder exists");
            const auto linearToolPath = std::filesystem::path(linearToolFolder.path().toStdWString()) / "tool.stl";
            {
                std::ofstream mesh(linearToolPath);
                mesh << "solid tool\n";
                const double vertices[4][3] = {{0,0,0},{.001,0,0},{0,.001,0},{0,0,.001}};
                const int faces[4][3] = {{0,2,1},{0,1,3},{0,3,2},{1,2,3}};
                for(const auto& face : faces) {
                    mesh << "facet normal 0 0 0\nouter loop\n";
                    for(int v : face) mesh << "vertex " << vertices[v][0] << ' ' << vertices[v][1] << ' ' << vertices[v][2] << '\n';
                    mesh << "endloop\nendfacet\n";
                }
                mesh << "endsolid tool\n";
            }
            linearTool.visualPath = linearToolPath.generic_u8string();
            simulation_project::AttachmentFunctionalFrameDesc linearTcp;
            linearTcp.id = "tcp"; linearTcp.frameType = "tcp"; linearTcp.primary = true;
            linearTcp.assetMountToFrame.x = linkFromTcp.translation().x();
            linearTcp.assetMountToFrame.y = linkFromTcp.translation().y();
            linearTcp.assetMountToFrame.z = linkFromTcp.translation().z();
            const Eigen::Vector3d zyx = linkFromTcp.linear().eulerAngles(2, 1, 0);
            linearTcp.assetMountToFrame.yaw = zyx.x(); linearTcp.assetMountToFrame.pitch = zyx.y(); linearTcp.assetMountToFrame.roll = zyx.z();
            linearTool.functionalFrames.push_back(linearTcp);
            linearDocument.attachmentAssets.push_back(linearTool);
            simulation_project::MountedAttachmentDesc linearAttachment;
            linearAttachment.id = "movel_test_attachment"; linearAttachment.mountFrameId = linearMount.id;
            linearAttachment.assetId = linearTool.id; linearAttachment.visible = false;
            linearDocument.mountedAttachments.push_back(linearAttachment);
            require(MotionPlanningProjectStore::upsertPlan(linearDocument, linearPlan, &error), "MoveL playback fixture stores");
            session.setDocument(linearDocument, projectPath, false, false);
            robot_qt_viewer::RobotQtViewerEvent linearChanged;
            linearChanged.kind = robot_qt_viewer::RobotQtViewerEventKind::ProjectDocumentChanged;
            controller.handleEvent(linearChanged);
            auto runtimeTablePlan = linearPlan;
            for(auto& point : runtimeTablePlan.trajectory.points)
                point.q = ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(point.q);
            runtimeTablePlan.optimizationMetadata = R"({"motion":"cartesian_linear","joint_convention":"runtime"})";
            const auto runtimeTablePath = std::filesystem::path(jointRoundtripFolder.path().toStdWString()) / "runtime_joint_table.txt";
            std::string runtimeTableText;
            require(ProjectTrajectoryControlPointEditor::serializeJointTable(runtimeTablePlan, runtimeTableText, &error),
                "Runtime convention TXT serializes for actual GUI import");
            { std::ofstream file(runtimeTablePath); file << runtimeTableText; }
            const auto runtimeTableImport = ProjectTrajectoryImporter::importFile(runtimeTablePath, importOptions);
            const bool previousNativeDialogs = QApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
            SaveDialogAcceptor importAcceptor;
            importAcceptor.path = QString::fromStdWString(runtimeTablePath.wstring());
            application.installEventFilter(&importAcceptor);
            widget.importTrajectoryRequested();
            const auto guiImportedPlans = MotionPlanningProjectStore::plans(session.document());
            const auto guiImported = std::find_if(guiImportedPlans.begin(), guiImportedPlans.end(), [&](const auto& candidate) {
                return runtimeTableImport.success && candidate.id == runtimeTableImport.plan.id;
            });
            require(guiImported != guiImportedPlans.end() && CartesianMotionSampler::isMoveL(*guiImported) &&
                guiImported->trajectory.points.size() == 2 &&
                guiImported->trajectory.points.front().q == linearPlan.trajectory.points.front().q &&
                guiImported->trajectory.points.back().q == linearPlan.trajectory.points.back().q,
                "Actual GUI runtime TXT import converts ABB signs once and preserves exact MoveL commands");
            widget.importCdfJointAnglesRequested();
            widget.applySelectedCdfJointAnglesRequested(1);
            bool cdfRuntimeExact = true;
            for(std::size_t j = 0; j < names.size(); ++j) {
                bool readable = false;
                const double actual = services.robotJointValue(QStringLiteral("ABB4600_urdf"), QString::fromStdString(names[j]), &readable);
                cdfRuntimeExact &= readable && actual == runtimeEnd[j];
            }
            require(cdfRuntimeExact, "Actual GUI CDF runtime TXT import/apply preserves exact endpoint encoder signs");
            application.removeEventFilter(&importAcceptor);
            QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, previousNativeDialogs);
            auto playbackStart=expected,playbackEnd=runtimeEnd;
            playbackStart.back()+=4*std::acos(-1.);playbackEnd.back()+=4*std::acos(-1.);
            auto basicPlaybackPlan=linearPlan;
            basicPlaybackPlan.trajectory.points.front().q=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(playbackStart);
            basicPlaybackPlan.trajectory.points.back().q=ProjectTrajectoryInverseKinematics::irb4600RobotSystemJointValues(playbackEnd);
            basicPlaybackPlan.id="ordinary-six-axis-movel-playback";
            basicPlaybackPlan.optimizationMetadata=R"({"joint_convention":"irb4600_stored"})";
            auto basicPlaybackDocument=session.document();
            require(MotionPlanningProjectStore::upsertPlan(basicPlaybackDocument,basicPlaybackPlan,&error),
                "Ordinary six-axis playback fixture stores without a MoveL metadata tag");
            session.setDocument(basicPlaybackDocument,projectPath,false,false);
            controller.handleEvent(linearChanged);
            widget.trajectorySelectionChanged(QString::fromStdString(basicPlaybackPlan.id));
            std::vector<std::vector<double>> displayedJoints;
            services.jointObserver = [&](const auto& q) { displayedJoints.push_back(q); };
            widget.playbackRequested(0.5);
            QEventLoop linearLoop;
            QTimer linearCompletion;
            QElapsedTimer linearElapsed; linearElapsed.start();
#ifdef NDEBUG
            constexpr int linearTimeoutMs = 15000;
#else
            constexpr int linearTimeoutMs = 300000;
#endif
            QObject::connect(&linearCompletion, &QTimer::timeout, &linearLoop, [&]() {
                bool atEnd = !displayedJoints.empty();
                if(atEnd) for(std::size_t j = 0; j < playbackEnd.size(); ++j)
                    atEnd &= std::abs(displayedJoints.back()[j] - playbackEnd[j]) < 1e-9;
                if((atEnd && services.isFramePresented(services.lastTicket)) || linearElapsed.elapsed() > linearTimeoutMs)
                    linearLoop.quit();
            });
            linearCompletion.start(20);
            linearLoop.exec();
            linearCompletion.stop();
            widget.playbackStopRequested();
            services.jointObserver = {};
            const auto fromPose = actualFk(playbackStart), toPose = actualFk(playbackEnd);
            const Eigen::Vector3d chord = toPose.translation() - fromPose.translation();
            bool linearFrames = chord.squaredNorm() > 1e-10;
            int interiorFrames = 0;
            for(const auto& q : displayedJoints) {
                const auto pose = actualFk(q);
                const double fraction = (pose.translation() - fromPose.translation()).dot(chord) / chord.squaredNorm();
                const double f = std::clamp(fraction, 0., 1.);
                const auto rotation = Eigen::Quaterniond(fromPose.linear()).slerp(f, Eigen::Quaterniond(toPose.linear()));
                linearFrames &= fraction >= -1e-6 && fraction <= 1 + 1e-6 &&
                    (pose.translation() - fromPose.translation() - f * chord).norm() < 1e-7 &&
                    Eigen::AngleAxisd(rotation.toRotationMatrix() * pose.linear().transpose()).angle() < 1e-7;
                if(f > 0.05 && f < 0.95) ++interiorFrames;
            }
            bool endpointMatches = !displayedJoints.empty();
            if(endpointMatches) for(std::size_t j = 0; j < playbackEnd.size(); ++j)
                endpointMatches &= std::abs(displayedJoints.back()[j] - playbackEnd[j]) < 1e-9;
            std::cout << "MoveL GUI frames=" << displayedJoints.size() << " interior=" << interiorFrames
                << " linear=" << linearFrames << " endpoint=" << endpointMatches << std::endl;
            if(!linearFrames || interiorFrames < 3 || !endpointMatches) for(auto* label : widget.findChildren<QLabel*>())
                if(label->property("error").toBool()) std::cout << "MoveL GUI diagnostic: " << label->text().toStdString() << std::endl;
            require(linearFrames && interiorFrames >= 3 && endpointMatches,
                "Ordinary six-axis GUI playback accepts out-of-limit turns, follows TCP line/slerp and retains ABB encoder signs");
            QString moveLSummary;
            for(auto* label : widget.findChildren<QLabel*>())
                if(label->text().contains("Collision states:")) moveLSummary=label->text();
            const auto moveLCounts=QRegularExpression(QStringLiteral("Collision states: (\\d+) / (\\d+)")).match(moveLSummary);
            require(moveLCounts.hasMatch() && moveLCounts.captured(2).toInt()>2 &&
                moveLSummary.contains(QStringLiteral("Invalid states: 0.")),
                "Two sparse GUI controls validate actual MoveL interior IK samples as well as endpoints");
            widget.trajectorySelectionChanged(QString::fromStdString(imported.plan.id));
            // A long result must export every stored row, not only sampled table rows.
            auto exportPlan = *plan;
            const auto solvedPoints = exportPlan.trajectory.points;
            exportPlan.trajectory.points.clear();
            for(int i = 0; i < 2001; ++i) {
                auto point = solvedPoints[static_cast<std::size_t>(i) % solvedPoints.size()];
                point.time = i * 0.125;
                exportPlan.trajectory.points.push_back(std::move(point));
            }
            exportPlan.optimizationMetadata = R"({"motion":"cartesian_linear","joint_convention":"irb4600_stored"})";
            auto exportDocument = session.document();
            require(MotionPlanningProjectStore::upsertPlan(exportDocument, exportPlan, &error), "Long export fixture stores");
            session.setDocument(exportDocument, projectPath, false, false);
            robot_qt_viewer::RobotQtViewerEvent changed;
            changed.kind = robot_qt_viewer::RobotQtViewerEventKind::ProjectDocumentChanged;
            controller.handleEvent(changed);
            require(exportButton->isEnabled() && !services.pointsVisible,
                "Export is enabled after IK and marker preference survives document refresh");
            QTemporaryDir exportFolder;
            require(exportFolder.isValid(), "Export fixture directory exists");
            if(!exportFolder.isValid()) { return; }
            const auto output = std::filesystem::path(exportFolder.path().toStdWString()) / "joint_export.txt";
            SaveDialogAcceptor acceptor;
            acceptor.path = QString::fromStdWString(output.wstring());
            qputenv("SMROBOT_FILE_DIALOG_BACKEND", "qt");
            application.installEventFilter(&acceptor);
            const QLocale previousLocale;
            QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
            exportButton->click();
            QLocale::setDefault(previousLocale);
            application.removeEventFilter(&acceptor);
            const auto exported = ProjectCdfJointAngleImporter::importFile(output);
            require(exported.success && exported.points.size() == exportPlan.trajectory.points.size(),
                "TXT exports all 2001 rows and can be read by the degree-format importer");
            require(exported.hasMotionMetadata && CartesianMotionSampler::isMoveL(exported.plan) &&
                exported.plan.jointNames == exportPlan.jointNames,
                "Actual GUI TXT export retains the MoveL law and exact joint names");
            bool valuesMatch = exported.points.size() == exportPlan.trajectory.points.size() &&
                exported.plan.trajectory.points.size() == exportPlan.trajectory.points.size();
            for(std::size_t i = 0; valuesMatch && i < exported.points.size(); ++i) {
                const auto& expectedPoint = exportPlan.trajectory.points[i];
                const auto& actualPoint = exported.points[i];
                valuesMatch = exported.plan.trajectory.points[i].q == expectedPoint.q &&
                    exported.plan.trajectory.points[i].time == expectedPoint.time &&
                    std::abs(expectedPoint.time - actualPoint.timeSeconds) < 0.00000051 &&
                    actualPoint.jointAnglesDegrees.size() == expectedPoint.q.size();
                for(std::size_t j = 0; valuesMatch && j < expectedPoint.q.size(); ++j) {
                    valuesMatch = std::abs(actualPoint.jointAnglesDegrees[j] - expectedPoint.q[j] * 180.0 / 3.141592653589793) < 0.00000051;
                }
            }
            require(valuesMatch, "Export preserves times and stored IK signs, converts radians to degrees with six decimals");
            std::ifstream raw(output);
            std::string header, line;
            for(int i = 0; i < 4 && std::getline(raw, line); ++i) {
                if(!line.empty() && line.back() == '\r') { line.pop_back(); }
                header += line + '\n';
            }
            require(header == "# IK joint angle export\n# Time unit: seconds\n# Joint angle unit: degrees\ntime_s\tJ1_deg\tJ2_deg\tJ3_deg\tJ4_deg\tJ5_deg\tJ6_deg\n",
                "Export header and tab delimiters match the supplied TXT example");

        }
        require(!collisionViewport.collisionQueriesEnabled(),
            "IK/CDF playback does not force duplicate continuous viewport collision queries");
        viewport.close();
    }

    void verifyPlaybackExport(QApplication& application, const std::filesystem::path& folder)
    {
        simulation_project::ProjectSession session;
        auto document = session.document();
        simulation_project::RobotDesc gun;
        gun.id = "gun";
        gun.name = "Spray gun";
        gun.sourceType = "urdf";
        gun.sourcePath = (folder / "gun.urdf").generic_string();
        gun.collisionEnabled = true;
        document.robots.push_back(gun);
        simulation_project::SceneObjectDesc target;
        target.id = "A"; target.name = "A.STL"; target.objectType = "workpiece";
        target.sourcePath = (folder / "surface.stl").generic_string();
        target.visualScale = target.collisionScale = 1.0;
        target.transform.z = 0.3;
        target.collisionEnabled = true;
        document.objects.push_back(target);
        motion_planning::StoredMotionPlan plan;
        plan.id = "spray-test";
        plan.name = "Spray test";
        plan.robotId = "gun";
        plan.jointNames = { "tilt", "slide" };
        constexpr double pi = 3.14159265358979323846;
        plan.trajectory.points = { { 0.0, { 0.0, 0.0 }, {}, {} },
            { 0.5, { pi / 6.0, 3.0 }, {}, {} }, { 1.0, { pi, 6.0 }, {}, {} } };
        std::string error;
        require(motion_planning::MotionPlanningProjectStore::upsertPlan(document, plan, &error),
            "Playback fixture plan stores in project document");
        session.setDocument(std::move(document), folder / "spray.sys.json", false, false);

        robot_qt_viewer::RobotQtViewerEventHub hub;
        robot_qt_viewer::RobotQtViewerDocumentController documentController(session, hub);
        robot_qt_viewer::RobotQtViewerSelectionModel selection(hub);
        robot_qt_viewer::RobotQtViewerViewportPreviewState preview(hub);
        robot_qt_viewer::RobotQtViewerOperationStatusStore status(hub);
        robot_qt_viewer::RobotQtViewerDocumentContext context(
            session, documentController, selection, preview, hub, status);
        RobotViewport viewport;
        viewport.resize(760, 600);
        robot_qt_viewer::RobotQtViewerViewportProjectState projectState;
        robot_qt_viewer::RobotQtViewerDocumentViewportAdapter documentViewport(viewport, projectState);
        documentViewport.loadProjectDocument(session.document(), folder);
        robot_qt_viewer::RobotQtViewerAssemblyViewportAdapter assemblyViewport(viewport, projectState);
        robot_qt_viewer::RobotQtViewerSelectionViewportAdapter selectionViewport(viewport);
        robot_qt_viewer::RobotQtViewerCollisionViewportAdapter collisionViewport(viewport, projectState);
        robot_qt_viewer::RobotQtViewerViewportEventController viewportEvents(
            selectionViewport, assemblyViewport, collisionViewport, preview, {});
        hub.subscribe(robot_qt_viewer::RobotQtViewerEventKind::ViewportPreviewChanged, &viewportEvents,
            [&](const auto& event) { viewportEvents.handleEvent(event); });
        viewport.show();
        application.processEvents();
        viewport.setCameraView(ProjectSceneCameraView::Isometric);
        require(viewport.sprayMeasurement(QStringLiteral("gun")).valid, "Playback viewport loads fixture");
        TraceObservingServices services(viewport);
        context.setMotionPlanningViewport(&services);
        selection.selectRobotLink(QStringLiteral("gun"), QStringLiteral("Link6"));
        MotionPlanningEditorWidget widget;
        robot_qt_viewer::MotionPlanningModuleController controller(widget, context);

        auto* trace = widget.findChild<QCheckBox*>(QStringLiteral("endEffectorTraceVisible"));
        auto* tabs = widget.findChild<QTabWidget*>();
        require(trace && !trace->isChecked() && tabs && tabs->widget(0)->isAncestorOf(trace),
            "Trace checkbox exists on Basic Planning and defaults to off");
        if(!trace) { return; }
        trace->setChecked(true);
        require(services.enabled, "Checkbox enables viewport trace through controller");
        const int resetsBeforePlayback = services.resets;
        qputenv("SMROBOT_FILE_DIALOG_BACKEND", "qt");
        SaveDialogAcceptor acceptor;
        acceptor.path = QString::fromStdWString((folder / "spray-export.txt").wstring());
        application.installEventFilter(&acceptor);
        const auto waitUntil = [](const std::function<bool()>& ready) {
            QElapsedTimer deadline; deadline.start();
            while(!ready() && deadline.elapsed() < 5000) {
                QEventLoop tick;
                QTimer::singleShot(10, &tick, &QEventLoop::quit);
                tick.exec();
            }
            return ready();
        };
        widget.playbackRequested(0.3);
        widget.exportSprayMeasurementsRequested();
        QEventLoop playbackLoop;
        auto* plot = widget.findChild<QPushButton*>(QStringLiteral("plotSprayMeasurements"));
        require(waitUntil([&]() { return plot && plot->isEnabled() && std::filesystem::exists(folder / "spray-export.txt"); }),
            "Playback endpoint is presented and pending export commits within bounded wait");
        application.removeEventFilter(&acceptor);
        require(plot && plot->isEnabled(), "Controller records all playback samples and enables plot");
        widget.plotSprayMeasurementsRequested();
        auto* recordedPlot = widget.findChild<QDialog*>(QStringLiteral("sprayMeasurementPlotDialog"));
        auto* recordedIndex = recordedPlot ? recordedPlot->findChild<QSpinBox*>(QStringLiteral("sprayPlotSampleIndex")) : nullptr;
        auto* recordedInfo = recordedPlot ? recordedPlot->findChild<QLabel*>(QStringLiteral("sprayPlotSelectedInfo")) : nullptr;
        require(recordedIndex && recordedInfo && recordedIndex->maximum() == 3,
            "Real playback supplies each original control point to interactive plot");
        if(recordedIndex && recordedInfo) {
            recordedIndex->setValue(2);
            require(recordedInfo->text().contains("#2") && recordedInfo->text().contains("3000.000") && recordedInfo->text().contains("-30.000000"),
                "Recorded selection shows that point's TCP, rather than current endpoint pose");
        }
        auto* inspector = recordedPlot ? recordedPlot->findChild<RobotViewport*>(QStringLiteral("sprayPlotGeometryView")) : nullptr;
        require(inspector != nullptr, "Spray plot embeds an actual independent RobotViewport");
        if(inspector && recordedIndex) {
            require(waitUntil([&]() { return inspector->environmentSceneInitialized(); }),
                "Inspector creates its real robot/workpiece scene");
            recordedIndex->setValue(1);
            const auto firstTicket=inspector->requestFramePresentation();
            require(waitUntil([&]() { return inspector->isFramePresented(firstTicket); }), "Selected inspection point is presented");
            bool mainOk=false, previewOk=false;
            const double mainSlide=viewport.robotJointValue(QStringLiteral("gun"),QStringLiteral("slide"),&mainOk);
            const double firstSlide=inspector->robotJointValue(QStringLiteral("gun"),QStringLiteral("slide"),&previewOk);
            require(mainOk && previewOk && std::abs(mainSlide-6.)<1e-9 && std::abs(firstSlide)<1e-9,
                "Historical inspection joints do not change main scene endpoint");
            recordedIndex->setValue(2);
            const auto secondTicket=inspector->requestFramePresentation();
            require(waitUntil([&]() { return inspector->isFramePresented(secondTicket); }), "Next historical point is presented");
            require(std::abs(inspector->robotJointValue(QStringLiteral("gun"),QStringLiteral("slide"),&previewOk)-3.)<1e-9 &&
                std::abs(viewport.robotJointValue(QStringLiteral("gun"),QStringLiteral("slide"),&mainOk)-6.)<1e-9,
                "Plot selection updates only the independent robot pose");
            require(!inspector->collisionQueriesEnabled(), "Inspection rendering does not run collision queries");
            const auto beforeOrbit=inspector->grabFramebuffer();
            const QPointF center(inspector->width()*.5,inspector->height()*.5);
            QMouseEvent orbitPress(QEvent::MouseButtonPress,center,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent orbitMove(QEvent::MouseMove,center+QPointF(60,30),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent orbitRelease(QEvent::MouseButtonRelease,center+QPointF(60,30),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(inspector,&orbitPress); QApplication::sendEvent(inspector,&orbitMove); QApplication::sendEvent(inspector,&orbitRelease);
            const auto orbitTicket=inspector->requestFramePresentation();
            require(waitUntil([&]() { return inspector->isFramePresented(orbitTicket); }), "Inspector camera rotation renders");
            const auto afterOrbit=inspector->grabFramebuffer();
            require(!beforeOrbit.isNull() && beforeOrbit!=afterOrbit,"Focused real scene supports camera orbit");
            afterOrbit.save(QStringLiteral("spray_real_scene_orbit.png"));
            inspector->focusSprayInspection();
            const auto focusTicket=inspector->requestFramePresentation();
            require(waitUntil([&]() { return inspector->isFramePresented(focusTicket); }), "Reset focuses current point");
            recordedPlot->grab().save(QStringLiteral("spray_real_scene_dialog.png"));
        }
        auto* sprayTabs = recordedPlot ? recordedPlot->findChild<QTabWidget*>(QStringLiteral("sprayPlotTabs")) : nullptr;
        require(sprayTabs && sprayTabs->count() == 2, "Discrete and continuous recording have separate pages");
        if(sprayTabs && recordedIndex && inspector && recordedInfo) {
            sprayTabs->setCurrentIndex(1);
            require(recordedIndex->maximum() >= 3, "Continuous page retains actual playback frames");
            bool foundIntermediate = false;
            for(int i = 1; i <= recordedIndex->maximum(); ++i) {
                recordedIndex->setValue(i);
                const auto selectedTicket = inspector->requestFramePresentation();
                require(waitUntil([&]() { return inspector->isFramePresented(selectedTicket); }), "Continuous selected pose is presented before inspection");
                bool ok = false;
                const double q = inspector->robotJointValue(QStringLiteral("gun"), QStringLiteral("slide"), &ok);
                if(ok && q > .001 && q < 5.999 && std::abs(q - 3.) > .001) {
                    foundIntermediate = true;
                    const auto actual = inspector->sprayMeasurement(QStringLiteral("gun"), QStringLiteral("A"));
                    require(actual.poseValid && recordedInfo->text().contains(QString::number(q * 1000., 'f', 3)),
                        "Continuous selected point restores actual intermediate TCP, not endpoint interpolation of results");
                    require(actual.valid && recordedInfo->text().contains(QString::number(actual.distanceMeters * 1000., 'f', 6)) &&
                        recordedInfo->text().contains(QString::number(actual.angleDegrees, 'f', 6)),
                        "Continuous distance and angle match independent measurement at the restored pose");
                    require(recordedInfo->text().contains("->") && recordedInfo->text().contains("%"),
                        "Continuous inspection identifies source trajectory interval and fraction");
                    break;
                }
            }
            require(foundIntermediate, "Continuous recording contains poses beyond source control points");
            const auto ticket = inspector->requestFramePresentation();
            require(waitUntil([&]() { return inspector->isFramePresented(ticket); }), "Continuous historical scene is presented");
            recordedPlot->grab().save(QStringLiteral("spray_continuous_scene_dialog.png"));
            sprayTabs->setCurrentIndex(0);
            require(recordedIndex->maximum() == 3 && recordedIndex->value() == 2, "Tab switch preserves independent discrete selection");
        }
        auto* direction = widget.findChild<QComboBox*>(QStringLiteral("sprayDirection"));
        require(direction && direction->currentIndex() == 0, "Direction defaults to backward-compatible TCP Z+");
        if(direction) {
            direction->setCurrentIndex(1);
            require(!plot->isEnabled() && viewport.sprayMeasurement("gun").poseValid &&
                viewport.sprayMeasurement("gun").sprayDirection.z() > .999,
                "Changing to Z- propagates to actual viewport and invalidates previous recording");
            if(recordedInfo) { require(recordedInfo->text().contains("#2"), "Open plot retains its labelled historical snapshot after direction change"); }
            direction->setCurrentIndex(0);
        }
        if(recordedPlot) { recordedPlot->close(); }
        std::ifstream exported(folder / "spray-export.txt");
        std::string line;
        int dataRows = 0, validRows = 0, invalidRows = 0;
        bool hasNegativeThirty = false, hasDirectionMetadata = false;
        while(std::getline(exported, line)) {
            hasDirectionMetadata |= line.find("# spray_direction_tcp=Z+") == 0;
            if(line.empty() || line[0] == '#') { continue; }
            if(line.rfind("index", 0) == 0) { continue; }
            ++dataRows;
            if(line.find("\t1\tOK") != std::string::npos) { ++validRows; }
            if(line.find("\t0\tNo forward intersection") != std::string::npos) { ++invalidRows; }
            hasNegativeThirty = hasNegativeThirty || line.find("-3.0000000000e+01") != std::string::npos;
        }
        require(dataRows == 3 && validRows == 2 && invalidRows == 1,
            "TXT contains one row per joint group with validity status");
        require(hasNegativeThirty && hasDirectionMetadata, "TXT records signed -30 degree sample and TCP direction metadata");
        require(services.samples == 3 && services.resets > resetsBeforePlayback,
            "Playback resets trace and samples every joint group including invalid spray hits");
        application.processEvents();
        const QImage traceImage = viewport.grabFramebuffer();
        int tracePixels = 0;
        for(int y = 0; y < traceImage.height(); ++y) {
            for(int x = 0; x < traceImage.width(); ++x) {
                const QColor c = traceImage.pixelColor(x, y);
                if(c.red() > 200 && c.green() < 110 && c.blue() > 120 && c.blue() < 220) { ++tracePixels; }
            }
        }
        require(tracePixels > 10, "Actual viewport renders the completed magenta TCP trace");
        traceImage.save(QStringLiteral("end_effector_trace.png"));
        if(direction) {
            services.samples = 0;
            widget.playbackRequested(10.0);
            direction->setCurrentIndex(1);
            QTimer::singleShot(60, &playbackLoop, &QEventLoop::quit);
            playbackLoop.exec();
            require(services.samples == 1 && !plot->isEnabled(),
                "Direction change during playback stops sampling and discards mixed-direction results");
            direction->setCurrentIndex(0);
        }
        trace->setChecked(false);
        require(!services.enabled, "Unchecking disables viewport trace");
        trace->setChecked(true);
        auto* measurement = widget.findChild<QCheckBox*>(QStringLiteral("sprayMeasurementEnabled"));
        require(measurement != nullptr, "Measurement toggle is available");
        if(measurement) { measurement->setChecked(false); }
        services.samples = 0;
        const int resetsBeforeReplay = services.resets;
        widget.playbackRequested(0.03);
        QTimer::singleShot(300, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        require(services.samples == 3 && services.resets > resetsBeforeReplay,
            "Replay clears previous trace and records with spray calculation disabled");
        services.samples = 0;
        widget.playbackRequested(10.0);
        widget.playbackStopRequested();
        require(services.samples == 1 && services.enabled, "Early stop retains partial visible trace");
        services.samples = 0;
        widget.playbackRequested(1.0);
        QTimer::singleShot(150, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        const double interpolatedTilt = viewport.robotJointValue("gun", "tilt");
        require(services.samples == 1 && interpolatedTilt > 0.02 && interpolatedTilt < pi / 6.0,
            "Robot moves between sparse source points instead of waiting then jumping at each timer tick");
        widget.playbackStopRequested();
        // A real suspended viewport must stop progression even while timers run.
        services.samples = 0;
        services.lastTicket = 0;
        services.unpresentedOverwrites = 0;
        viewport.setUpdatesEnabled(false);
        widget.playbackRequested(1.0);
        const auto pausedTicket = services.lastTicket;
        QTimer::singleShot(400, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        require(services.samples == 1 && services.lastTicket == pausedTicket &&
            !viewport.isFramePresented(pausedTicket) && std::abs(viewport.robotJointValue("gun", "tilt")) < 1e-12,
            "Suppressed presentation keeps first pose/trace/progress fixed despite active timers");
        viewport.setUpdatesEnabled(true);
        QTimer::singleShot(100, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        const auto resumedTilt = viewport.robotJointValue("gun", "tilt");
        require(viewport.isFramePresented(pausedTicket) && resumedTilt > 0.0 && resumedTilt < 0.13,
            "Resuming presentation advances smoothly without catching up the 400ms pause");
        widget.playbackStopRequested();

        // Hold only the endpoint acknowledgment to check completion/export order.
        if(measurement) { measurement->setChecked(true); }
        services.samples = 0;
        services.lastTicket = 0;
        services.holdAtSamples = 3;
        widget.playbackRequested(0.03);
        require(waitUntil([&]() { return services.samples >= 3; }), "Endpoint source sample arrives within bounded wait");
        const auto finalTicket = services.lastTicket;
        require(services.samples == 3 && plot && !plot->isEnabled(),
            "Processing final source point does not complete playback before final frame acknowledgment");
        services.holdAtSamples = 0;
        require(waitUntil([&]() { return plot && plot->isEnabled(); }), "Endpoint presentation finishes within bounded wait");
        require(plot && plot->isEnabled() && services.lastTicket == finalTicket &&
            viewport.isFramePresented(finalTicket) && services.unpresentedOverwrites == 0,
            "Endpoint presentation completes playback without duplicate source samples or overwritten frames");
        if(measurement) { measurement->setChecked(false); }
        // Long playback exercises the exact preview-clear event path used by MainWindow
        // when changing workbenches. Runtime joints and the existing trace must survive.
        auto longPlan = plan;
        longPlan.trajectory.points.clear();
        constexpr int longCount = 2001;
        for(int i = 0; i < longCount; ++i) {
            longPlan.trajectory.points.push_back({ i * 0.001, { 0.1 + i * 0.0001, i * 0.001 }, {}, {} });
        }
        auto longDocument = session.document();
        require(motion_planning::MotionPlanningProjectStore::upsertPlan(longDocument, longPlan, &error),
            "Long playback fixture stores");
        session.setDocument(longDocument, folder / "spray.sys.json", false, false);
        robot_qt_viewer::RobotQtViewerEvent changed;
        changed.kind = robot_qt_viewer::RobotQtViewerEventKind::ProjectDocumentChanged;
        controller.handleEvent(changed);
        int reloads = 0;
        QObject::connect(&viewport, &RobotViewport::robotLinksAvailable, &widget,
            [&](const auto&, const auto&, const auto&, const auto&, const auto&, const auto&) { ++reloads; });
        services.samples = 0;
        int clears = 0;
        bool posesPreserved = true;
        QElapsedTimer elapsed;
        elapsed.start();
        qint64 lastBeat = 0, maxBeatGap = 0;
        QTimer heartbeat;
        heartbeat.setInterval(10);
        QObject::connect(&heartbeat, &QTimer::timeout, &widget, [&]() {
            const qint64 now = elapsed.elapsed();
            maxBeatGap = std::max(maxBeatGap, now - lastBeat);
            lastBeat = now;
            if(clears < 20) {
                bool ok = false;
                const double before = viewport.robotJointValue("gun", "tilt", &ok);
                preview.clearTaskPreview(QStringLiteral("workbenchSwitchRegression"));
                const double after = viewport.robotJointValue("gun", "tilt");
                posesPreserved = posesPreserved && ok && std::abs(before - after) < 1e-12;
                ++clears;
            }
            if((services.samples >= longCount && services.isFramePresented(services.lastTicket)) ||
                elapsed.elapsed() > 60000) { playbackLoop.quit(); }
        });
        heartbeat.start();
        widget.playbackRequested(1.0);
        playbackLoop.exec();
        heartbeat.stop();
        widget.playbackStopRequested();
        std::cout << "Long playback: points=" << services.samples << " elapsedMs=" << elapsed.elapsed()
                  << " maxHeartbeatGapMs=" << maxBeatGap << " reloads=" << reloads << '\n';
        require(services.samples == longCount && services.completeGroups,
            "All 2001 joint groups reach runtime and TCP sampling without dropping points");
        require(clears == 20 && posesPreserved && reloads == 0,
            "Twenty workbench preview clears preserve runtime poses without reloading the scene");
        require(maxBeatGap < 1000, "GUI heartbeat remains responsive during long playback");
        require(std::abs(viewport.robotJointValue("gun", "slide") - 2.0) < 1e-12,
            "Long playback reaches the final joint pose");
        const double beforeInvalid = viewport.robotJointValue("gun", "slide");
        require(!viewport.setRobotJointValues("gun", { "slide", "missing" }, { 4.0, 0.0 }) &&
            std::abs(viewport.robotJointValue("gun", "slide") - beforeInvalid) < 1e-12,
            "Invalid joint groups fail without partially updating the runtime");
        // An actual document edit must invalidate the snapshot, unlike a mode switch.
        services.samples = 0;
        widget.playbackRequested(20.0);
        controller.handleEvent(changed);
        QTimer::singleShot(100, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        require(services.samples == 1, "Document changes stop playback of the old snapshot");
        // Single-point paths have no next source sample, but still need a timer
        // to complete after their only display frame is presented.
        auto singlePlan = plan;
        singlePlan.trajectory.points.resize(1);
        auto singleDocument = session.document();
        require(motion_planning::MotionPlanningProjectStore::upsertPlan(singleDocument, singlePlan, &error),
            "Single-point playback fixture stores");
        session.setDocument(singleDocument, folder / "spray.sys.json", false, false);
        controller.handleEvent(changed);
        if(measurement) { measurement->setChecked(true); }
        services.samples = 0;
        viewport.setUpdatesEnabled(false);
        widget.playbackRequested(1.0);
        QTimer::singleShot(100, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        require(services.samples == 1 && plot && !plot->isEnabled(),
            "Single-point playback also waits for presentation");
        viewport.setUpdatesEnabled(true);
        QTimer::singleShot(150, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        require(services.samples == 1 && plot && plot->isEnabled() && viewport.isFramePresented(services.lastTicket),
            "Single-point playback completes after presentation without duplicate samples");
        const int resetsBeforeSelection = services.resets;
        widget.trajectorySelectionChanged(QStringLiteral("missing"));
        require(services.resets > resetsBeforeSelection, "Trajectory selection clears previous trace");
        viewport.close();
    }
}

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    if(argc >= 2 && (std::string(argv[1]) == "--cdf-progress" || std::string(argv[1]) == "--cdf-analysis" || std::string(argv[1]) == "--cdf-reference" || std::string(argv[1]) == "--cdf-shape" || std::string(argv[1]) == "--cdf-alternative"))
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    if(argc >= 3 && std::string(argv[1]) == "--telemetry-live") {
        using namespace robot_qt_viewer;
        RobotQtViewerTelemetryAdapter port;
        DigitalTwinTelemetryPanel panel(nullptr);
        DigitalTwinTelemetryController controller(panel, port);
        panel.resize(500, 820); panel.show();
        auto* path = panel.findChild<QLineEdit*>(QStringLiteral("telemetryCsvPath"));
        path->setText(QString::fromLocal8Bit(argv[2])); path->setModified(true);
        panel.findChild<QPushButton*>(QStringLiteral("telemetryStart"))->click();
        QElapsedTimer deadline; deadline.start();
        while(port.snapshot().sequence < 150 && !port.snapshot().fault && deadline.elapsed() < 15000) {
            QEventLoop tick; QTimer::singleShot(10, &tick, &QEventLoop::quit); tick.exec();
        }
        const auto state = port.snapshot();
        panel.render(state);
        auto* tcp = panel.findChild<QTableWidget*>(QStringLiteral("telemetryTcpPose"));
        auto* tcpInfo = panel.findChild<QLabel*>(QStringLiteral("telemetryTcpInfo"));
        panel.findChild<QTabWidget*>(QStringLiteral("telemetryDataTabs"))->setCurrentIndex(1);
        require(state.tcpPoseValid && state.tcpUserFrame == 0 && state.tcpUserTool >= 0 && tcp->rowCount() == 6 &&
            tcpInfo->text().contains(QStringLiteral("UF0")), "Real TCP page receives WORLD XYZ/WPR and active tool metadata");
        panel.grab().save(QStringLiteral("digital_twin_telemetry_live.png"));
        panel.findChild<QPushButton*>(QStringLiteral("telemetryStop"))->click();
        require(state.sequence >= 150 && state.streaming && !state.source.isEmpty() && !state.joints.empty(),
            "Real panel start auto-discovers ROBOGUIDE and records 150 timestamped frames");
        std::cout << "source=" << state.source.toUtf8().constData() << " samples=" << state.sequence
            << " elapsed_ns=" << state.elapsedNs << " error=" << state.diagnostic.toUtf8().constData() << '\n';
        std::cout << "TCP UF=" << state.tcpUserFrame << " UT=" << state.tcpUserTool << " XYZ/WPR=";
        for(double v : state.tcpXyzwpr) std::cout << v << ',';
        std::cout << '\n';
        return failures ? 1 : 0;
    }
    QTimer collisionConditions;
    QObject::connect(&collisionConditions, &QTimer::timeout, &application, [&]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if(!dialog || dialog->objectName() != QStringLiteral("motionPlanningCollisionDialog")) return;
        auto* list = dialog->findChild<QListWidget*>(QStringLiteral("planningCollisionDetectors"));
        auto* target = dialog->findChild<QComboBox*>(QStringLiteral("planningCollisionTarget"));
        int index = target->findData(QStringLiteral("A"));
        if(index < 0 && target->count() > 1) index = 1;
        target->setCurrentIndex(index);
        if(list->count()) {
            list->clearSelection();
            // A partial first detector need not cover mounted tools. Keep all
            // configured enabled conditions in this playback regression.
            for(int i=0;i<list->count();++i)if(list->item(i)->flags() & Qt::ItemIsEnabled)
                list->item(i)->setSelected(true);
        }
        else dialog->findChild<QCheckBox*>(QStringLiteral("createPlanningCollisionDetector"))->setChecked(true);
        dialog->findChild<QCheckBox*>(QStringLiteral("enablePlanningCollisionQueries"))->setChecked(true);
        dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
    });
    collisionConditions.start(20);
    std::cout.setf(std::ios::unitbuf);
    if(argc >= 2 && std::string(argv[1]) == "--configuration-tabs") {
        verifyGraphResultTabs(application, argc >= 3 ? QString::fromLocal8Bit(argv[2]) : QString{});
        return failures ? 1 : 0;
    }
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    QOpenGLContext context;
    context.setShareContext(QOpenGLContext::globalShareContext());
    context.setFormat(format);
    require(context.create() && context.makeCurrent(&surface), "Offscreen OpenGL context available");
    if(failures) { return 1; }
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary fixture directory available");
    if(failures) { return 1; }
    const std::filesystem::path folder(temporary.path().toStdWString());
    writeFixture(folder);
    if(argc >= 2 && std::string(argv[1]) == "--spray-direct-geometry") {
        try { verifyGeometry(folder); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        std::cout << "Direct geometry failures: " << failures << '\n';
        return failures ? 1 : 0;
    }
    if(argc >= 4 && std::string(argv[1]) == "--collision-project") {
        const auto arguments=application.arguments();
        try { inspectProjectCollision(application,std::filesystem::path(arguments[2].toStdWString()),arguments[3].toStdString()); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        return failures ? 1 : 0;
    }
    if(argc >= 4 && std::string(argv[1]) == "--collision-models") {
        const auto arguments=application.arguments();
        try { verifyImportedCollisionModels(application,folder,
            std::filesystem::path(arguments[2].toStdWString()),std::filesystem::path(arguments[3].toStdWString())); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        return failures ? 1 : 0;
    }
    if(argc >= 2 && std::string(argv[1]) == "--collision-config-nearest") {
        try { verifyCollisionConfigNearest(application,folder); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        return failures ? 1 : 0;
    }
    if(argc >= 3 && std::string(argv[1]) == "--a-workpiece") {
        try { verifyAWorkpiece(folder,std::filesystem::u8path(argv[2])); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        std::cout << "A workpiece failures: " << failures << '\n';
        return failures ? 1 : 0;
    }

    if(argc >= 6 && std::string(argv[1]) == "--cdf-top1-refined") {
        runTop1Repair(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]),std::filesystem::u8path(argv[5]));
        return failures?1:0;
    }
    if(argc >= 6 && std::string(argv[1]) == "--refine-cdf-checkpoint") {
        refineCdfCheckpoint(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]),std::filesystem::u8path(argv[5]));
        return failures?1:0;
    }
    if(argc >= 7 && std::string(argv[1]) == "--validate-cdf-result") {
        verifyCdfResult(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]),
            std::filesystem::u8path(argv[5]),std::filesystem::u8path(argv[6]),!(argc>=8 && std::string(argv[7])=="--geometry-only"));
        return failures?1:0;
    }
    if(argc >= 5 && std::string(argv[1]) == "--apf-candidates") {
        inspectApfCandidates(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]));
        return failures?1:0;
    }
    if(argc >= 8 && std::string(argv[1]) == "--apf-window") {
        verifyApfWindow(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]),
            std::stoull(argv[5]),std::stoull(argv[6]),std::stoull(argv[7]),argc>=9?std::filesystem::u8path(argv[8]):std::filesystem::path{});
        return failures?1:0;
    }
    if(argc >= 3 && std::string(argv[1]) == "--cdf-progress") {
        verifyCdfProgress(application, std::filesystem::u8path(argv[2]), folder);
        return failures ? 1 : 0;
    }
    if(argc >= 6 && std::string(argv[1]) == "--cdf-reference") {
        verifyCdfPipeline(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]), std::filesystem::u8path(argv[4]), std::filesystem::u8path(argv[5]), true);
        return failures ? 1 : 0;
    }
    if(argc >= 6 && std::string(argv[1]) == "--cdf-alternative") {
        verifyCdfPipeline(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]), std::filesystem::u8path(argv[4]), std::filesystem::u8path(argv[5]), false, true, true);
        return failures ? 1 : 0;
    }
    if(argc >= 5 && std::string(argv[1]) == "--cdf-shape") {
        verifyCdfPipeline(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]), std::filesystem::u8path(argv[4]), argc >= 6 ? std::filesystem::u8path(argv[5]) : std::filesystem::path{}, false, true);
        return failures ? 1 : 0;
    }
    if(argc >= 5 && std::string(argv[1]) == "--cdf-analysis") {
        verifyCdfPipeline(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]), std::filesystem::u8path(argv[4]), argc >= 6 ? std::filesystem::u8path(argv[5]) : std::filesystem::path{});
        return failures ? 1 : 0;
    }
    if(argc >= 4 && std::string(argv[1]) == "--optimized-playback") {
        verifyOptimizedPlayback(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]),
            argc >= 5 ? std::stod(argv[4]) : 5.0);
        return failures ? 1 : 0;
    }
    verifyCdfAnalysisControls();
    verifyTelemetryPanel(application);
    verifyPlaybackTimeline();
    verifyModelIk();
    verifyMultiIkDomain();
    if(argc >= 2 && std::string(argv[1]) == "--configuration-plot") {
        verifyConfigurationPlot(application, argc >= 3 ? QString::fromLocal8Bit(argv[2]) : QString{});
        return failures == 0 ? 0 : 1;
    }
    if(argc >= 4 && std::string(argv[1]) == "--multi-ik-project") {
        verifyConfigurationPlot(application);
        try {
            verifyMultiIkImported(std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]),
                argc >= 5 ? std::filesystem::u8path(argv[4]) : std::filesystem::path("multi_ik.csv"), argc >= 6 ? std::stoi(argv[5]) : 64);
            verifyIkController(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]));
        } catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        return failures ? 1 : 0;
    }
    if(argc >= 4 && std::string(argv[1]) == "--ik-project") {
        const auto report = argc >= 5 ? std::filesystem::u8path(argv[4]) : std::filesystem::path("ik_round_trip.csv");
        try { verifyImportedIk(std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]), report); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        try { verifyIkController(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3])); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        return failures ? 1 : 0;
    }
    try { verifyFanucTelemetryMapping(folder); verifyGeometry(folder); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    verifyPlot(application);
    try { verifyPlaybackExport(application, folder); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::cout << "Spray smoke failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
