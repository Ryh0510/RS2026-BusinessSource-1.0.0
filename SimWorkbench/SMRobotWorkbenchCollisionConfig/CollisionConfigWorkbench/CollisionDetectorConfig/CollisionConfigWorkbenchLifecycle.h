#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"
#include "CollisionConfigWorkbenchShellPort.h"

#include <functional>

class QObject;
class QWidget;

namespace robot_qt_viewer
{
    class CollisionWorkbenchModuleController;
    class CollisionWorkbenchServices;
    class RobotQtViewerDocumentContext;
    class RobotQtViewerDocumentViewRegistry;

    struct CollisionConfigWorkbenchComposition
    {
        RobotQtViewerDocumentContext* documentContext = nullptr;
        RobotQtViewerDocumentViewRegistry* documentViewRegistry = nullptr;
        CollisionWorkbenchServices* appServices = nullptr;
        std::function<void(const QString&, int)> showStatus;
        std::function<void()> reloadViewport;
        std::function<void()> saveProjectAs;
        std::function<void(const QString&)> setTaskPanelTitle;
        std::function<void(CollisionConfigWorkbenchShellPort*)> bindShellPort;
    };

    class CollisionConfigWorkbenchLifecycle final : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        explicit CollisionConfigWorkbenchLifecycle(
            CollisionWorkbenchModuleController& controller);

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
        void clearConfigurationPreview(const QString& sourceId) noexcept;

        CollisionWorkbenchModuleController& m_controller;
    };

    bool registerCollisionConfigWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeCollisionConfigWorkbenchRuntimeContributionFactory(
            CollisionWorkbenchModuleController& controller,
            QWidget& taskPanel,
            QObject& languageRoot);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeOwnedCollisionConfigWorkbenchRuntimeContributionFactory(
            CollisionConfigWorkbenchComposition composition);
}
