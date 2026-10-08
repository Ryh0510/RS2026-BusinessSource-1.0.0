#include "PlaybackWindowProfile.h"

#include <RobotViewport.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>
#include <iostream>
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
    const int sourcePoints = QJsonDocument::fromJson(trajectory.readAll()).object()
        .value(QStringLiteral("trajectory")).toObject()
        .value(QStringLiteral("points")).toArray().size();
    if(sourcePoints == 0 && !qApp->arguments().contains(QStringLiteral("--profile-cartesian-import")) &&
        !qApp->arguments().contains(QStringLiteral("--profile-cdf-top1"))) {
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
    QObject::connect(viewport, &QOpenGLWidget::frameSwapped, &window, [state]() {
        if(!state->started) {
            return;
        }
        const auto now = state->clock.elapsed();
        if(state->lastFrame >= 0) {
            state->frameGaps.push_back(now - state->lastFrame);
        }
        state->lastFrame = now;
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
        [&window, state, heartbeat, reportFile, sourcePoints]() {
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
        const QJsonObject result{
            {"finished", finished}, {"elapsedMs", double(now)},
            {"sourcePoints", sourcePoints}, {"allPointsChecked", allPointsChecked},
            {"requestedPreviewSeconds", 5.0},
            {"frames", state->lastFrame >= 0 ? int(state->frameGaps.size() + 1) : 0},
            {"frameP95Ms", double(percentile(state->frameGaps, 95))},
            {"frameMaxMs", double(percentile(state->frameGaps, 100))},
            {"heartbeatP95Ms", double(percentile(state->heartbeatGaps, 95))},
            {"heartbeatMaxMs", double(percentile(state->heartbeatGaps, 100))},
            {"summary", summary}};
        const auto json = QJsonDocument(result).toJson();
        QFile report(reportFile);
        const bool reportSaved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
        const bool screenshotSaved = window.grab().save(reportFile + QStringLiteral(".png"));
        std::cout << "Main-window playback profile: "
            << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
        qApp->exit(finished && allPointsChecked && reportSaved && screenshotSaved ? 0 : 2);
    });
    QTimer::singleShot(500, &window, [&window, state, heartbeat, setupTimeout, trajectoryFile, reportFile, sourcePoints, viewport]() {
        for(auto* action : window.findChildren<QAction*>()) {
            if(action->text() == QStringLiteral("Motion Planning") ||
                action->text() == QStringLiteral("\u8fd0\u52a8\u89c4\u5212")) {
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
        auto* import = button(*state->editor, QStringLiteral("Import trajectory..."));
        if(!import || !import->isEnabled()) {
            fail("original import control is unavailable");
            return;
        }
        ImportFileResponder responder(trajectoryFile);
        qApp->installEventFilter(&responder);
        import->click();
        qApp->removeEventFilter(&responder);
        const auto arguments = qApp->arguments();
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
                auto* repair = button(*state->editor, QStringLiteral("Repair imported trajectory with APF + CDF/QP"));
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
                    {"safetyMarginMeters",margin->value()},{"repairElapsedMs",double(repairMs)},
                    {"top1",topRow.join(QStringLiteral(" | "))},{"labels",labels},{"exports",exports}};
                QFile report(reportFile); const auto json=QJsonDocument(result).toJson();
                const bool saved=report.open(QIODevice::WriteOnly) && report.write(json)==json.size();
                const bool screenshot=window.grab().save(reportFile+QStringLiteral(".png"));
                std::cout << "Main-window CDF profile: output=" << outputAvailable
                    << ", repair ms=" << repairMs << ", report=" << reportFile.toStdString() << std::endl;
                qApp->exit(outputAvailable && exports.size()==3 && saved && screenshot ? 0 : 2);
            });
            IkSettingsResponder settingsResponder;
            qApp->installEventFilter(&settingsResponder);
            solve->click();
            qApp->removeEventFilter(&settingsResponder);
            elapsed->restart(); poll->start();
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
            const QJsonObject result{{"success", valid}, {"sourcePoints", lines.size()},
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
        if(rapidOption >= 0 && rapidOption + 1 < arguments.size()) {
            const QString outputPath = arguments[rapidOption + 1];
            auto* exportButton = state->editor->findChild<QPushButton*>(QStringLiteral("exportCdfRapid"));
            if(!exportButton || !exportButton->isEnabled()) {
                fail("CDF RAPID export button was not enabled for the imported CDF result");
                return;
            }
            for(auto* tabs : state->editor->findChildren<QTabWidget*>()) {
                for(int index = 0; index < tabs->count(); ++index) {
                    if(tabs->tabText(index) == QStringLiteral("CDF")) { tabs->setCurrentIndex(index); }
                }
            }
            for(QWidget* parent = exportButton->parentWidget(); parent; parent = parent->parentWidget()) {
                if(auto* scroll = qobject_cast<QScrollArea*>(parent)) { scroll->ensureWidgetVisible(exportButton); }
            }
            ImportFileResponder saveResponder(outputPath);
            qApp->installEventFilter(&saveResponder);
            exportButton->click();
            qApp->removeEventFilter(&saveResponder);
            setupTimeout->stop();
            QFile exportedFile(outputPath);
            if(!exportedFile.open(QIODevice::ReadOnly)) { fail("RAPID file was not saved"); return; }
            const QString program = QString::fromUtf8(exportedFile.readAll());
            const QRegularExpression targetPattern(QStringLiteral(
                R"(CONST\s+robtarget\s+p(\d+)\s*:=\s*\[\[([^\]]+)\],\[([^\]]+)\])"));
            auto targets = targetPattern.globalMatch(program);
            std::vector<Eigen::Isometry3d> poses;
            bool parsed = program.startsWith(QStringLiteral("MODULE MainModule")) &&
                !program.contains(QStringLiteral("tooldata")) &&
                program.count(QStringLiteral(", v50, z10, tool0\\WObj:=wobj0;")) == sourcePoints;
            while(targets.hasNext()) {
                const auto match = targets.next();
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
            double positionError = 0.0, orientationError = 0.0;
            for(int i = 0; valid && i < sourcePoints; ++i) {
                std::vector<double> q;
                for(const auto& value : sourceRows[i].toObject().value(QStringLiteral("q")).toArray()) { q.push_back(value.toDouble()); }
                // This diagnostic fixture is an ABB4600 CDF plan in stored IK convention.
                if(q.size() != 6) { valid = false; break; }
                for(const int axis : {0, 3, 4, 5}) { q[axis] = -q[axis]; }
                const auto expected = fk(q);
                const auto& actual = poses[i];
                if(!actual.matrix().allFinite()) { valid = false; break; }
                positionError = std::max(positionError, (expected.translation() - actual.translation()).norm());
                orientationError = std::max(orientationError,
                    Eigen::AngleAxisd(expected.linear().transpose() * actual.linear()).angle());
            }
            valid = valid && positionError < 1.0e-8 && orientationError < 1.0e-8;
            const QJsonObject result{{"success", valid}, {"sourcePoints", sourcePoints},
                {"exportedPoints", int(poses.size())},
                {"maxPositionErrorMeters", positionError}, {"maxOrientationErrorRadians", orientationError}};
            QFile report(reportFile);
            const auto json = QJsonDocument(result).toJson();
            const bool saved = report.open(QIODevice::WriteOnly) && report.write(json) == json.size();
            QTimer::singleShot(150, &window, [&window, reportFile, result, valid, saved]() {
                const bool screenshot = window.grab().save(reportFile + QStringLiteral(".png"));
                std::cout << "Main-window RAPID export: "
                    << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
                qApp->exit(valid && saved && screenshot ? 0 : 2);
            });
            return;
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
