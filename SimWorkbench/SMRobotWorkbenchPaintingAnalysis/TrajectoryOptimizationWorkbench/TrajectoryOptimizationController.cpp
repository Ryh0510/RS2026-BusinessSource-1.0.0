#include "TrajectoryOptimizationWorkbench.h"
#include "TrajectoryOptimizationService.h"
#include <SprayThicknessPrediction/ThicknessPrediction.h>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <stdexcept>

namespace robot_qt_viewer
{
    TrajectoryOptimizationController::TrajectoryOptimizationController(TrajectoryOptimizationPanel& panel,
        std::function<QJsonObject()> coatingSettings, QObject* parent)
        : QObject(parent), m_panel(panel), m_coatingSettings(std::move(coatingSettings))
    {
        setObjectName(QStringLiteral("trajectoryOptimizationController"));
        connect(&panel, &TrajectoryOptimizationPanel::optimizeRequested, this, &TrajectoryOptimizationController::optimize);
        connect(&panel, &TrajectoryOptimizationPanel::compareRequested, this, &TrajectoryOptimizationController::compare);
        connect(&panel, &TrajectoryOptimizationPanel::cancelRequested, this, &TrajectoryOptimizationController::cancel);
        connect(&panel, &TrajectoryOptimizationPanel::openOutputRequested, this, [this]() {
            if(!m_session.runDirectory.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(m_session.runDirectory));
        });
        connect(&panel, &TrajectoryOptimizationPanel::inputsChanged, this, [this]() {
            if(m_session.busy) return;
            m_session.result = {};
            m_session.status = QStringLiteral("输入或指标已改变，请重新优化。");
            refresh();
        });
        m_session.status = QStringLiteral("导入STL工件和待优化轨迹后开始。结果会写入独立运行目录。");
        refresh();
    }
    TrajectoryOptimizationController::~TrajectoryOptimizationController()
    {
        if(m_cancelled) m_cancelled->store(true);
        if(m_worker.joinable()) m_worker.join();
    }
    void TrajectoryOptimizationController::clear()
    {
        ++m_generation;
        if(m_cancelled) m_cancelled->store(true);
        if(m_worker.joinable()) m_worker.join();
        m_session = {};
        m_session.status = QStringLiteral("会话已清除；已导出的文件保留。");
        m_panel.modelPath->clear(); m_panel.trajectoryPath->clear(); m_panel.log->clear();
        refresh();
    }
    bool TrajectoryOptimizationController::busy() const { return m_session.busy; }
    const TrajectoryOptimizationSession& TrajectoryOptimizationController::session() const { return m_session; }
    void TrajectoryOptimizationController::setActive(bool active) { m_active = active; m_panel.inputs->setVisible(active); }

    QJsonObject TrajectoryOptimizationController::predictionSnapshot() const
    {
        QJsonObject opts;
        const bool shared = m_panel.shareCoating->isChecked();
        if(shared) {
            opts = m_coatingSettings ? m_coatingSettings() : QJsonObject{};
            if(!opts.value(QStringLiteral("available")).toBool())
                throw std::runtime_error("Coating-analysis controls are unavailable. Enable that workbench, or disable shared settings.");
        } else {
            opts.insert(QStringLiteral("model"), m_panel.predictionModel->currentData().toString());
            opts.insert(QStringLiteral("spray_axis"), QJsonArray{0, 0, 1});
            opts.insert(QStringLiteral("feed_axis"), QJsonArray{0, 1, 0});
            opts.insert(QStringLiteral("time_step_s"), m_panel.timeStep->value());
        }
        opts.insert(QStringLiteral("history"), m_panel.useHistory->isChecked() && (!shared || opts.value(QStringLiteral("history")).toBool()));
        opts.insert(QStringLiteral("bvh"), m_panel.useBvh->isChecked() && (!shared || opts.value(QStringLiteral("bvh")).toBool()));
        opts.insert(QStringLiteral("auto_flip"), m_panel.autoFlip->isChecked() && (!shared || opts.value(QStringLiteral("auto_flip")).toBool()));
        opts.insert(QStringLiteral("source"), shared ? QStringLiteral("coating_analysis_snapshot") : QStringLiteral("independent_coating_defaults"));
        // Serialize the actual shared C++ defaults, rather than duplicating
        // constants in the UI or silently ignoring future default changes.
        const spraythickness::PaperGaussianParameters d;
        const spraythickness::HeatHistoryParameters h;
#define ADD_PARAMETER(object, name) opts.insert(QStringLiteral(#name), object.name)
        ADD_PARAMETER(d, amplitudeMillimeters); ADD_PARAMETER(d, rotationRadians);
        ADD_PARAMETER(d, phiOffsetRadians); ADD_PARAMETER(d, psiOffsetRadians);
        ADD_PARAMETER(d, sigmaPhiRadians); ADD_PARAMETER(d, sigmaPsiRadians);
        ADD_PARAMETER(d, referenceDistanceMeters); ADD_PARAMETER(d, referenceAngleDegrees);
        ADD_PARAMETER(d, referenceExposureSeconds); ADD_PARAMETER(h, correctionAmplitude);
        ADD_PARAMETER(h, historyScaleSeconds); ADD_PARAMETER(h, referenceHistorySeconds);
        ADD_PARAMETER(h, coolingTimeSeconds); ADD_PARAMETER(h, activityThresholdRatio);
#undef ADD_PARAMETER
        return opts;
    }

    void TrajectoryOptimizationController::optimize() { launch(false); }
    void TrajectoryOptimizationController::compare() { launch(true); }
    void TrajectoryOptimizationController::launch(bool compareOnly)
    {
        if(m_session.busy) return;
        try {
            if(m_panel.object->currentData().toString() != QStringLiteral("chaomai.pipe_plate.plane"))
                throw std::runtime_error("Full optimization has no registered algorithm. The plane algorithm is not allowed for this object.");
            if(compareOnly) {
                if(m_session.result.isEmpty()) throw std::runtime_error("No optimization result to compare.");

            } else {
                for(const auto* edit : {m_panel.modelPath, m_panel.trajectoryPath})
                    if(!QFileInfo(edit->text()).isFile()) throw std::runtime_error("Select an existing STL and trajectory file.");
                const QString parent = m_panel.outputPath->text().trimmed();
                if(parent.isEmpty()) throw std::runtime_error("Choose an output parent directory.");
                const QString run = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))+QStringLiteral("_")+QUuid::createUuid().toString(QUuid::Id128).left(8);
                m_session.runDirectory = QDir(parent).absoluteFilePath(run);
                if(!QDir().mkpath(m_session.runDirectory)) throw std::runtime_error("Cannot create output directory.");
                QJsonObject request;
                request.insert(QStringLiteral("schema_version"), 1);
                request.insert(QStringLiteral("object_id"), m_panel.object->currentData().toString());
                request.insert(QStringLiteral("model_path"), QFileInfo(m_panel.modelPath->text()).absoluteFilePath());
                request.insert(QStringLiteral("trajectory_path"), QFileInfo(m_panel.trajectoryPath->text()).absoluteFilePath());
                request.insert(QStringLiteral("output_dir"), m_session.runDirectory);
                request.insert(QStringLiteral("tolerance_percent"), m_panel.tolerance->value());
                request.insert(QStringLiteral("target_um"), m_panel.target->value());
                request.insert(QStringLiteral("prediction"), predictionSnapshot());
                const QString path = m_session.runDirectory+QStringLiteral("/request.json");
                QSaveFile file(path);
                if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(request).toJson()) < 0 || !file.commit())
                    throw std::runtime_error("Cannot save optimization request.");
                m_session.request = request; m_session.result = {};

            }
            m_panel.log->clear(); m_panel.progress->setValue(0);
            m_session.busy = true; m_session.cancelled = false; m_session.comparing = compareOnly;
            m_session.status = compareOnly ? QStringLiteral("正在使用本次参数快照重新计算优化前后厚度…") : QStringLiteral("正在使用 C++ 优化并复核；达标前不会发布为成功结果…");
            if(m_worker.joinable()) m_worker.join();
            m_cancelled = std::make_shared<std::atomic_bool>(false);
            const auto cancelled = m_cancelled;
            const auto generation = ++m_generation;
            const auto request = m_session.request;
            const auto directory = m_session.runDirectory;
            refresh();
            m_worker = std::thread([this, cancelled, generation, request, directory, compareOnly]() {
                QJsonObject result; QString error;
                try {
                    result = runNativePlaneOptimization(request, directory, compareOnly, *cancelled,
                        [this, generation](double value, const QString& message) {
                            QMetaObject::invokeMethod(this, [this, generation, value, message]() {
                                if(generation != m_generation || !m_session.busy) return;
                                m_panel.progress->setValue(qBound(0, int(value*1000), 1000));
                                m_panel.log->appendPlainText(message);
                            }, Qt::QueuedConnection);
                        });
                } catch(const std::exception& e) { error = QString::fromUtf8(e.what()); }
                catch(...) { error = QStringLiteral("Unexpected native optimization error."); }
                QMetaObject::invokeMethod(this, [this, generation, result, error]() {
                    if(generation == m_generation) finish(result, error);
                }, Qt::QueuedConnection);
            });
        } catch(const std::exception& error) {
            m_session.busy = false;
            m_session.status = QStringLiteral("未开始：")+QString::fromUtf8(error.what()); refresh();
        }
    }
    void TrajectoryOptimizationController::finish(const QJsonObject& result, const QString& error)
    {
        if(m_worker.joinable()) m_worker.join();
        m_session.busy = false;
        if(m_session.cancelled) { m_session.result = {}; m_session.status = QStringLiteral("已取消。本次未接受任何轨迹；输入文件未变。"); }
        else if(!error.isEmpty()) {
            m_session.result = {};
            m_session.status = QStringLiteral("计算失败：")+error;
            m_panel.log->appendPlainText(error);
        } else if(result.value(QStringLiteral("object_id")).toString() != QStringLiteral("chaomai.pipe_plate.plane") || !result.contains(QStringLiteral("dense_validation"))) {
            m_session.result = {}; m_session.status = QStringLiteral("结果缺少平面优化密集复核记录。");
        } else {
            m_session.result = result;
            m_session.status = result.value(QStringLiteral("accepted")).toBool()
                ? QStringLiteral("C++ 优化已通过密集采样及更小时间步长复核。TCP TXT与工艺CSV已输出。")
                : QStringLiteral("未达到输入的均匀性范围。仅保存未接受候选与真实预测图，未标记为成功轨迹。");
            if(m_session.comparing) showGraph();
        }
        refresh();
    }
    void TrajectoryOptimizationController::cancel()
    {
        if(!m_session.busy) return;
        m_session.cancelled = true; m_session.status = QStringLiteral("正在取消…");
        if(m_cancelled) m_cancelled->store(true);
        refresh();
    }
    void TrajectoryOptimizationController::refresh()
    {
        const bool plane = m_panel.object->currentData().toString() == QStringLiteral("chaomai.pipe_plate.plane");
        m_panel.inputs->setEnabled(!m_session.busy);
        m_panel.object->setEnabled(!m_session.busy); m_panel.target->setEnabled(!m_session.busy); m_panel.tolerance->setEnabled(!m_session.busy);
        m_panel.optimize->setEnabled(plane && !m_session.busy);
        m_panel.compare->setEnabled(plane && !m_session.busy && !m_session.result.isEmpty());
        m_panel.cancel->setEnabled(m_session.busy);
        m_panel.openOutput->setEnabled(!m_session.runDirectory.isEmpty());
        m_panel.scope->setText(plane ? QStringLiteral("专用保形算法：保留36道身份和20道平板正喷，局部调整管根、间距和沿道速度；不增加补喷道。指标=铜片各样本相对目标厚度的最大偏差，不是CV；管面不计入。")
            : QStringLiteral("全优化尚未注册独立算法，当前不可运行。此对象禁止调用平面专用算法。"));
        m_panel.status->setText(m_session.status);
        QString text;
        if(!m_session.result.isEmpty()) {
            const auto before = m_session.result.value(QStringLiteral("before")).toObject();
            const auto after = m_session.result.value(QStringLiteral("after")).toObject();
            const auto fine = m_session.result.value(QStringLiteral("dense_validation")).toObject();
            auto describe = [](const QString& name, const QJsonObject& m) {
                return QStringLiteral("%1：%2～%3 μm；CV %4%；最大偏差 %5%")
                    .arg(name).arg(m.value(QStringLiteral("min_um")).toDouble(), 0, 'f', 2)
                    .arg(m.value(QStringLiteral("max_um")).toDouble(), 0, 'f', 2)
                    .arg(m.value(QStringLiteral("cv_percent")).toDouble(), 0, 'f', 3)
                    .arg(m.value(QStringLiteral("max_relative_error_percent")).toDouble(), 0, 'f', 3);
            };
            text = describe(QStringLiteral("优化前"), before)+QStringLiteral("\n")+describe(QStringLiteral("优化后"), after)+QStringLiteral("\n")+
                describe(QStringLiteral("密集复核"), fine)+QStringLiteral("\n结果：")+m_session.runDirectory;
        }
        m_panel.statistics->setText(text);
    }
    void TrajectoryOptimizationController::showGraph()
    {
        const QPixmap image(m_session.runDirectory+QStringLiteral("/thickness_comparison.png"));
        if(image.isNull()) return;
        auto* dialog = new QDialog(&m_panel); dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setObjectName(QStringLiteral("optimizationThicknessComparisonDialog"));
        dialog->setWindowTitle(QStringLiteral("优化前后厚度与均匀性（本次同参数预测）")); dialog->resize(1100, 850);
        auto* layout = new QVBoxLayout(dialog); auto* scroll = new QScrollArea(dialog);
        auto* label = new QLabel(scroll); label->setPixmap(image.scaledToWidth(1040, Qt::SmoothTransformation));
        scroll->setWidget(label); layout->addWidget(scroll); dialog->show();
    }
}
