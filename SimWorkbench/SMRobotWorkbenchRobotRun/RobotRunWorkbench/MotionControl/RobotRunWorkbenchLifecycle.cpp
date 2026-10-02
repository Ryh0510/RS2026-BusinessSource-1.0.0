#include "RobotRunWorkbenchLifecycle.h"

#include "MotionControlModuleController.h"
#include "MotionControlWidget.h"

#include "CollisionRuntimeResultsWidget.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentViewRegistry.h"

#include <RobotRuntime/RobotRunService.h>

#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolButton>
#include <QVariant>

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeOwnedRobotRunWorkbenchRuntimeContributionFactory(
        RobotRunWorkbenchComposition composition)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::Motion);
        return {
            workbenchId,
            [workbenchId, composition = std::move(composition)](QWidget* panelParent) {
                if(panelParent == nullptr || composition.documentContext == nullptr ||
                    composition.documentViewRegistry == nullptr ||
                    composition.runService == nullptr || composition.mainWindow == nullptr) {
                    return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>();
                }
                auto* panel = new QScrollArea(panelParent);
                panel->setMinimumWidth(0);
                panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                panel->setWidgetResizable(true);
                panel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                panel->setFrameShape(QFrame::NoFrame);
                auto* widget = new MotionControlWidget(panel);
                auto* controller = new MotionControlModuleController(
                    *widget,
                    *composition.documentContext,
                    *composition.runService,
                    panel);
                panel->setWidget(widget);

                auto* detailsDock = new QDockWidget(composition.detailsTitle, composition.mainWindow);
                constexpr int expandedHeight = 280;
                constexpr int collapsedHeight = 36;
                auto* titleBar = new QWidget(detailsDock);
                auto* titleLayout = new QHBoxLayout(titleBar);
                titleLayout->setContentsMargins(8, 2, 4, 2);
                titleLayout->setSpacing(4);
                auto* title = new QLabel(composition.detailsTitle, titleBar);
                title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                auto* collapseButton = new QToolButton(titleBar);
                collapseButton->setText(QStringLiteral("-"));
                collapseButton->setToolTip(QStringLiteral("Collapse Robot Run Details"));
                collapseButton->setAutoRaise(true);
                auto* closeButton = new QToolButton(titleBar);
                closeButton->setText(QStringLiteral("x"));
                closeButton->setToolTip(QStringLiteral("Close Robot Run Details"));
                closeButton->setAutoRaise(true);
                titleLayout->addWidget(title, 1);
                titleLayout->addWidget(collapseButton);
                titleLayout->addWidget(closeButton);
                detailsDock->setTitleBarWidget(titleBar);
                auto* details = new CollisionRuntimeResultsWidget(detailsDock);
                details->setDisplayMode(CollisionResultsWidget::DisplayMode::Full);
                details->setMinimumHeight(expandedHeight - 32);
                detailsDock->setWidget(details);
                detailsDock->setMinimumHeight(expandedHeight);
                detailsDock->resize(detailsDock->width(), expandedHeight);
                detailsDock->setFeatures(
                    QDockWidget::DockWidgetClosable |
                    QDockWidget::DockWidgetMovable |
                    QDockWidget::DockWidgetFloatable);
                detailsDock->setProperty("robotRunDetailsCollapsed", false);
                detailsDock->setVisible(false);
                composition.mainWindow->addDockWidget(Qt::BottomDockWidgetArea, detailsDock);
                controller->setCollisionDetailsWidget(details->resultsWidget());

                QObject::connect(collapseButton, &QToolButton::clicked, detailsDock,
                    [detailsDock, collapseButton, expandedHeight, collapsedHeight]() {
                        const bool collapsed =
                            detailsDock->property("robotRunDetailsCollapsed").toBool();
                        if(QWidget* dockWidget = detailsDock->widget()) {
                            dockWidget->setVisible(collapsed);
                        }
                        detailsDock->setMinimumHeight(collapsed ? expandedHeight : collapsedHeight);
                        detailsDock->setMaximumHeight(collapsed ? 16777215 : collapsedHeight);
                        detailsDock->resize(
                            detailsDock->width(),
                            collapsed ? expandedHeight : collapsedHeight);
                        collapseButton->setText(collapsed ? QStringLiteral("-") : QStringLiteral("+"));
                        collapseButton->setToolTip(collapsed
                            ? QStringLiteral("Collapse Robot Run Details")
                            : QStringLiteral("Restore Robot Run Details"));
                        detailsDock->setProperty("robotRunDetailsCollapsed", !collapsed);
                    });
                QObject::connect(closeButton, &QToolButton::clicked, detailsDock, &QDockWidget::hide);
                QObject::connect(widget, &MotionControlWidget::collisionDetailsRequested,
                    detailsDock,
                    [detailsDock, controller]() {
                        detailsDock->show();
                        detailsDock->raise();
                        controller->refreshCollisionMonitor();
                    });
                QObject::connect(detailsDock, &QDockWidget::visibilityChanged, detailsDock,
                    [callback = composition.detailsVisibilityChanged](bool visible) {
                        if(callback) {
                            callback(visible);
                        }
                    });
                QObject::connect(controller, &MotionControlModuleController::statusMessageRequested,
                    panel,
                    [callback = composition.showStatus](const QString& message, int timeoutMs) {
                        if(callback) {
                            callback(message, timeoutMs);
                        }
                    });
                QObject::connect(controller, &MotionControlModuleController::collisionQueriesEnabledChanged,
                    panel,
                    [callback = composition.collisionQueriesChanged](bool enabled) {
                        if(callback) {
                            callback(enabled);
                        }
                    });
                QObject::connect(controller, &MotionControlModuleController::collisionGeometryVisibleChanged,
                    panel,
                    [callback = composition.collisionGeometryVisibilityChanged](bool visible) {
                        if(callback) {
                            callback(visible);
                        }
                    });
                RobotQtViewerDocumentViewRegistry* const registry = composition.documentViewRegistry;
                registry->registerModule(QStringLiteral("motion"), controller,
                    [controller](const RobotQtViewerEvent& event) {
                        controller->handleEvent(event);
                    });
                if(composition.bindDetailsDock) {
                    composition.bindDetailsDock(detailsDock);
                }
                if(composition.bindShellPort) {
                    composition.bindShellPort(controller);
                }
                auto language = std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(*widget);
                language->addRoot(*details);
                return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>(
                    std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                        workbenchId,
                        [panel]() { return panel; },
                        std::make_unique<RobotRunWorkbenchLifecycle>(
                            *controller,
                            *composition.runService),
                        RobotQtViewerWorkbenchLifecyclePolicy{
                            RobotQtViewerWorkbenchExecutionPolicy::MustQuiesce,
                            RobotQtViewerWorkbenchReactivationPolicy::ManualResume },
                        std::move(language),
                        false,
                        RobotQtViewerWorkbenchRuntimeContribution::PanelTitleResolver{},
                        [registry,
                         controller,
                         panel,
                         detailsDock,
                         bindDock = composition.bindDetailsDock,
                         bindPort = composition.bindShellPort]() {
                            registry->unregisterModule(controller);
                            if(bindPort) {
                                bindPort(nullptr);
                            }
                            if(bindDock) {
                                bindDock(nullptr);
                            }
                            delete detailsDock;
                            delete panel;
                        }));
            }
        };
    }

    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeRobotRunWorkbenchRuntimeContributionFactory(
        MotionControlModuleController& controller,
        robotruntime::IRobotRunService& runService,
        QWidget& taskPanel,
        QObject& motionControlRoot,
        QObject* collisionDetailsRoot)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::Motion);
        return {
            workbenchId,
            [workbenchId, &controller, &runService, &taskPanel,
                &motionControlRoot, collisionDetailsRoot](QWidget*) {
                auto language =
                    std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(motionControlRoot);
                if(collisionDetailsRoot != nullptr) {
                    language->addRoot(*collisionDetailsRoot);
                }
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&taskPanel]() { return &taskPanel; },
                    std::make_unique<RobotRunWorkbenchLifecycle>(controller, runService),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::MustQuiesce,
                        RobotQtViewerWorkbenchReactivationPolicy::ManualResume },
                    std::move(language));
            }
        };
    }

    bool registerRobotRunWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.robot-run");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Robot Run"))) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::Motion,
               QStringLiteral("robotRunWorkbench"),
               30,
               { QStringLiteral("smrobot.feature.robot-run") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.robot-run"),
            QStringLiteral("Robot Run"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Motion) }));
    }

    RobotRunWorkbenchLifecycle::RobotRunWorkbenchLifecycle(
        MotionControlModuleController& controller,
        robotruntime::IRobotRunService& runService)
        : m_controller(controller)
        , m_runService(runService)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    RobotRunWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult RobotRunWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        const RobotQtViewerWorkbenchTransitionResult result = quiesce();
        if(result.succeeded()) {
            m_controller.setWorkbenchActive(false);
        }
        return result;
    }

    RobotQtViewerWorkbenchTransitionResult RobotRunWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.setWorkbenchActive(true);
        m_controller.updateRowsFromScene();
        m_controller.refreshTrajectories();
        m_controller.refreshCollisionMonitor();
        return workbenchTransitionSucceeded();
    }

    void RobotRunWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
    {
        m_controller.setWorkbenchActive(false);
        m_controller.clearRuntime();
    }

    void RobotRunWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
        m_controller.setWorkbenchActive(false);
        quiesce();
    }

    RobotQtViewerWorkbenchTransitionResult RobotRunWorkbenchLifecycle::quiesce()
    {
        m_controller.stopAllAutoMotion();
        const robotruntime::RobotRunExecutionSnapshot snapshot = m_runService.trajectorySnapshot();
        if(snapshot.state != robotruntime::RobotRunExecutionState::Running) {
            return workbenchTransitionSucceeded();
        }
        const robotruntime::RobotRunCommandResult pause = m_runService.pauseTrajectory();
        if(!pause.success) {
            return workbenchTransitionFailed(
                QString::fromStdString(pause.message),
                QStringLiteral("robot_run.trajectory.pause_failed"));
        }
        return workbenchTransitionSucceeded(QString::fromStdString(pause.message));
    }
}
