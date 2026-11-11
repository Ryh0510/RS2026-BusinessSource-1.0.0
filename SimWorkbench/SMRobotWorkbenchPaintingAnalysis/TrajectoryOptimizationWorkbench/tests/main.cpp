#include "TrajectoryOptimizationWorkbench.h"
#include "TrajectoryOptimizationService.h"
#include "ChaomaiPlaneOptimizer.h"
#include "CoatingAnalysisWorkbenchLifecycle.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>

using namespace robot_qt_viewer;
namespace {
    int failures = 0;
    void check(bool value, const char* text) {
        std::cout << (value ? "[OK] " : "[FAIL] ") << text << std::endl;
        if(!value) ++failures;
    }
    bool waitFor(TrajectoryOptimizationController& controller, int timeoutMs) {
        QElapsedTimer timer; timer.start();
        while(controller.busy() && timer.elapsed() < timeoutMs) {
            QApplication::processEvents(); QThread::msleep(20);
        }
        if(controller.busy()) controller.cancel();
        QApplication::processEvents();
        return !controller.busy();
    }
}
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const auto commandLine = app.arguments();
    if(commandLine.size() >= 3 && commandLine[1] == QStringLiteral("--run-request")) {
        try {
            QFile file(commandLine[2]); if(!file.open(QIODevice::ReadOnly)) return 2;
            const auto request = QJsonDocument::fromJson(file.readAll()).object();
            std::atomic_bool cancelled{false};
            const auto result = runNativePlaneOptimization(request, request.value(QStringLiteral("output_dir")).toString(),
                commandLine.contains(QStringLiteral("--compare")), cancelled, [](double value, const QString& message) {
                    std::cout << value << " " << message.toStdString() << std::endl;
                });
            std::cout << QJsonDocument(result).toJson().constData() << std::endl;return 0;
        } catch(const std::exception& e) {std::cerr << e.what() << std::endl;return 1;}
    }
    check(chaomai_plane::boundedSolverRegression(), "native bounded solver satisfies known KKT solution");
    std::atomic_bool stopped{true}; bool cancellationObserved = false;
    try {chaomai_plane::Context{stopped,{}}.check();} catch(const std::exception&) {cancellationObserved = true;}
    check(cancellationObserved, "domain cancellation is cooperative and independent of Qt");
    bool fractionalFlagsRejected = false;
    try {chaomai_plane::planFromJson({{"on",{0,.5,1}}});}
    catch(const std::exception& e) {fractionalFlagsRejected = std::string(e.what()).find("flags") != std::string::npos;}
    check(fractionalFlagsRejected, "JSON fractional spray flags cannot be truncated into valid intervals");
    bool backendObjectRejected = false;
    try {std::atomic_bool running{false};runNativePlaneOptimization({{QStringLiteral("object_id"),QStringLiteral("chaomai.pipe_plate.full")}}, QString{}, false, running, {});}
    catch(const std::exception& e) {backendObjectRejected = std::string(e.what()).find("registered only") != std::string::npos;}
    check(backendObjectRejected, "native service also refuses unrelated/full optimization objects");
    RobotQtViewerWorkbenchPackageRegistry registry;
    check(registerPaintingAnalysisWorkbenchContribution(registry), "register painting and optimization workbenches");
    const auto* descriptor = registry.registeredWorkbench(QStringLiteral("smrobot.mode.trajectory-optimization"));
    check(descriptor != nullptr && descriptor->descriptor.displayName == QStringLiteral("轨迹优化"), "optimization has a distinct catalog entry");
    QWidget root;
    auto* panel = new TrajectoryOptimizationPanel(&root);
    auto* controller = new TrajectoryOptimizationController(*panel, [] {
        QJsonObject options;
        options.insert(QStringLiteral("available"), false);
        return options;
    }, panel);
    check(panel->findChild<QLineEdit*>(QStringLiteral("optimizationPythonPath")) == nullptr, "native panel needs no interpreter");
    check(panel->object->count() == 2, "two explicit optimization objects");
    check(!panel->compare->isEnabled(), "comparison unavailable before optimization");
    panel->object->setCurrentIndex(1);
    check(!panel->optimize->isEnabled(), "full mode cannot invoke plane algorithm");
    controller->optimize();
    check(!controller->busy() && controller->session().result.isEmpty(), "controller also rejects full mode");
    panel->object->setCurrentIndex(0);
    check(panel->optimize->isEnabled(), "plane mode available");
    controller->optimize();
    check(!controller->busy() && controller->session().result.isEmpty(), "missing inputs cannot publish a result");

    const auto args = app.arguments();
    if(args.size() >= 3 && args[1] == QStringLiteral("--capture")) {
        // The Windows platform font engine renders the production UI. Keep
        // this test window off the user's screen while capturing its layout.
        app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9));
        root.setAttribute(Qt::WA_DontShowOnScreen);
        auto* layout = new QHBoxLayout(&root);
        layout->addWidget(panel->inputs, 1); layout->addWidget(panel, 1);
        root.resize(1180, 1050); root.show(); QApplication::processEvents();
        check(root.grab().save(args[2]+QStringLiteral("/optimization_ui.png")), "capture production UI layout with Windows fonts");
    }
    if(args.size() >= 5 && args[1] == QStringLiteral("--real")) {
        panel->shareCoating->setChecked(false); panel->timeStep->setValue(.02);
        panel->modelPath->setText(args[2]); panel->trajectoryPath->setText(args[3]);
        panel->outputPath->setText(args[4]);
        controller->optimize();
        check(controller->busy(), "real request starts asynchronously");
        check(!panel->object->isEnabled() && panel->cancel->isEnabled(), "inputs locked and cancellation enabled while running");
        check(waitFor(*controller, 900000), "real optimization completes within budget");
        std::cout << controller->session().status.toStdString() << std::endl;
        const auto result = controller->session().result;
        std::cout << QJsonDocument(result).toJson().constData() << std::endl;
        check(result.value(QStringLiteral("accepted")).toBool(), "real trajectory accepted after dense verification");
        check(result.value(QStringLiteral("dense_validation")).toObject().value(QStringLiteral("max_relative_error_percent")).toDouble(1e9) <= 10., "dense thickness meets requested +/-10 percent");
        check(panel->compare->isEnabled(), "comparison enabled for actual result");
        const auto duration = result.value(QStringLiteral("optimized_duration_s")).toDouble();
        check(duration > 0, "result contains actual optimized timing");
        controller->compare();
        check(waitFor(*controller, 600000), "comparison recomputation completes");
        check(controller->session().result.value(QStringLiteral("comparison_recomputed")).toBool(), "button recalculates both thickness fields");
        check(panel->findChild<QDialog*>(QStringLiteral("optimizationThicknessComparisonDialog")) != nullptr, "comparison opens real plot dialog");
        panel->resize(440, 1000); panel->show(); QApplication::processEvents();
        panel->grab().save(args[4]+QStringLiteral("/right_panel.png"));
        panel->inputs->resize(520, 1000); panel->inputs->show(); QApplication::processEvents();
        panel->inputs->grab().save(args[4]+QStringLiteral("/left_panel.png"));
        panel->tolerance->setValue(9.);
        check(controller->session().result.isEmpty() && !panel->compare->isEnabled(), "changed target invalidates previous result");
        qputenv("RS2026_OPTIMIZATION_NO_CACHE", "1");
        controller->optimize();
        check(controller->busy(), "second request can start");
        // Cooperatively cancel inside the actual native predictor, not just
        // before startup. The request bypasses fields cached by the first run.
        QElapsedTimer cancelTimer; cancelTimer.start();
        while(controller->busy() && panel->progress->value() < 20 && cancelTimer.elapsed() < 30000) {
            QApplication::processEvents(); QThread::msleep(20);
        }
        check(controller->busy(), "cancellation reaches a running native calculation");
        controller->cancel();
        check(waitFor(*controller, 10000), "cancel terminates computation");
        check(controller->session().result.isEmpty(), "cancelled request cannot become an accepted trajectory");
        controller->optimize(); controller->clear(); QApplication::processEvents();
        check(!controller->busy() && controller->session().result.isEmpty(), "release joins native worker and ignores stale queued callbacks");
        qunsetenv("RS2026_OPTIMIZATION_NO_CACHE");
    }
    controller->clear();
    check(controller->session().result.isEmpty() && panel->modelPath->text().isEmpty(), "project release clears transient session");
    std::cout << "Failures: " << failures << std::endl;
    return failures ? 1 : 0;
}
