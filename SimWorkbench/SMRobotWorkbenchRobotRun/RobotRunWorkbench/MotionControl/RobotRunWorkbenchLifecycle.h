#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"

namespace robotruntime
{
    class IRobotRunService;
}

namespace robot_qt_viewer
{
    class MotionControlModuleController;

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
        RobotQtViewerWorkbenchPackageRegistry& catalog,
        RobotQtViewerWorkbenchPackageSource source);
}
