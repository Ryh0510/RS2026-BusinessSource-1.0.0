#include "ToolSetupWorkbenchLifecycle.h"

#include "ToolSetupModuleController.h"
#include "ToolSetupWidget.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentViewRegistry.h"

#include <utility>

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeOwnedToolSetupWorkbenchRuntimeContributionFactory(
        ToolSetupWorkbenchComposition composition)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::ToolSetup);
        return {
            workbenchId,
            [workbenchId, composition = std::move(composition)](QWidget* panelParent) {
                if(panelParent == nullptr || composition.documentContext == nullptr ||
                    composition.documentViewRegistry == nullptr ||
                    composition.appServices == nullptr) {
                    return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>();
                }

                auto* widget = new ToolSetupWidget(panelParent);
                auto* controller = new ToolSetupModuleController(
                    *widget,
                    *composition.documentContext,
                    *composition.appServices,
                    widget);

                QObject::connect(controller, &ToolSetupModuleController::mountFrameFocusRequested,
                    widget,
                    [callback = composition.focusMountFrame](
                        const QString& robotId,
                        const QString& linkName,
                        const QString& mountId) {
                        if(callback) {
                            callback(robotId, linkName, mountId);
                        }
                    });
                QObject::connect(controller, &ToolSetupModuleController::linkFocusRequested,
                    widget,
                    [callback = composition.focusLink](
                        const QString& robotId,
                        const QString& linkName) {
                        if(callback) {
                            callback(robotId, linkName);
                        }
                    });
                QObject::connect(
                    controller,
                    &ToolSetupModuleController::selectionDependentViewsRefreshRequested,
                    widget,
                    [callback = composition.refreshSelectionDependentViews]() {
                        if(callback) {
                            callback();
                        }
                    });
                QObject::connect(controller, &ToolSetupModuleController::statusMessageRequested,
                    widget,
                    [callback = composition.showStatus](const QString& message, int timeoutMs) {
                        if(callback) {
                            callback(message, timeoutMs);
                        }
                    });
                QObject::connect(controller, &ToolSetupModuleController::viewModelRefreshed,
                    widget,
                    [controller]() {
                        controller->updateToolFrameVisibility();
                    });
                QObject::connect(controller, &ToolSetupModuleController::taskDirtyChanged,
                    widget,
                    [callback = composition.taskDirtyChanged](bool dirty) {
                        if(callback) {
                            callback(dirty);
                        }
                    });
                QObject::connect(widget, &ToolSetupWidget::saveProjectRequested,
                    widget,
                    [callback = composition.saveProject]() {
                        if(callback) {
                            callback();
                        }
                    });
                QObject::connect(controller, &ToolSetupModuleController::taskExitRequested,
                    widget,
                    [callback = composition.requestTaskExit]() {
                        if(callback) {
                            callback();
                        }
                    });

                RobotQtViewerDocumentViewRegistry* const registry =
                    composition.documentViewRegistry;
                registry->registerModule(QStringLiteral("toolSetup"), controller,
                    [controller,
                     selectedRobotId = composition.selectedRobotId,
                     selectedLinkName = composition.selectedLinkName](
                        const RobotQtViewerEvent& event) {
                        const QString eventRobotId = !event.selection.robotId.isEmpty()
                            ? event.selection.robotId
                            : (selectedRobotId ? selectedRobotId() : QString());
                        const QString eventLinkName = !event.selection.linkName.isEmpty()
                            ? event.selection.linkName
                            : (selectedLinkName ? selectedLinkName() : QString());
                        controller->handleEvent(event, eventRobotId, eventLinkName);
                    });
                if(composition.bindShellPort) {
                    composition.bindShellPort(controller);
                }

                auto language = std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(*widget);
                return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>(
                    std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                        workbenchId,
                        [widget]() { return widget; },
                        std::make_unique<ToolSetupWorkbenchLifecycle>(*controller),
                        RobotQtViewerWorkbenchLifecyclePolicy{
                            RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                            RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                        std::move(language),
                        false,
                        RobotQtViewerWorkbenchRuntimeContribution::PanelTitleResolver{},
                        [registry,
                         controller,
                         widget,
                         bindPort = composition.bindShellPort]() {
                            registry->unregisterModule(controller);
                            if(bindPort) {
                                bindPort(nullptr);
                            }
                            delete widget;
                        }));
            }
        };
    }

    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeToolSetupWorkbenchRuntimeContributionFactory(
        ToolSetupModuleController& controller,
        QWidget& taskPanel,
        QObject& languageRoot)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::ToolSetup);
        return {
            workbenchId,
            [workbenchId, &controller, &taskPanel, &languageRoot](QWidget*) {
                auto language =
                    std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(languageRoot);
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&taskPanel]() { return &taskPanel; },
                    std::make_unique<ToolSetupWorkbenchLifecycle>(controller),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                        RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                    std::move(language));
            }
        };
    }

    bool registerProjectAssemblyWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.project-assembly");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Project Assembly")))) {
            return false;
        }
        if(!catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::Browse,
               QStringLiteral("projectAssemblyWorkbench"),
               10,
               { QStringLiteral("smrobot.feature.project-assembly") })) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::ToolSetup,
               QStringLiteral("toolSetupWorkbench"),
               20,
               { QStringLiteral("smrobot.feature.tool-setup") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
                   QStringLiteral("smrobot.feature.project-assembly"),
                   QStringLiteral("Project Assembly"),
                   packageId,
                   { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) })) &&
            catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
                   QStringLiteral("smrobot.feature.tool-setup"),
                   QStringLiteral("Tool Setup"),
                   packageId,
                   { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::ToolSetup) }));
    }

    ToolSetupWorkbenchLifecycle::ToolSetupWorkbenchLifecycle(
        ToolSetupModuleController& controller)
        : m_controller(controller)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    ToolSetupWorkbenchLifecycle::prepareDeactivate(
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

    RobotQtViewerWorkbenchTransitionResult ToolSetupWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        if(m_controller.hasPendingTaskChanges()) {
            return workbenchTransitionFailed(
                QStringLiteral("Tool Setup still has pending changes after preparation."),
                QStringLiteral("tool_setup.pending_after_prepare"));
        }
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult ToolSetupWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.updateToolFrameVisibility();
        return workbenchTransitionSucceeded();
    }

    void ToolSetupWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept
    {
        releaseEditSession(context.nextProjectGeneration);
    }

    void ToolSetupWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
        m_controller.releaseProjectState();
    }

    QString ToolSetupWorkbenchLifecycle::ownerWorkbenchId() const
    {
        return robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::ToolSetup);
    }

    QString ToolSetupWorkbenchLifecycle::taskId() const
    {
        return QStringLiteral("tool_setup.task");
    }

    RobotQtViewerEditSessionState ToolSetupWorkbenchLifecycle::editSessionState() const
    {
        return m_controller.hasPendingTaskChanges()
            ? RobotQtViewerEditSessionState::Dirty
            : RobotQtViewerEditSessionState::Clean;
    }

    RobotQtViewerWorkbenchTransitionResult ToolSetupWorkbenchLifecycle::prepareTransition(
        const RobotQtViewerEditTransitionRequest& request)
    {
        if(!m_controller.hasPendingTaskChanges()) {
            return workbenchTransitionSucceeded();
        }
        if(!m_controller.resolvePendingTaskChanges(
               request.promptParent,
               request.restoreEditorTarget)) {
            return workbenchTransitionRejected(
                QStringLiteral("Tool Setup has unresolved changes."),
                QStringLiteral("tool_setup.pending_changes"));
        }
        return workbenchTransitionSucceeded();
    }

    void ToolSetupWorkbenchLifecycle::releaseEditSession(std::uint64_t) noexcept
    {
        m_controller.releaseProjectState();
    }
}
