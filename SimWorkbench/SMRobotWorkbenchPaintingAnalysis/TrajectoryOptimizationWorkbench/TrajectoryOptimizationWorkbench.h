#pragma once

#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include <QJsonObject>
#include <QObject>
#include <QWidget>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentViewRegistry;

    struct TrajectoryOptimizationComposition
    {
        RobotQtViewerDocumentViewRegistry* registry = nullptr;
        std::function<void(QWidget*)> installLeftPanel;
        std::function<QJsonObject()> coatingSettings;
    };

    // UI inputs only. The session/controller below owns requests and results.
    class TrajectoryOptimizationPanel final : public QWidget
    {
        Q_OBJECT
    public:
        explicit TrajectoryOptimizationPanel(QWidget* parent = nullptr);
        QWidget* inputs = nullptr;
        QLineEdit *modelPath = nullptr, *trajectoryPath = nullptr;
        QLineEdit *outputPath = nullptr;
        QComboBox *object = nullptr, *predictionModel = nullptr;
        QCheckBox *shareCoating = nullptr, *useHistory = nullptr, *useBvh = nullptr, *autoFlip = nullptr;
        QDoubleSpinBox *tolerance = nullptr, *target = nullptr, *timeStep = nullptr;
        QPushButton *optimize = nullptr, *compare = nullptr, *cancel = nullptr, *openOutput = nullptr;
        QLabel *status = nullptr, *statistics = nullptr, *scope = nullptr;
        QProgressBar* progress = nullptr;
        QPlainTextEdit* log = nullptr;
    signals:
        void optimizeRequested();
        void compareRequested();
        void cancelRequested();
        void openOutputRequested();
        void inputsChanged();
    };

    struct TrajectoryOptimizationSession
    {
        QJsonObject request;
        QJsonObject result;
        QString runDirectory;
        QString status;
        bool busy = false;
        bool cancelled = false;
        bool comparing = false;
    };

    class TrajectoryOptimizationController final : public QObject
    {
        Q_OBJECT
    public:
        TrajectoryOptimizationController(TrajectoryOptimizationPanel& panel,
            std::function<QJsonObject()> coatingSettings, QObject* parent = nullptr);
        ~TrajectoryOptimizationController() override;
        void clear();
        void setActive(bool active);
        bool busy() const;
        const TrajectoryOptimizationSession& session() const;
    public slots:
        void optimize();
        void compare();
        void cancel();
    private:
        QJsonObject predictionSnapshot() const;
        void launch(bool compareOnly);
        void finish(const QJsonObject& result, const QString& error);
        void refresh();
        void showGraph();
        TrajectoryOptimizationPanel& m_panel;
        std::function<QJsonObject()> m_coatingSettings;
        TrajectoryOptimizationSession m_session;
        std::thread m_worker;
        std::shared_ptr<std::atomic_bool> m_cancelled;
        quint64 m_generation = 0;
        bool m_active = false;
    };

    bool registerTrajectoryOptimizationWorkbench(RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeTrajectoryOptimizationWorkbenchFactory(TrajectoryOptimizationComposition composition);
}
