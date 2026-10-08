#include "PlaybackWindowProfile.h"

#include <RobotViewport.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
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
    if(sourcePoints == 0) {
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
    QTimer::singleShot(500, &window, [&window, state, heartbeat, setupTimeout, trajectoryFile]() {
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
