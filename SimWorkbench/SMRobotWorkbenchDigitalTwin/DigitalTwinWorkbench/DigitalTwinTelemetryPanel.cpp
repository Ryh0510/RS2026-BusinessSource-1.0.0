#include "DigitalTwinTelemetryPanel.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <functional>
#include <cmath>
#include <limits>

namespace {
    QFont preferredTelemetryFont()
    {
        QFont font = QApplication::font();
        const auto families = QFontDatabase().families();
        for(const QString& family : {QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"), QStringLiteral("Noto Sans CJK SC"), QStringLiteral("SimSun")}) {
            if(families.contains(family, Qt::CaseInsensitive)) { font.setFamily(family); break; }
        }
        font.setPointSizeF(std::max(9.0, font.pointSizeF()));
        return font;
    }

    QString speedStatisticsText(const robot_qt_viewer::TelemetrySpeedStatistics& statistics)
    {
        if(!statistics.valid) return QStringLiteral("\u901f\u5ea6\u7edf\u8ba1\u4e0d\u53ef\u7528\n%1").arg(statistics.diagnostic);
        const QString methodText = statistics.method == robot_qt_viewer::TelemetrySpeedCalculationMethod::WindowAverage
            ? QStringLiteral("\u65f6\u95f4\u7a97\u53e3\u5e73\u5747\uff08\u6700\u5c0f %1 s\uff09").arg(statistics.minimumIntervalSeconds, 0, 'f', 3)
            : QStringLiteral("\u76f8\u90bb\u91c7\u6837\u77ac\u65f6\u5dee\u5206");
        const auto number = [](double value) { return std::isfinite(value) ? QString::number(value, 'f', 3)
            : QStringLiteral("\u65e0\u6709\u6548\u6570\u636e"); };
        QString text = QStringLiteral("\u65f6\u95f4 %1 - %2 s\n\u8ba1\u7b97\u65b9\u6cd5: %3\n\u6709\u6548\u901f\u5ea6\u6837\u672c %4\nTCP \u7ebf\u901f\u5ea6\uff1a\u5e73\u5747 %5 / \u6700\u5927 %6 mm/s\nTCP \u89d2\u901f\u5ea6\uff1a\u5e73\u5747 %7 / \u6700\u5927 %8 deg/s")
            .arg(statistics.startSeconds, 0, 'f', 3).arg(statistics.endSeconds, 0, 'f', 3)
            .arg(methodText).arg(statistics.sampleCount)
            .arg(number(statistics.tcpLinearAverageMmPerSecond)).arg(number(statistics.tcpLinearMaxMmPerSecond))
            .arg(number(statistics.tcpAngularAverageDegPerSecond)).arg(number(statistics.tcpAngularMaxDegPerSecond));
        text += QStringLiteral("\n\u5173\u8282\u901f\u5ea6\uff08\u7edd\u5bf9\u503c\uff09\uff1a");
        for(int i = 0; i < statistics.jointMaxSpeed.size(); ++i) {
            text += QStringLiteral("J%1 %2/%3").arg(i + 1)
                .arg(statistics.jointAverageSpeed.value(i), 0, 'f', 3)
                .arg(statistics.jointMaxSpeed.value(i), 0, 'f', 3);
            if(i + 1 < statistics.jointMaxSpeed.size()) text += QStringLiteral(", ");
        }
        return text;
    }

    enum class SpeedMetric { TcpLinear, TcpAngular, Joint };

    class TelemetrySpeedPlot final : public QWidget {
    public:
        explicit TelemetrySpeedPlot(QWidget* parent, SpeedMetric metric, int jointIndex = 0)
            : QWidget(parent), m_metric(metric), m_jointIndex(jointIndex) {
            setMinimumHeight(260);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }
        void setStatistics(const robot_qt_viewer::TelemetrySpeedStatistics& value) { m_stats = value; update(); }
    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
            painter.fillRect(rect(), palette().base());
            const QRectF plot(54, 20, std::max(1, width() - 72), std::max(1, height() - 48));
            painter.setPen(palette().mid().color());
            for(int i = 0; i <= 4; ++i) {
                const double y = plot.bottom() - i * plot.height() / 4.;
                painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            }
            painter.setPen(palette().text().color()); painter.drawRect(plot);
            if(!m_stats.valid || m_stats.points.empty()) {
                painter.drawText(plot, Qt::AlignCenter, QStringLiteral("\u8bf7\u5148\u91c7\u96c6\u6570\u636e\uff0c\u8bbe\u7f6e\u6709\u6548\u65f6\u95f4\u6bb5\u540e\u70b9\u51fb\u8ba1\u7b97\u901f\u5ea6")); return;
            }
            const auto valueAt = [this](const robot_qt_viewer::TelemetrySpeedPoint& point) {
                if(m_metric == SpeedMetric::TcpLinear) return point.tcpLinearMmPerSecond;
                if(m_metric == SpeedMetric::TcpAngular) return point.tcpAngularDegPerSecond;
                return m_jointIndex < point.jointAbsSpeed.size() ? point.jointAbsSpeed[m_jointIndex] : std::numeric_limits<double>::quiet_NaN();
            };
            double maxValue = 1.0;
            bool hasValue = false;
            for(const auto& point : m_stats.points) if(std::isfinite(valueAt(point))) {
                hasValue = true; maxValue = std::max(maxValue, valueAt(point));
            }
            if(!hasValue) {
                painter.drawText(plot, Qt::AlignCenter, QStringLiteral("\u6240\u9009\u65f6\u6bb5\u65e0\u6709\u6548\u901f\u5ea6\u6570\u636e")); return;
            }
            const double span = std::max(1.e-9, m_stats.endSeconds - m_stats.startSeconds);
            const auto x = [&](double t) { return plot.left() + (t - m_stats.startSeconds) / span * plot.width(); };
            const auto y = [&](double value) { return plot.bottom() - value / maxValue * plot.height(); };
            const QColor color = m_metric == SpeedMetric::TcpLinear ? QColor(0, 145, 160)
                : m_metric == SpeedMetric::TcpAngular ? QColor(205, 80, 85)
                : QColor::fromHsv((m_jointIndex * 47) % 360, 190, 190);
            QPainterPath path; bool first = true;
            painter.setPen(QPen(color, 2.2));
            for(const auto& point : m_stats.points) {
                if(!std::isfinite(valueAt(point))) { first = true; continue; }
                const QPointF p(x(point.elapsedSeconds), y(valueAt(point)));
                if(first) path.moveTo(p); else path.lineTo(p); first = false;
            }
            painter.drawPath(path);
            painter.setBrush(color);
            for(const auto& point : m_stats.points) if(std::isfinite(valueAt(point)))
                painter.drawEllipse(QPointF(x(point.elapsedSeconds), y(valueAt(point))), 2.6, 2.6);
            painter.setPen(color);
            if(m_metric == SpeedMetric::TcpLinear) painter.drawText(62, 15, QStringLiteral("TCP \u7ebf\u901f\u5ea6 (mm/s)"));
            else if(m_metric == SpeedMetric::TcpAngular) painter.drawText(62, 15, QStringLiteral("TCP \u89d2\u901f\u5ea6 (deg/s)"));
            else painter.drawText(62, 15, QStringLiteral("J%1 \u5173\u8282\u901f\u5ea6 (deg/s)").arg(m_jointIndex + 1));
            painter.setPen(palette().text().color()); painter.drawText(4, plot.top() + 5, QString::number(maxValue, 'g', 5));
            painter.drawText(4, plot.bottom(), QStringLiteral("0"));
            painter.drawText(plot.left(), height() - 8, QString::number(m_stats.startSeconds, 'f', 3));
            painter.drawText(plot.right() - 45, height() - 8, QString::number(m_stats.endSeconds, 'f', 3));
        }
    private:
        robot_qt_viewer::TelemetrySpeedStatistics m_stats;
        SpeedMetric m_metric;
        int m_jointIndex = 0;
    };

    class TelemetrySpeedDialog final : public QDialog {
    public:
        explicit TelemetrySpeedDialog(QWidget* parent) : QDialog(parent) {
            setObjectName(QStringLiteral("telemetrySpeedDialog"));
            setWindowTitle(QStringLiteral("ROBOGUIDE \u901f\u5ea6\u5206\u6790"));
            setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
            resize(980, 700);
            setFont(preferredTelemetryFont());
            auto* layout = new QVBoxLayout(this);
            auto* controls = new QHBoxLayout;
            controls->addWidget(new QLabel(QStringLiteral("\u91c7\u6837\u65f6\u95f4\u6bb5 (s)"), this));
            m_start = new QDoubleSpinBox(this); m_start->setObjectName(QStringLiteral("telemetryDialogSpeedStart")); m_start->setRange(0., 1.e9); m_start->setDecimals(3); m_start->setSingleStep(.1); controls->addWidget(m_start);
            controls->addWidget(new QLabel(QStringLiteral("\u81f3"), this));
            m_end = new QDoubleSpinBox(this); m_end->setObjectName(QStringLiteral("telemetryDialogSpeedEnd")); m_end->setRange(0., 1.e9); m_end->setDecimals(3); m_end->setSingleStep(.1); controls->addWidget(m_end);
            controls->addWidget(new QLabel(QStringLiteral("\u65b9\u6cd5"), this));
            m_method = new QComboBox(this); m_method->setObjectName(QStringLiteral("telemetryDialogSpeedMethod"));
            m_method->addItem(QStringLiteral("\u76f8\u90bb\u91c7\u6837\u77ac\u65f6\u5dee\u5206"), int(robot_qt_viewer::TelemetrySpeedCalculationMethod::InstantaneousDifference));
            m_method->addItem(QStringLiteral("\u65f6\u95f4\u7a97\u53e3\u5e73\u5747\u901f\u5ea6"), int(robot_qt_viewer::TelemetrySpeedCalculationMethod::WindowAverage));
            controls->addWidget(m_method);
            controls->addWidget(new QLabel(QStringLiteral("\u6700\u5c0f\u7a97\u53e3 (s)"), this));
            m_minimum = new QDoubleSpinBox(this); m_minimum->setObjectName(QStringLiteral("telemetryDialogSpeedMinimumInterval")); m_minimum->setRange(.001, 1.e6); m_minimum->setDecimals(3); m_minimum->setValue(.1); m_minimum->setSingleStep(.01); controls->addWidget(m_minimum);
            m_calculate = new QPushButton(QStringLiteral("\u8ba1\u7b97\u901f\u5ea6"), this); m_calculate->setObjectName(QStringLiteral("telemetryDialogSpeedCalculate")); controls->addWidget(m_calculate);
            controls->addStretch(1); layout->addLayout(controls);
            m_info = new QLabel(this); m_info->setObjectName(QStringLiteral("telemetryDialogSpeedInfo")); m_info->setWordWrap(true); layout->addWidget(m_info);
            m_tabs = new QTabWidget(this); m_tabs->setObjectName(QStringLiteral("telemetryDialogSpeedTabs"));
            addPlot(SpeedMetric::TcpLinear, 0, QStringLiteral("TCP \u7ebf\u901f\u5ea6"), QStringLiteral("telemetryDialogTcpLinearSpeedPlot"));
            addPlot(SpeedMetric::TcpAngular, 0, QStringLiteral("TCP \u89d2\u901f\u5ea6"), QStringLiteral("telemetryDialogTcpAngularSpeedPlot"));
            for(int i = 0; i < 6; ++i) addPlot(SpeedMetric::Joint, i, QStringLiteral("J%1 \u5173\u8282\u901f\u5ea6").arg(i + 1), QStringLiteral("telemetryDialogJoint%1SpeedPlot").arg(i + 1));
            layout->addWidget(m_tabs, 1);
            m_calculate->setFont(font());
            QObject::connect(m_calculate, &QPushButton::clicked, this, [this] { if(onCalculate) onCalculate(m_start->value(), m_end->value(), static_cast<robot_qt_viewer::TelemetrySpeedCalculationMethod>(m_method->currentData().toInt()), m_minimum->value()); });
        }
        std::function<void(double, double, robot_qt_viewer::TelemetrySpeedCalculationMethod, double)> onCalculate;
        void setInterval(double start, double end, double maximum, robot_qt_viewer::TelemetrySpeedCalculationMethod method, double minimum) { m_start->setMaximum(std::max(1.0, maximum)); m_end->setMaximum(std::max(1.0, maximum)); m_start->setValue(start); m_end->setValue(end); m_method->setCurrentIndex(m_method->findData(int(method))); m_minimum->setValue(minimum > 0.0 ? minimum : .1); }
        double startSeconds() const { return m_start->value(); }
        double endSeconds() const { return m_end->value(); }
        void setStatistics(const robot_qt_viewer::TelemetrySpeedStatistics& statistics) {
            for(auto* plot : m_plots) plot->setStatistics(statistics);
            m_info->setText(speedStatisticsText(statistics));
        }
    private:
        void addPlot(SpeedMetric metric, int jointIndex, const QString& label, const QString& objectName) {
            auto* plot = new TelemetrySpeedPlot(m_tabs, metric, jointIndex);
            plot->setObjectName(objectName);
            m_tabs->addTab(plot, label);
            m_plots.push_back(plot);
        }
        QDoubleSpinBox* m_start = nullptr;
        QDoubleSpinBox* m_end = nullptr;
        QComboBox* m_method = nullptr;
        QDoubleSpinBox* m_minimum = nullptr;
        QPushButton* m_calculate = nullptr;
        QLabel* m_info = nullptr;
        QTabWidget* m_tabs = nullptr;
        QVector<TelemetrySpeedPlot*> m_plots;
    };
}

namespace robot_qt_viewer {
    DigitalTwinTelemetryPanel::DigitalTwinTelemetryPanel(QWidget* parent) : QWidget(parent) {
        setObjectName(QStringLiteral("digitalTwinTelemetryPanel"));
        setFont(preferredTelemetryFont());
        auto* layout = new QVBoxLayout(this);
        auto* heading = new QLabel(QStringLiteral("ROBOGUIDE Classic \u5b9e\u65f6\u901a\u4fe1"), this);
        layout->addWidget(heading);
        auto* hint = new QLabel(QStringLiteral("\u6253\u5f00\u5de5\u4f5c\u7ad9\u540e\u542f\u52a8\u901a\u4fe1\u3002\u81ea\u52a8\u53d1\u73b0\u8fd0\u884c\u4e2d\u7684\u865a\u62df\u63a7\u5236\u5668\uff1b\u591a\u4e2a\u63a7\u5236\u5668\u65f6\u8bf7\u9009\u62e9\u3002\u5173\u8282\u503c\u4fdd\u6301 FANUC \u539f\u59cb\u503c\uff0c\u8f6c\u8f74\u5355\u4f4d\u4e3a\u5ea6\uff0c\u5916\u8f74\u4fdd\u6301\u63a7\u5236\u5668\u539f\u5355\u4f4d\u3002"), this);
        hint->setWordWrap(true); layout->addWidget(hint);
        auto* form = new QFormLayout;
        m_endpoints = new QComboBox(this); m_endpoints->setObjectName(QStringLiteral("telemetryEndpoints"));
        m_endpoints->addItem(QStringLiteral("\u81ea\u52a8\u9009\u62e9\u552f\u4e00\u8fd0\u884c\u63a7\u5236\u5668"), QString());
        m_endpoints->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        m_endpoints->setMinimumContentsLength(16);
        form->addRow(QStringLiteral("\u5de5\u4f5c\u7ad9 / \u63a7\u5236\u5668"), m_endpoints);
        m_group = new QSpinBox(this); m_group->setRange(1, 32);
        form->addRow(QStringLiteral("\u8fd0\u52a8\u7ec4"), m_group);
        m_interval = new QSpinBox(this); m_interval->setRange(10, 1000); m_interval->setValue(20); m_interval->setSuffix(QStringLiteral(" ms"));
        form->addRow(QStringLiteral("\u8bf7\u6c42\u91c7\u6837\u5468\u671f"), m_interval);
        m_csv = new QLineEdit(this); m_csv->setObjectName(QStringLiteral("telemetryCsvPath"));
        m_csv->setPlaceholderText(QStringLiteral("\u81ea\u52a8\u751f\u6210\u65b0\u7684 CSV \u8bb0\u5f55\u6587\u4ef6"));
        auto* file = new QHBoxLayout; file->addWidget(m_csv);
        m_browse = new QPushButton(QStringLiteral("\u9009\u62e9"), this); file->addWidget(m_browse);
        form->addRow(QStringLiteral("CSV \u8bb0\u5f55"), file); layout->addLayout(form);
        auto* buttons = new QHBoxLayout;
        m_start = new QPushButton(QStringLiteral("\u542f\u52a8\u901a\u4fe1"), this); m_start->setObjectName(QStringLiteral("telemetryStart"));
        m_stop = new QPushButton(QStringLiteral("\u505c\u6b62\u901a\u4fe1"), this); m_stop->setObjectName(QStringLiteral("telemetryStop"));
        m_refresh = new QPushButton(QStringLiteral("\u5237\u65b0\u5de5\u4f5c\u7ad9"), this); m_refresh->setObjectName(QStringLiteral("telemetryRefresh"));
        buttons->addWidget(m_start); buttons->addWidget(m_stop); buttons->addWidget(m_refresh); layout->addLayout(buttons);
        m_sceneRobotFollow = new QCheckBox(QStringLiteral("\u5c06\u5b9e\u65f6\u5173\u8282\u89d2\u6620\u5c04\u5230\u573a\u666f\u673a\u5668\u4eba"), this);
        m_sceneRobotFollow->setObjectName(QStringLiteral("telemetrySceneRobotFollow"));
        m_sceneRobotFollow->setChecked(false);
        layout->addWidget(m_sceneRobotFollow);
        m_endEffectorTraceVisible = new QCheckBox(QStringLiteral("\u663e\u793a\u5b9e\u65f6\u672b\u7aef\u8f68\u8ff9"), this);
        m_endEffectorTraceVisible->setObjectName(QStringLiteral("telemetryEndEffectorTraceVisible"));
        m_endEffectorTraceVisible->setChecked(false);
        layout->addWidget(m_endEffectorTraceVisible);
        m_sceneFollowStatus = new QLabel(this);
        m_sceneFollowStatus->setObjectName(QStringLiteral("telemetrySceneFollowStatus"));
        m_sceneFollowStatus->setWordWrap(true);
        layout->addWidget(m_sceneFollowStatus);
        m_status = new QLabel(this); m_status->setObjectName(QStringLiteral("telemetryStatus")); m_status->setWordWrap(true); layout->addWidget(m_status);
        m_source = new QLabel(this); m_source->setWordWrap(true); m_source->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(m_source);
        m_time = new QLabel(this); m_time->setWordWrap(true); m_time->setObjectName(QStringLiteral("telemetryTime")); m_time->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(m_time);
        auto* tabs = new QTabWidget(this);
        tabs->setObjectName(QStringLiteral("telemetryDataTabs"));
        auto* jointPage = new QWidget(tabs);
        auto* jointLayout = new QVBoxLayout(jointPage);
        jointLayout->setContentsMargins(0, 0, 0, 0);
        m_joints = new QTableWidget(0, 2, jointPage); m_joints->setObjectName(QStringLiteral("telemetryJoints"));
        m_joints->setHorizontalHeaderLabels({QStringLiteral("\u5173\u8282"), QStringLiteral("\u63a7\u5236\u5668\u539f\u59cb\u503c")});
        m_joints->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_joints->verticalHeader()->hide();
        m_joints->verticalHeader()->setDefaultSectionSize(24);
        m_joints->setEditTriggers(QAbstractItemView::NoEditTriggers); jointLayout->addWidget(m_joints, 1);
        tabs->addTab(jointPage, QStringLiteral("\u5173\u8282"));
        auto* posePage = new QWidget(tabs);
        auto* poseLayout = new QVBoxLayout(posePage);
        poseLayout->setContentsMargins(0, 0, 0, 0);
        m_poseInfo = new QLabel(posePage);
        m_poseInfo->setObjectName(QStringLiteral("telemetryTcpInfo"));
        m_poseInfo->setWordWrap(true);
        m_poseInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        poseLayout->addWidget(m_poseInfo);
        m_pose = new QTableWidget(0, 2, posePage);
        m_pose->setObjectName(QStringLiteral("telemetryTcpPose"));
        m_pose->setHorizontalHeaderLabels({QStringLiteral("\u5206\u91cf"), QStringLiteral("\u5b9e\u65f6\u503c")});
        m_pose->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_pose->verticalHeader()->hide();
        m_pose->verticalHeader()->setDefaultSectionSize(24);
        m_pose->setEditTriggers(QAbstractItemView::NoEditTriggers);
        poseLayout->addWidget(m_pose, 1);
        tabs->addTab(posePage, QStringLiteral("\u672b\u7aef TCP \u4f4d\u59ff"));
        auto* speedBox = new QWidget(this);
        auto* speedLayout = new QVBoxLayout(speedBox);
        auto* speedControls = new QHBoxLayout;
        speedControls->addWidget(new QLabel(QStringLiteral("\u901f\u5ea6\u7edf\u8ba1\u65f6\u95f4\u6bb5 (s)"), speedBox));
        m_speedStart = new QDoubleSpinBox(speedBox); m_speedStart->setObjectName(QStringLiteral("telemetrySpeedStart")); m_speedStart->setRange(0., 1.e9); m_speedStart->setDecimals(3); m_speedStart->setSingleStep(.1); speedControls->addWidget(m_speedStart);
        speedControls->addWidget(new QLabel(QStringLiteral("\u81f3"), speedBox));
        m_speedEnd = new QDoubleSpinBox(speedBox); m_speedEnd->setObjectName(QStringLiteral("telemetrySpeedEnd")); m_speedEnd->setRange(0., 1.e9); m_speedEnd->setDecimals(3); m_speedEnd->setSingleStep(.1); speedControls->addWidget(m_speedEnd);
        speedControls->addWidget(new QLabel(QStringLiteral("\u65b9\u6cd5"), speedBox));
        m_speedMethod = new QComboBox(speedBox); m_speedMethod->setObjectName(QStringLiteral("telemetrySpeedMethod"));
        m_speedMethod->addItem(QStringLiteral("\u76f8\u90bb\u91c7\u6837\u77ac\u65f6\u5dee\u5206"), int(TelemetrySpeedCalculationMethod::InstantaneousDifference));
        m_speedMethod->addItem(QStringLiteral("\u65f6\u95f4\u7a97\u53e3\u5e73\u5747\u901f\u5ea6"), int(TelemetrySpeedCalculationMethod::WindowAverage));
        speedControls->addWidget(m_speedMethod);
        speedControls->addWidget(new QLabel(QStringLiteral("\u6700\u5c0f\u7a97\u53e3 (s)"), speedBox));
        m_speedMinimumInterval = new QDoubleSpinBox(speedBox); m_speedMinimumInterval->setObjectName(QStringLiteral("telemetrySpeedMinimumInterval")); m_speedMinimumInterval->setRange(.001, 1.e6); m_speedMinimumInterval->setDecimals(3); m_speedMinimumInterval->setValue(.1); m_speedMinimumInterval->setSingleStep(.01); speedControls->addWidget(m_speedMinimumInterval);
        auto* speedButton = new QPushButton(QStringLiteral("\u8ba1\u7b97\u901f\u5ea6"), speedBox); speedButton->setObjectName(QStringLiteral("telemetrySpeedCalculate")); speedControls->addWidget(speedButton);
        auto* openSpeedButton = new QPushButton(QStringLiteral("\u72ec\u7acb\u7a97\u53e3\u67e5\u770b"), speedBox); openSpeedButton->setObjectName(QStringLiteral("telemetrySpeedOpenWindow")); speedControls->addWidget(openSpeedButton); speedLayout->addLayout(speedControls);
        m_speedInfo = new QLabel(speedBox); m_speedInfo->setObjectName(QStringLiteral("telemetrySpeedInfo")); m_speedInfo->setWordWrap(true); speedLayout->addWidget(m_speedInfo);
        m_speedTabs = new QTabWidget(speedBox); m_speedTabs->setObjectName(QStringLiteral("telemetrySpeedTabs"));
        auto addMainPlot = [this](SpeedMetric metric, int jointIndex, const QString& label, const QString& objectName) {
            auto* plot = new TelemetrySpeedPlot(m_speedTabs, metric, jointIndex);
            plot->setObjectName(objectName);
            m_speedTabs->addTab(plot, label);
            m_speedPlots.push_back(plot);
        };
        addMainPlot(SpeedMetric::TcpLinear, 0, QStringLiteral("TCP \u7ebf\u901f\u5ea6"), QStringLiteral("telemetryTcpSpeedPlot"));
        addMainPlot(SpeedMetric::TcpAngular, 0, QStringLiteral("TCP \u89d2\u901f\u5ea6"), QStringLiteral("telemetryTcpAngularSpeedPlot"));
        for(int i = 0; i < 6; ++i) addMainPlot(SpeedMetric::Joint, i, QStringLiteral("J%1 \u5173\u8282\u901f\u5ea6").arg(i + 1), QStringLiteral("telemetryJoint%1SpeedPlot").arg(i + 1));
        speedLayout->addWidget(m_speedTabs, 1);
        speedBox->setObjectName(QStringLiteral("telemetrySpeedPage"));
        auto* mainTabs = new QTabWidget(this);
        mainTabs->setObjectName(QStringLiteral("telemetryMainTabs"));
        mainTabs->addTab(tabs, QStringLiteral("\u5b9e\u65f6\u6570\u636e"));
        mainTabs->addTab(speedBox, QStringLiteral("\u901f\u5ea6\u5206\u6790"));
        layout->addWidget(mainTabs, 1);
        connect(speedButton, &QPushButton::clicked, this, [this] { emit speedStatisticsRequested(m_speedStart->value(), m_speedEnd->value(), speedCalculationMethod(), speedMinimumIntervalSeconds()); });
        connect(openSpeedButton, &QPushButton::clicked, this, &DigitalTwinTelemetryPanel::openSpeedStatisticsWindow);
        auto* timeHint = new QLabel(QStringLiteral("\u65f6\u95f4\u6233\u4e3a\u7535\u8111\u63a5\u6536\u65f6\u523b\uff08UTC\uff09\u548c\u5355\u8c03\u8ba1\u65f6\u3002\u5b9e\u9645\u5468\u671f\u53d7\u63a7\u5236\u5668\u53ca\u901a\u4fe1\u8017\u65f6\u5f71\u54cd\uff1b\u5207\u6362\u5de5\u4f5c\u53f0\u540e\u7ee7\u7eed\u8bb0\u5f55\u3002"), this);
        timeHint->setWordWrap(true); layout->addWidget(timeHint);
        connect(m_start, &QPushButton::clicked, this, [this] {
            if(m_csv->text().trimmed().isEmpty() || !m_csv->isModified()) {
                const auto directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/RS2026/Telemetry");
                QDir().mkpath(directory);
                m_csv->setText(directory + QStringLiteral("/ROBOGUIDE_%1.csv").arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
            }
            emit startRequested();
        });
        connect(m_stop, &QPushButton::clicked, this, &DigitalTwinTelemetryPanel::stopRequested);
        connect(m_refresh, &QPushButton::clicked, this, &DigitalTwinTelemetryPanel::refreshRequested);
        connect(m_sceneRobotFollow, &QCheckBox::toggled, this, &DigitalTwinTelemetryPanel::sceneRobotFollowChanged);
        connect(m_endEffectorTraceVisible, &QCheckBox::toggled, this, &DigitalTwinTelemetryPanel::endEffectorTraceVisibilityChanged);
        connect(m_browse, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("\u9009\u62e9\u65b0\u7684\u91c7\u96c6\u6587\u4ef6"), m_csv->text(), QStringLiteral("CSV (*.csv)"));
            if(!path.isEmpty()) { m_csv->setText(path); m_csv->setModified(true); }
        });
        render({});
    }
    QString DigitalTwinTelemetryPanel::endpointId() const { return m_endpoints->currentData().toString(); }
    QString DigitalTwinTelemetryPanel::csvPath() const { return m_csv->text().trimmed(); }
    int DigitalTwinTelemetryPanel::group() const { return m_group->value(); }
    int DigitalTwinTelemetryPanel::intervalMs() const { return m_interval->value(); }
    double DigitalTwinTelemetryPanel::speedStartSeconds() const { return m_speedStart->value(); }
    double DigitalTwinTelemetryPanel::speedEndSeconds() const { return m_speedEnd->value(); }
    TelemetrySpeedCalculationMethod DigitalTwinTelemetryPanel::speedCalculationMethod() const { return static_cast<TelemetrySpeedCalculationMethod>(m_speedMethod->currentData().toInt()); }
    double DigitalTwinTelemetryPanel::speedMinimumIntervalSeconds() const { return m_speedMinimumInterval->value(); }
    bool DigitalTwinTelemetryPanel::sceneRobotFollowEnabled() const { return m_sceneRobotFollow->isChecked(); }
    bool DigitalTwinTelemetryPanel::endEffectorTraceVisible() const { return m_endEffectorTraceVisible->isChecked(); }
    void DigitalTwinTelemetryPanel::setSceneFollowStatus(const QString& message) {
        if(m_sceneFollowStatus->text() != message) m_sceneFollowStatus->setText(message);
    }
    void DigitalTwinTelemetryPanel::openSpeedStatisticsWindow() {
        if(!m_speedDialog) {
            auto* dialog = new TelemetrySpeedDialog(this);
            m_speedDialog = dialog;
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->onCalculate = [this](double start, double end, TelemetrySpeedCalculationMethod method, double minimum) { emit speedStatisticsRequested(start, end, method, minimum); };
            connect(dialog, &QObject::destroyed, this, [this] { m_speedDialog = nullptr; });
        }
        auto* dialog = static_cast<TelemetrySpeedDialog*>(m_speedDialog);
        const double maximum = std::max(1.0, m_speedEnd->maximum());
        dialog->setInterval(m_speedStart->value(), m_speedEnd->value(), maximum, speedCalculationMethod(), speedMinimumIntervalSeconds());
        dialog->setStatistics(m_lastSpeedStatistics);
        dialog->show(); dialog->raise(); dialog->activateWindow();
    }
    void DigitalTwinTelemetryPanel::renderSpeedStatistics(const TelemetrySpeedStatistics& statistics) {
        m_lastSpeedStatistics = statistics;
        for(auto* plot : m_speedPlots) static_cast<TelemetrySpeedPlot*>(plot)->setStatistics(statistics);
        m_speedInfo->setText(speedStatisticsText(statistics));
        if(m_speedDialog) static_cast<TelemetrySpeedDialog*>(m_speedDialog)->setStatistics(statistics);
    }
    void DigitalTwinTelemetryPanel::render(const TelemetrySnapshot& state) {
        bool changed = m_endpoints->count() != state.endpoints.size() + 1;
        for(int i = 0; !changed && i < state.endpoints.size(); ++i) changed = m_endpoints->itemData(i+1).toString() != state.endpoints[i].id;
        if(changed) {
            const auto previous = endpointId();
            m_endpoints->clear(); m_endpoints->addItem(QStringLiteral("\u81ea\u52a8\u9009\u62e9\u552f\u4e00\u8fd0\u884c\u63a7\u5236\u5668"), QString());
            for(const auto& endpoint : state.endpoints) m_endpoints->addItem(endpoint.name + QStringLiteral(" | ") + endpoint.id, endpoint.id);
            m_endpoints->setCurrentIndex(qMax(0, m_endpoints->findData(previous)));
            for(int i=1; i<m_endpoints->count(); ++i) m_endpoints->setItemData(i, m_endpoints->itemText(i), Qt::ToolTipRole);
        }
        m_start->setEnabled(!state.busy); m_refresh->setEnabled(!state.busy); m_stop->setEnabled(state.busy);
        m_group->setEnabled(!state.busy); m_interval->setEnabled(!state.busy); m_endpoints->setEnabled(!state.busy);
        m_csv->setEnabled(!state.busy); m_browse->setEnabled(!state.busy);
        m_status->setText((state.fault ? QStringLiteral("\u901a\u4fe1\u5f02\u5e38 / \u5df2\u505c\u6b62") : state.streaming ? QStringLiteral("\u6b63\u5728\u91c7\u96c6") : state.busy ? QStringLiteral("\u6b63\u5728\u53d1\u73b0 / \u8fde\u63a5") : QStringLiteral("\u672a\u91c7\u96c6"))
            + QStringLiteral("\n") + state.diagnostic);
        QString sourceName = state.source;
        sourceName.replace(QChar('\\'), QChar('/'));
        sourceName = sourceName.section(QChar('/'), -2, -1);
        m_source->setText(state.source.isEmpty() ? QString() : QStringLiteral("\u63a7\u5236\u5668\uff1a%1\nCSV\uff1a%2")
            .arg(sourceName, QFileInfo(state.csvPath).fileName()));
        m_source->setToolTip(state.source + QChar('\n') + state.csvPath);
        if(state.sequence) {
            if(state.elapsedNs > 0 && m_speedEnd->value() <= m_speedStart->value()) m_speedEnd->setValue(state.elapsedNs * 1e-9);
            m_speedEnd->setMaximum(std::max(1.0, state.elapsedNs * 1e-9 + 1.0));
            m_speedStart->setMaximum(m_speedEnd->maximum());
            m_time->setText(QStringLiteral("\u6837\u672c #%1 | %4 s\n\u63a5\u6536 UTC\uff1a%2\nUnix ns\uff1a%3\n\u8bfb\u53d6\u8017\u65f6\uff1a%5 ms%6")
                .arg(state.sequence).arg(QDateTime::fromMSecsSinceEpoch(state.receivedUnixNs / 1000000, Qt::UTC).toString(Qt::ISODateWithMs))
                .arg(state.receivedUnixNs).arg(state.elapsedNs * 1e-9, 0, 'f', 6).arg(state.readLatencyNs * 1e-6, 0, 'f', 3)
                .arg(state.streaming ? QString() : QStringLiteral("\n\u6700\u540e\u4e00\u5e27\uff08\u5df2\u505c\u6b62\u66f4\u65b0\uff09")));
        } else m_time->clear();
        if(state.tcpPoseValid) {
            m_poseInfo->setText(QStringLiteral("FANUC WORLD / UF%1 | \u5f53\u524d\u5de5\u5177 UT%2\nXYZ\uff1amm\uff0cWPR\uff1adeg%3")
                .arg(state.tcpUserFrame).arg(state.tcpUserTool)
                .arg(state.streaming ? QString() : QStringLiteral("\n\u6700\u540e\u4e00\u5e27\uff08\u5df2\u505c\u6b62\u66f4\u65b0\uff09")));
        } else m_poseInfo->setText(QStringLiteral("\u672b\u7aef TCP \u4f4d\u59ff\u4e0d\u53ef\u7528\n%1").arg(state.tcpDiagnostic));
        m_pose->setRowCount(state.tcpPoseValid ? 6 : 0);
        const QStringList components = {QStringLiteral("X (mm)"), QStringLiteral("Y (mm)"), QStringLiteral("Z (mm)"),
            QStringLiteral("W (deg)"), QStringLiteral("P (deg)"), QStringLiteral("R (deg)")};
        if(state.tcpPoseValid) for(int i = 0; i < 6; ++i) {
            m_pose->setItem(i, 0, new QTableWidgetItem(components[i]));
            m_pose->setItem(i, 1, new QTableWidgetItem(QString::number(state.tcpXyzwpr[i], 'f', 6)));
        }
        m_joints->setRowCount(int(state.joints.size()));
        for(int i=0; i<int(state.joints.size()); ++i) {
            m_joints->setItem(i, 0, new QTableWidgetItem(QStringLiteral("J%1").arg(i+1)));
            m_joints->setItem(i, 1, new QTableWidgetItem(QString::number(state.joints[i], 'f', 6)));
        }
    }
}
