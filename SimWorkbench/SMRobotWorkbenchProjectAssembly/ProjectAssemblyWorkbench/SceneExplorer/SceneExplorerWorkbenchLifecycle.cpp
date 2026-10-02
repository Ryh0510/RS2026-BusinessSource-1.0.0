#include "SceneExplorerWorkbenchLifecycle.h"

#include "SceneExplorerModuleController.h"
#include "SceneExplorerTaskWidget.h"
#include "SceneExplorerWidget.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentViewRegistry.h"

#include <QDockWidget>
#include <QMainWindow>

#include <utility>

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeOwnedSceneExplorerWorkbenchRuntimeContributionFactory(
        SceneExplorerWorkbenchComposition composition)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::Browse);
        return {
            workbenchId,
            [workbenchId, composition = std::move(composition)](QWidget* panelParent) {
                if(panelParent == nullptr || composition.documentContext == nullptr ||
                    composition.documentViewRegistry == nullptr ||
                    composition.sceneEntityWorkflow == nullptr ||
                    composition.assemblyViewport == nullptr ||
                    composition.mainWindow == nullptr || composition.statusPanel == nullptr) {
                    return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>();
                }

                auto* dock = new QDockWidget(composition.dockTitle, composition.mainWindow);
                auto* widget = new SceneExplorerWidget(dock);
                auto* taskWidget = new SceneExplorerTaskWidget(panelParent);
                auto* controller = new SceneExplorerModuleController(
                    *widget,
                    *composition.documentContext,
                    dock);
                controller->setTaskWidget(taskWidget);
                controller->setSceneEntityWorkflow(composition.sceneEntityWorkflow);
                controller->setAssemblyViewport(composition.assemblyViewport);
                if(composition.activeWorkbenchDescriptor) {
                    controller->setWorkbenchDescriptor(
                        composition.activeWorkbenchDescriptor());
                }
                dock->setWidget(widget);
                dock->setMinimumWidth(320);
                composition.mainWindow->addDockWidget(Qt::LeftDockWidgetArea, dock);

                QObject::connect(controller, &SceneExplorerModuleController::nodeActivated,
                    dock,
                    [callback = composition.nodeActivated](
                        const SceneExplorerNodeRef& node,
                        int column) {
                        if(callback) {
                            callback(node, column);
                        }
                    });
                QObject::connect(widget, &SceneExplorerWidget::nodeDoubleActivated,
                    dock,
                    [callback = composition.nodeDoubleActivated](
                        const SceneExplorerNodeRef& node,
                        int column) {
                        if(callback) {
                            callback(node, column);
                        }
                    });
                QObject::connect(
                    controller,
                    &SceneExplorerModuleController::contextMenuActionRequested,
                    dock,
                    [callback = composition.contextActionRequested](
                        const SceneTreeIntentController::ContextMenuAction& action) {
                        if(callback) {
                            callback(action);
                        }
                    });
                QObject::connect(controller, &SceneExplorerModuleController::statusMessageRequested,
                    dock,
                    [callback = composition.showStatus](const QString& message, int timeoutMs) {
                        if(callback) {
                            callback(message, timeoutMs);
                        }
                    });

                RobotQtViewerDocumentViewRegistry* const registry =
                    composition.documentViewRegistry;
                registry->registerModule(QStringLiteral("sceneExplorer"), controller,
                    [controller, selectionChanged = composition.selectionChanged](
                        const RobotQtViewerEvent& event) {
                        controller->handleEvent(event);
                        if(event.kind == RobotQtViewerEventKind::SelectionChanged &&
                            selectionChanged) {
                            selectionChanged();
                        }
                    });
                if(composition.bindDock) {
                    composition.bindDock(dock);
                }
                if(composition.bindShellPort) {
                    composition.bindShellPort(controller);
                }

                auto language = std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(*widget);
                language->addRoot(*taskWidget);
                return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>(
                    std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                        workbenchId,
                        [controller, taskWidget, statusPanel = composition.statusPanel]() -> QWidget* {
                            return controller->currentNode().kind == SceneExplorerNodeKind::Unknown
                                ? statusPanel
                                : taskWidget;
                        },
                        std::make_unique<SceneExplorerWorkbenchLifecycle>(*controller),
                        RobotQtViewerWorkbenchLifecyclePolicy{
                            RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                            RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                        std::move(language),
                        false,
                        [controller](const QString& defaultTitle) {
                            if(controller->currentNode().kind == SceneExplorerNodeKind::ObjectFrame) {
                                return QStringLiteral("Object Frame Editor");
                            }
                            if(controller->currentNode().kind == SceneExplorerNodeKind::RobotMount) {
                                return QStringLiteral("Mount Frame");
                            }
                            return defaultTitle;
                        },
                        [registry,
                         controller,
                         dock,
                         taskWidget,
                         bindDock = composition.bindDock,
                         bindPort = composition.bindShellPort]() {
                            registry->unregisterModule(controller);
                            if(bindPort) {
                                bindPort(nullptr);
                            }
                            if(bindDock) {
                                bindDock(nullptr);
                            }
                            delete dock;
                            delete taskWidget;
                        }));
            }
        };
    }

    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeSceneExplorerWorkbenchRuntimeContributionFactory(
        SceneExplorerModuleController& controller,
        QWidget& taskPanel,
        QWidget& statusPanel,
        QObject& sceneExplorerRoot,
        QObject& taskRoot)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::Browse);
        return {
            workbenchId,
            [workbenchId, &controller, &taskPanel, &statusPanel,
                &sceneExplorerRoot, &taskRoot](QWidget*) {
                auto language =
                    std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(sceneExplorerRoot);
                language->addRoot(taskRoot);
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&controller, &taskPanel, &statusPanel]() -> QWidget* {
                        return controller.currentNode().kind == SceneExplorerNodeKind::Unknown
                            ? &statusPanel
                            : &taskPanel;
                    },
                    std::make_unique<SceneExplorerWorkbenchLifecycle>(controller),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                        RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                    std::move(language),
                    false,
                    [&controller](const QString& defaultTitle) {
                        if(controller.currentNode().kind == SceneExplorerNodeKind::ObjectFrame) {
                            return QStringLiteral("Object Frame Editor");
                        }
                        if(controller.currentNode().kind == SceneExplorerNodeKind::RobotMount) {
                            return QStringLiteral("Mount Frame");
                        }
                        return defaultTitle;
                    });
            }
        };
    }

    SceneExplorerWorkbenchLifecycle::SceneExplorerWorkbenchLifecycle(
        SceneExplorerModuleController& controller)
        : m_controller(controller)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    SceneExplorerWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext& context)
    {
        RobotQtViewerEditTransitionRequest request;
        request.cause = context.cause;
        request.projectGeneration = context.projectGeneration;
        request.sourceId = context.sourceId;
        request.promptParent = context.promptParent;
        request.restoreEditorTarget =
            context.cause != RobotQtViewerWorkbenchTransitionCause::ProjectReplacing &&
            context.cause != RobotQtViewerWorkbenchTransitionCause::ApplicationShutdown;
        return prepareTransition(request);
    }

    RobotQtViewerWorkbenchTransitionResult SceneExplorerWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        if(m_controller.hasPendingTransformPreview()) {
            return workbenchTransitionFailed(
                QStringLiteral("Project Assembly still has pending changes after preparation."),
                QStringLiteral("project_assembly.pending_after_prepare"));
        }
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult SceneExplorerWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        return workbenchTransitionSucceeded();
    }

    void SceneExplorerWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept
    {
        releaseEditSession(context.nextProjectGeneration);
    }

    void SceneExplorerWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
        m_controller.releaseProjectState();
    }

    QString SceneExplorerWorkbenchLifecycle::ownerWorkbenchId() const
    {
        return robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
    }

    QString SceneExplorerWorkbenchLifecycle::taskId() const
    {
        return QStringLiteral("project_assembly.transform_task");
    }

    RobotQtViewerEditSessionState SceneExplorerWorkbenchLifecycle::editSessionState() const
    {
        return m_controller.hasPendingTransformPreview()
            ? RobotQtViewerEditSessionState::Dirty
            : RobotQtViewerEditSessionState::Clean;
    }

    RobotQtViewerWorkbenchTransitionResult SceneExplorerWorkbenchLifecycle::prepareTransition(
        const RobotQtViewerEditTransitionRequest& request)
    {
        if(!m_controller.hasPendingTransformPreview()) {
            return workbenchTransitionSucceeded();
        }
        if(!m_controller.resolvePendingTransformPreview(request.promptParent)) {
            return workbenchTransitionRejected(
                QStringLiteral("Project Assembly has unresolved transform changes."),
                QStringLiteral("project_assembly.pending_changes"));
        }
        return workbenchTransitionSucceeded();
    }

    void SceneExplorerWorkbenchLifecycle::releaseEditSession(std::uint64_t) noexcept
    {
        m_controller.releaseProjectState();
    }
}
