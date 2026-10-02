#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "RobotRunWorkbenchShellPort.h"

#include <functional>

class QObject;
class QDockWidget;
class QMainWindow;
class QWidget;

namespace robotruntime
{
    class IRobotRunService;
}

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;
    class RobotQtViewerDocumentViewRegistry;
    class MotionControlModuleController;

    struct RobotRunWorkbenchComposition
    {
        RobotQtViewerDocumentContext* documentContext = nullptr;
        RobotQtViewerDocumentViewRegistry* documentViewRegistry = nullptr;
        robotruntime::IRobotRunService* runService = nullptr;
        QMainWindow* mainWindow = nullptr;
        QString detailsTitle;
        std::function<void(const QString&, int)> showStatus;
        std::function<void(bool)> collisionQueriesChanged;
        std::function<void(bool)> collisionGeometryVisibilityChanged;
        std::function<void(bool)> detailsVisibilityChanged;
        std::function<void(QDockWidget*)> bindDetailsDock;
        std::function<void(RobotRunWorkbenchShellPort*)> bindShellPort;
    };

    class RobotRunWorkbenchLifecycle final : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        RobotRunWorkbenchLifecycle(
            MotionControlModuleController& controller,
            robotruntime::IRobotRunService& runService);

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

    private:
        RobotQtViewerWorkbenchTransitionResult quiesce();

        MotionControlModuleController& m_controller;
        robotruntime::IRobotRunService& m_runService;
    };

    bool registerRobotRunWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeRobotRunWorkbenchRuntimeContributionFactory(
            MotionControlModuleController& controller,
            robotruntime::IRobotRunService& runService,
            QWidget& taskPanel,
            QObject& motionControlRoot,
            QObject* collisionDetailsRoot);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeOwnedRobotRunWorkbenchRuntimeContributionFactory(
            RobotRunWorkbenchComposition composition);
}
