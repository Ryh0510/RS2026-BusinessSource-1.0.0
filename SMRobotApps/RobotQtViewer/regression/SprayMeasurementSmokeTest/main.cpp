#include <ProjectScene.h>
#include <MotionPlanningEditorWidget.h>
#include <MotionPlanningModuleController.h>
#include <RobotQtViewerDocumentContext.h>
#include <RobotQtViewerDocumentController.h>
#include <RobotQtViewerEventHub.h>
#include <RobotQtViewerOperationStatus.h>
#include <RobotQtViewerSelectionModel.h>
#include <RobotQtViewerViewportPreviewState.h>
#include <RobotViewport.h>
#include "RobotQtViewerViewportServicesAdapter.h"
#include <MotionPlanningCore/MotionPlanning.h>
#include <ProjectMotionPlanning/TrajectoryImport.h>
#include <ProjectMotionPlanning/CdfJointAngleImport.h>
#include <QLocale>
#include <QTableWidget>
#include <ProjectMotionPlanning/TrajectoryInverseKinematics.h>
#include <SimulationProject/ProjectIo.h>
#include <Eigen/SVD>
#include <iomanip>

#include <QApplication>
#include <QCheckBox>
#include <QTabWidget>
#include <QDialog>
#include <QImage>
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

    void writeFixture(const std::filesystem::path& folder)
    {
        std::ofstream(folder / "gun.urdf") << R"(<robot name="gun">
<link name="base"/><link name="carriage"/><link name="Link6"/>
<joint name="slide" type="prismatic"><parent link="base"/><child link="carriage"/>
<axis xyz="1 0 0"/><limit lower="-10" upper="10" effort="1" velocity="1"/></joint>
<joint name="tilt" type="revolute"><parent link="carriage"/><child link="Link6"/>
<axis xyz="1 0 0"/><limit lower="-3.14" upper="3.14" effort="1" velocity="1"/></joint>
</robot>)";
        for(bool reversed : { false, true }) {
            const auto filename = reversed ? "flipped.stl" : "surface.stl";
            std::ofstream stl(folder / filename);
            stl << "solid surface\n";
            // Far face deliberately precedes near face to test nearest-hit selection.
            for(double z : { 0.3, 0.0 }) {
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
        auto value = scene.sprayMeasurement("gun");
        require(value.valid && std::abs(value.distanceMeters - 0.3) < 1.0e-6,
            "Nearest forward STL intersection is 0.3 m");
        require(value.valid && std::abs(value.angleDegrees) < 1.0e-6,
            "Normal incidence is zero degrees");
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 0, "Trace is disabled by default");
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
        scene.setRobotJointValue("gun", "slide", 0.0);
        scene.setSprayRangeVisible("gun", true);
        const auto visible = scene.sprayMeasurement("gun");
        require(visible.valid && visible.distanceMeters == value.distanceMeters,
            "Measurement is independent of cone visibility");
        constexpr double pi = 3.14159265358979323846;
        scene.setRobotJointValue("gun", "tilt", pi / 6.0);
        value = scene.sprayMeasurement("gun");
        require(value.valid && std::abs(value.distanceMeters - 0.3 / std::cos(pi / 6.0)) < 1.0e-6,
            "FK tilt updates ray distance immediately");
        require(value.valid && std::abs(value.angleDegrees + 30.0) < 1.0e-6,
            "Local-X sign gives -30 degrees for positive gun tilt");
        const auto flipped = scene.sprayMeasurement("gun", "flipped");
        require(flipped.valid && std::abs(flipped.angleDegrees - value.angleDegrees) < 1.0e-6,
            "Reversed STL winding preserves signed angle");
        scene.setRobotJointValue("gun", "tilt", -pi / 6.0);
        value = scene.sprayMeasurement("gun");
        require(value.valid && std::abs(value.angleDegrees - 30.0) < 1.0e-6,
            "Opposite tilt gives +30 degrees");
        scene.setRobotJointValue("gun", "tilt", 0.0);
        simulation_project::TransformDesc gunPose;
        gunPose.z = 0.1;
        scene.setRobotBaseTransform("gun", gunPose);
        value = scene.sprayMeasurement("gun");
        require(value.valid && std::abs(value.distanceMeters - 0.2) < 1.0e-6,
            "Nozzle translation changes distance to 0.2 m");
        simulation_project::TransformDesc targetPose;
        targetPose.z = 0.4;
        scene.setRobotBaseTransform("burnner", targetPose);
        value = scene.sprayMeasurement("gun");
        require(value.valid && std::abs(value.distanceMeters - 0.3) < 1.0e-6,
            "Target transform updates despite cached local mesh");
        scene.setRobotJointValue("gun", "tilt", pi);
        require(!scene.sprayMeasurement("gun").valid, "Backward-only intersections are invalid");
        scene.setRobotJointValue("gun", "tilt", pi / 2.0);
        require(!scene.sprayMeasurement("gun").valid, "Parallel ray is invalid");
        require(!scene.sprayMeasurement("missing").valid, "Missing nozzle is invalid");
        require(!scene.sprayMeasurement("gun", "missing").valid, "Missing target is invalid");
        scene.appendEndEffectorTraceSample();
        require(scene.endEffectorTracePointCount() == 1, "Trace still samples when spray ray misses target");
        scene.setProjectDocument(document, folder);
        require(scene.endEffectorTracePointCount() == 0, "Project reload clears trace");
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
        const double nan = std::numeric_limits<double>::quiet_NaN();
        widget.showSprayMeasurementPlot({ 200, 250, nan, 300, 290 }, { -30, 0, nan, 30, 20 },
            QStringLiteral("Spray regression"));
        auto* dialog = widget.findChild<QDialog*>();
        require(dialog != nullptr, "Independent plot dialog exists");
        if(!dialog) { return; }
        for(const auto& size : { QSize(760, 600), QSize(460, 470) }) {
            dialog->resize(size);
            application.processEvents();
            const QImage image = dialog->grab().toImage();
            int distancePixels = 0, anglePixels = 0;
            for(int y = 0; y < image.height(); ++y) {
                for(int x = 0; x < image.width(); ++x) {
                    const QColor color = image.pixelColor(x, y);
                    if(y < image.height() / 2 && color.green() > 100 && color.blue() > 100 && color.red() < 30) { ++distancePixels; }
                    if(y > image.height() / 2 && color.red() > 150 && color.green() < 110 && color.blue() < 120) { ++anglePixels; }
                }
            }
            require(distancePixels > 20 && anglePixels > 20, "Both curve panels render nonblank at tested window size");
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

    class TraceObservingServices : public robot_qt_viewer::RobotQtViewerViewportServicesAdapter
    {
    public:
        using RobotQtViewerViewportServicesAdapter::RobotQtViewerViewportServicesAdapter;
        int jointUpdates = 0;
        int samples = 0;
        int resets = 0;
        bool enabled = false;
        void setRobotJointValue(const QString& robotId, const QString& jointName, double value) override
        {
            RobotQtViewerViewportServicesAdapter::setRobotJointValue(robotId, jointName, value);
            ++jointUpdates;
        }
        void setEndEffectorTraceVisible(const QString& robotId, bool visible) override
        {
            enabled = visible;
            RobotQtViewerViewportServicesAdapter::setEndEffectorTraceVisible(robotId, visible);
        }
        void clearEndEffectorTrace() override
        {
            ++resets;
            RobotQtViewerViewportServicesAdapter::clearEndEffectorTrace();
        }
        void appendEndEffectorTraceSample() override
        {
            require(jointUpdates == 2, "TCP is sampled once after the entire two-joint group");
            jointUpdates = 0;
            ++samples;
            RobotQtViewerViewportServicesAdapter::appendEndEffectorTraceSample();
        }
    };

    class OverlayObservingServices : public robot_qt_viewer::RobotQtViewerViewportServicesAdapter
    {
    public:
        using RobotQtViewerViewportServicesAdapter::RobotQtViewerViewportServicesAdapter;
        bool pointsVisible = true;
        std::size_t pointCount = 0;
        void setTrajectoryControlPointOverlay(const QString& id,
            const std::vector<simulation_project::TransformDesc>& points, bool showPoints) override
        {
            pointsVisible = showPoints;
            pointCount = points.size();
            RobotQtViewerViewportServicesAdapter::setTrajectoryControlPointOverlay(id, points, showPoints);
        }
        void clearTrajectoryControlPointOverlay(const QString& id = QString()) override
        {
            pointCount = 0;
            RobotQtViewerViewportServicesAdapter::clearTrajectoryControlPointOverlay(id);
        }
    };

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
        context.setViewportServices(&services);
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
            // A long result must export every stored row, not only sampled table rows.
            auto exportPlan = *plan;
            const auto solvedPoints = exportPlan.trajectory.points;
            exportPlan.trajectory.points.clear();
            for(int i = 0; i < 2001; ++i) {
                auto point = solvedPoints[static_cast<std::size_t>(i) % solvedPoints.size()];
                point.time = i * 0.125;
                exportPlan.trajectory.points.push_back(std::move(point));
            }
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
            bool valuesMatch = exported.points.size() == exportPlan.trajectory.points.size();
            for(std::size_t i = 0; valuesMatch && i < exported.points.size(); ++i) {
                const auto& expectedPoint = exportPlan.trajectory.points[i];
                const auto& actualPoint = exported.points[i];
                valuesMatch = std::abs(expectedPoint.time - actualPoint.timeSeconds) < 0.00000051 &&
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
        viewport.close();
    }

    void verifyPlaybackExport(QApplication& application, const std::filesystem::path& folder)
    {
        simulation_project::ProjectSession session;
        auto document = session.document();
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
        viewport.loadProjectDocument(session.document(), folder);
        viewport.show();
        application.processEvents();
        viewport.setCameraView(ProjectSceneCameraView::Isometric);
        require(viewport.sprayMeasurement(QStringLiteral("gun")).valid, "Playback viewport loads fixture");
        TraceObservingServices services(viewport);
        context.setViewportServices(&services);
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
        widget.playbackRequested(0.03);
        widget.exportSprayMeasurementsRequested();
        QEventLoop playbackLoop;
        QTimer::singleShot(500, &playbackLoop, &QEventLoop::quit);
        playbackLoop.exec();
        application.removeEventFilter(&acceptor);
        auto* plot = widget.findChild<QPushButton*>(QStringLiteral("plotSprayMeasurements"));
        require(plot && plot->isEnabled(), "Controller records all playback samples and enables plot");
        std::ifstream exported(folder / "spray-export.txt");
        std::string line;
        int dataRows = 0, validRows = 0, invalidRows = 0;
        bool hasNegativeThirty = false;
        while(std::getline(exported, line)) {
            if(line.empty() || line[0] == '#') { continue; }
            if(line.rfind("index", 0) == 0) { continue; }
            ++dataRows;
            if(line.find("\t1\tOK") != std::string::npos) { ++validRows; }
            if(line.find("\t0\tNo forward intersection") != std::string::npos) { ++invalidRows; }
            hasNegativeThirty = hasNegativeThirty || line.find("-3.0000000000e+01") != std::string::npos;
        }
        require(dataRows == 3 && validRows == 2 && invalidRows == 1,
            "TXT contains one row per joint group with validity status");
        require(hasNegativeThirty, "TXT records signed -30 degree sample");
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
        const int resetsBeforeSelection = services.resets;
        widget.trajectorySelectionChanged(QStringLiteral("missing"));
        require(services.resets > resetsBeforeSelection, "Trajectory selection clears previous trace");
        viewport.close();
    }
}

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    std::cout.setf(std::ios::unitbuf);
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
    verifyModelIk();
    if(argc >= 4 && std::string(argv[1]) == "--ik-project") {
        const auto report = argc >= 5 ? std::filesystem::u8path(argv[4]) : std::filesystem::path("ik_round_trip.csv");
        try { verifyImportedIk(std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]), report); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        try { verifyIkController(application, std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3])); }
        catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
        return failures ? 1 : 0;
    }
    try { verifyGeometry(folder); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    verifyPlot(application);
    try { verifyPlaybackExport(application, folder); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::cout << "Spray smoke failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
