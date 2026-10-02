#pragma once

#include "RobotQtViewerEditSession.h"
#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchLifecycle.h"
#include "SceneExplorerWorkbenchShellPort.h"
#include "SceneTreeIntentController.h"

#include <functional>

class QObject;
class QDockWidget;
class QMainWindow;
class QWidget;

namespace robot_qt_viewer
{
    class SceneExplorerModuleController;
    class RobotQtViewerDocumentContext;
    class RobotQtViewerDocumentViewRegistry;
    class IRobotQtViewerAssemblyViewportPort;
    class SceneEntityWorkflowController;

    struct SceneExplorerWorkbenchComposition
    {
        using NodeHandler = std::function<void(const SceneExplorerNodeRef&, int)>;
        using ContextActionHandler =
            std::function<void(const SceneTreeIntentController::ContextMenuAction&)>;

        RobotQtViewerDocumentContext* documentContext = nullptr;
        RobotQtViewerDocumentViewRegistry* documentViewRegistry = nullptr;
        SceneEntityWorkflowController* sceneEntityWorkflow = nullptr;
        IRobotQtViewerAssemblyViewportPort* assemblyViewport = nullptr;
        QMainWindow* mainWindow = nullptr;
        QWidget* statusPanel = nullptr;
        QString dockTitle;
        std::function<RobotQtViewerWorkbenchDescriptor()> activeWorkbenchDescriptor;
        NodeHandler nodeActivated;
        NodeHandler nodeDoubleActivated;
        ContextActionHandler contextActionRequested;
        std::function<void(const QString&, int)> showStatus;
        std::function<void()> selectionChanged;
        std::function<void(QDockWidget*)> bindDock;
        std::function<void(SceneExplorerWorkbenchShellPort*)> bindShellPort;
    };

    class SceneExplorerWorkbenchLifecycle final
        : public IRobotQtViewerWorkbenchLifecycle
        , public IWorkbenchEditSession
    {
    public:
        explicit SceneExplorerWorkbenchLifecycle(SceneExplorerModuleController& controller);

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
        SceneExplorerModuleController& m_controller;
    };

    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeSceneExplorerWorkbenchRuntimeContributionFactory(
            SceneExplorerModuleController& controller,
            QWidget& taskPanel,
            QWidget& statusPanel,
            QObject& sceneExplorerRoot,
            QObject& taskRoot);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeOwnedSceneExplorerWorkbenchRuntimeContributionFactory(
            SceneExplorerWorkbenchComposition composition);
}
