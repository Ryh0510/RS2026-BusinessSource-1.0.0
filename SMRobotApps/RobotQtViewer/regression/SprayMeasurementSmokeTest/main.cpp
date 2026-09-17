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

#include <QApplication>
#include <QDialog>
#include <QImage>
#include <QFileDialog>
#include <QOffscreenSurface>
#include <QOpenGLContext>
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
<link name="base"/><link name="Link6"/>
<joint name="tilt" type="revolute"><parent link="base"/><child link="Link6"/>
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
        plan.jointNames = { "tilt" };
        constexpr double pi = 3.14159265358979323846;
        plan.trajectory.points = { { 0.0, { 0.0 }, {}, {} },
            { 0.5, { pi / 6.0 }, {}, {} }, { 1.0, { pi }, {}, {} } };
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
        viewport.resize(320, 240);
        viewport.loadProjectDocument(session.document(), folder);
        viewport.show();
        application.processEvents();
        require(viewport.sprayMeasurement(QStringLiteral("gun")).valid, "Playback viewport loads fixture");
        robot_qt_viewer::RobotQtViewerViewportServicesAdapter services(viewport);
        context.setViewportServices(&services);
        selection.selectRobotLink(QStringLiteral("gun"), QStringLiteral("Link6"));
        MotionPlanningEditorWidget widget;
        robot_qt_viewer::MotionPlanningModuleController controller(widget, context);

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
        viewport.close();
    }
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    std::cout.setf(std::ios::unitbuf);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    QOpenGLContext context;
    context.setFormat(format);
    require(context.create() && context.makeCurrent(&surface), "Offscreen OpenGL context available");
    if(failures) { return 1; }
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary fixture directory available");
    if(failures) { return 1; }
    const std::filesystem::path folder(temporary.path().toStdWString());
    writeFixture(folder);
    try { verifyGeometry(folder); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    verifyPlot(application);
    try { verifyPlaybackExport(application, folder); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; ++failures; }
    std::cout << "Spray smoke failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
