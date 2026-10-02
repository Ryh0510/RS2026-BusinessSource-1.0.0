#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerEditSession.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "ToolSetupWorkbenchShellPort.h"

#include <functional>

class QObject;
class QWidget;

namespace robot_qt_viewer
{
    class ToolSetupModuleController;
    class RobotQtViewerDocumentContext;
    class RobotQtViewerDocumentViewRegistry;
    class ToolSetupAppServices;

    struct ToolSetupWorkbenchComposition
    {
        RobotQtViewerDocumentContext* documentContext = nullptr;
        RobotQtViewerDocumentViewRegistry* documentViewRegistry = nullptr;
        ToolSetupAppServices* appServices = nullptr;
        std::function<QString()> selectedRobotId;
        std::function<QString()> selectedLinkName;
        std::function<void(const QString&, const QString&, const QString&)> focusMountFrame;
        std::function<void(const QString&, const QString&)> focusLink;
        std::function<void()> refreshSelectionDependentViews;
        std::function<void(const QString&, int)> showStatus;
        std::function<void(bool)> taskDirtyChanged;
        std::function<void()> saveProject;
        std::function<void()> requestTaskExit;
        std::function<void(ToolSetupWorkbenchShellPort*)> bindShellPort;
    };

    class ToolSetupWorkbenchLifecycle final
        : public IRobotQtViewerWorkbenchLifecycle
        , public IWorkbenchEditSession
    {
    public:
        explicit ToolSetupWorkbenchLifecycle(ToolSetupModuleController& controller);

        RobotQtViewerWorkbenchTransitionResult prepareDeactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override;
        RobotQtViewerWorkbenchTransitionResult deactivate(
            const RobotQtViewerWorkbenchTransitionContext& context) override;
        RobotQtViewerWorkbenchTransitionResult activate(
            const RobotQtViewerWorkbenchActivationContext& context) override;
        void releaseProject(
            const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept override;
        void shutdown(
            const RobotQtViewerWorkbenchShutdownContext& context) noexcept override;

        QString ownerWorkbenchId() const override;
        QString taskId() const override;
        RobotQtViewerEditSessionState editSessionState() const override;
        RobotQtViewerWorkbenchTransitionResult prepareTransition(
            const RobotQtViewerEditTransitionRequest& request) override;
        void releaseEditSession(std::uint64_t nextProjectGeneration) noexcept override;

    private:
        ToolSetupModuleController& m_controller;
    };

    bool registerProjectAssemblyWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeToolSetupWorkbenchRuntimeContributionFactory(
            ToolSetupModuleController& controller,
            QWidget& taskPanel,
            QObject& languageRoot);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeOwnedToolSetupWorkbenchRuntimeContributionFactory(
            ToolSetupWorkbenchComposition composition);
}
