#include "TrajectoryOptimizationWorkbench.h"
#include "RobotQtViewerDocumentViewRegistry.h"
#include <QScrollArea>
#include <QVBoxLayout>

namespace robot_qt_viewer
{
    bool registerTrajectoryOptimizationWorkbench(RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString package = QStringLiteral("smrobot.workbench.painting-analysis");
        RobotQtViewerWorkbenchDescriptor descriptor;
        descriptor.domain = RobotQtViewerWorkbenchDomain::CoatingAnalysis;
        descriptor.projectAssemblyTreeProjection = false; descriptor.projectAssemblyActions = false;
        descriptor.id = QStringLiteral("smrobot.mode.trajectory-optimization");
        descriptor.displayName = QStringLiteral("轨迹优化"); descriptor.rightPanelTitle = descriptor.displayName;
        return catalog.registerWorkbench(makeRobotQtViewerWorkbench(package, descriptor,
            QStringLiteral("trajectoryOptimizationWorkbench"), 72, {},
            {robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::CoatingAnalysis)}));
    }
    class OptimizationLifecycle final : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        explicit OptimizationLifecycle(TrajectoryOptimizationController& controller) : m_controller(controller) {}
        RobotQtViewerWorkbenchTransitionResult prepareDeactivate(const RobotQtViewerWorkbenchTransitionContext&) override
        {
            return m_controller.busy() ? workbenchTransitionRejected(QStringLiteral("轨迹优化计算中，请先完成或取消。")) : workbenchTransitionSucceeded();
        }
        RobotQtViewerWorkbenchTransitionResult activate(const RobotQtViewerWorkbenchActivationContext&) override
        { m_controller.setActive(true); return workbenchTransitionSucceeded(); }
        RobotQtViewerWorkbenchTransitionResult deactivate(const RobotQtViewerWorkbenchTransitionContext&) override
        { m_controller.setActive(false); return workbenchTransitionSucceeded(); }
        void releaseProject(const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept override { m_controller.clear(); }
        void shutdown(const RobotQtViewerWorkbenchShutdownContext&) noexcept override { m_controller.clear(); }
    private:
        TrajectoryOptimizationController& m_controller;
    };
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeTrajectoryOptimizationWorkbenchFactory(TrajectoryOptimizationComposition composition)
    {
        const QString id = QStringLiteral("smrobot.mode.trajectory-optimization");
        return {id, [id, composition](QWidget* parent) -> std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution> {
            auto* panel = new TrajectoryOptimizationPanel(parent);
            auto* scroll = new QScrollArea(parent); scroll->setWidgetResizable(true); scroll->setWidget(panel);
            if(composition.installLeftPanel) composition.installLeftPanel(panel->inputs);
            panel->inputs->hide();
            auto* controller = new TrajectoryOptimizationController(*panel, composition.coatingSettings, panel);
            auto language = std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
            language->addRoot(*panel); language->addRoot(*panel->inputs);
            auto* registry = composition.registry;
            if(registry) registry->registerModule(QStringLiteral("trajectoryOptimization"), controller,
                [controller](const RobotQtViewerEvent&) { controller->clear(); },
                {RobotQtViewerEventKind::ProjectOpened});
            return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(id,
                [scroll]() { return scroll; }, std::make_unique<OptimizationLifecycle>(*controller),
                RobotQtViewerWorkbenchLifecyclePolicy{}, std::move(language), false,
                RobotQtViewerWorkbenchRuntimeContribution::PanelTitleResolver{},
                [registry, controller, scroll, left=panel->inputs]() {
                    if(registry) registry->unregisterModule(controller);
                    delete scroll; delete left;
                });
        }};
    }
}
