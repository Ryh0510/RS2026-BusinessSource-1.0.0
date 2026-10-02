#pragma once

#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerWorkbenchContribution.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QPoint>

#include <functional>

class QObject;
class QWidget;

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentContext;
    class RobotQtViewerDocumentViewRegistry;
    class CoatingAnalysisModuleController;

    struct PaintingAnalysisWorkbenchComposition
    {
        using SurfaceHoverHandler =
            std::function<void(const QString&, double, const QPoint&, bool)>;

        RobotQtViewerDocumentContext* documentContext = nullptr;
        RobotQtViewerDocumentViewRegistry* documentViewRegistry = nullptr;
        QWidget* overlayParent = nullptr;
        QWidget* tooltipViewport = nullptr;
        std::function<void(const QString&, int)> showStatus;
        std::function<void(QObject&, SurfaceHoverHandler)> connectSurfaceHover;
        std::function<void(QWidget*)> bindLegendOverlay;
    };

    class CoatingAnalysisWorkbenchLifecycle final : public IRobotQtViewerWorkbenchLifecycle
    {
    public:
        explicit CoatingAnalysisWorkbenchLifecycle(
            CoatingAnalysisModuleController& controller);

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
        CoatingAnalysisModuleController& m_controller;
    };

    bool registerPaintingAnalysisWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makePaintingAnalysisWorkbenchRuntimeContributionFactory(
            CoatingAnalysisModuleController& controller,
            QWidget& taskPanel,
            QObject& panelRoot,
            QObject& overlayRoot);
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
        makeOwnedPaintingAnalysisWorkbenchRuntimeContributionFactory(
            PaintingAnalysisWorkbenchComposition composition);
}
