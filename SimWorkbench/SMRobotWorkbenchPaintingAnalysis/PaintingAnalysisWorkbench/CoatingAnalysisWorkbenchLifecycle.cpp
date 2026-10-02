#include "CoatingAnalysisWorkbenchLifecycle.h"

#include "CoatingAnalysisModuleController.h"
#include "CoatingAnalysisPanel.h"
#include "ThicknessLegendWidget.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentViewRegistry.h"

#include <QFrame>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolTip>

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeOwnedPaintingAnalysisWorkbenchRuntimeContributionFactory(
        PaintingAnalysisWorkbenchComposition composition)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::CoatingAnalysis);
        return {
            workbenchId,
            [workbenchId, composition = std::move(composition)](QWidget* panelParent) {
                if(panelParent == nullptr || composition.documentContext == nullptr ||
                    composition.documentViewRegistry == nullptr ||
                    composition.overlayParent == nullptr) {
                    return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>();
                }
                auto* panel = new QScrollArea(panelParent);
                panel->setMinimumWidth(0);
                panel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                panel->setWidgetResizable(true);
                panel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                panel->setFrameShape(QFrame::NoFrame);
                auto* analysisPanel = new CoatingAnalysisPanel(panel);
                auto* controller = new CoatingAnalysisModuleController(
                    *analysisPanel,
                    *composition.documentContext,
                    panel);
                auto* legend = new ThicknessLegendWidget(composition.overlayParent);
                legend->hide();
                panel->setWidget(analysisPanel);
                if(composition.bindLegendOverlay) {
                    composition.bindLegendOverlay(legend);
                }
                QObject::connect(
                    controller,
                    &CoatingAnalysisModuleController::statusMessageRequested,
                    panel,
                    [showStatus = composition.showStatus](const QString& message, int timeoutMs) {
                        if(showStatus) {
                            showStatus(message, timeoutMs);
                        }
                    });
                QObject::connect(
                    controller,
                    &CoatingAnalysisModuleController::thicknessLegendChanged,
                    panel,
                    [legend, bindLegend = composition.bindLegendOverlay](
                        bool visible,
                        double minimumMicrometers,
                        double maximumMicrometers) {
                        legend->setRange(minimumMicrometers, maximumMicrometers);
                        legend->setVisible(visible);
                        if(visible && bindLegend) {
                            bindLegend(legend);
                        }
                    });
                QWidget* const tooltipViewport = composition.tooltipViewport;
                QObject::connect(
                    controller,
                    &CoatingAnalysisModuleController::thicknessToolTipRequested,
                    panel,
                    [tooltipViewport](const QString& text, const QPoint& position, bool visible) {
                        if(!visible || text.isEmpty() || tooltipViewport == nullptr) {
                            QToolTip::hideText();
                            return;
                        }
                        QToolTip::showText(
                            tooltipViewport->mapToGlobal(position),
                            text,
                            tooltipViewport);
                    });
                if(composition.connectSurfaceHover) {
                    composition.connectSurfaceHover(
                        *panel,
                        [controller](
                            const QString& objectId,
                            double valueMeters,
                            const QPoint& position,
                            bool hit) {
                            controller->handleSurfaceScalarHover(
                                objectId,
                                valueMeters,
                                position,
                                hit);
                        });
                }
                RobotQtViewerDocumentViewRegistry* const registry =
                    composition.documentViewRegistry;
                registry->registerModule(
                    QStringLiteral("coatingAnalysis"),
                    controller,
                    [controller](const RobotQtViewerEvent& event) {
                        controller->handleEvent(event);
                    });
                auto language = std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(*analysisPanel);
                language->addRoot(*legend);
                return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>(
                    std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                        workbenchId,
                        [panel]() { return panel; },
                        std::make_unique<CoatingAnalysisWorkbenchLifecycle>(*controller),
                        RobotQtViewerWorkbenchLifecyclePolicy{
                            RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                            RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                        std::move(language),
                        false,
                        RobotQtViewerWorkbenchRuntimeContribution::PanelTitleResolver{},
                        [registry,
                         controller,
                         panel,
                         legend,
                         bindLegend = composition.bindLegendOverlay]() {
                            registry->unregisterModule(controller);
                            if(bindLegend) {
                                bindLegend(nullptr);
                            }
                            delete legend;
                            delete panel;
                        }));
            }
        };
    }

    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makePaintingAnalysisWorkbenchRuntimeContributionFactory(
        CoatingAnalysisModuleController& controller,
        QWidget& taskPanel,
        QObject& panelRoot,
        QObject& overlayRoot)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::CoatingAnalysis);
        return {
            workbenchId,
            [workbenchId, &controller, &taskPanel, &panelRoot, &overlayRoot](QWidget*) {
                auto language =
                    std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(panelRoot);
                language->addRoot(overlayRoot);
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&taskPanel]() { return &taskPanel; },
                    std::make_unique<CoatingAnalysisWorkbenchLifecycle>(controller),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                        RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                    std::move(language));
            }
        };
    }

    bool registerPaintingAnalysisWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.painting-analysis");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Painting Analysis"))) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::CoatingAnalysis,
               QStringLiteral("coatingAnalysisWorkbench"),
               70,
               { QStringLiteral("smrobot.feature.coating-analysis") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.coating-analysis"),
            QStringLiteral("Coating Analysis"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::CoatingAnalysis) }));
    }

    CoatingAnalysisWorkbenchLifecycle::CoatingAnalysisWorkbenchLifecycle(
        CoatingAnalysisModuleController& controller)
        : m_controller(controller)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    CoatingAnalysisWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult CoatingAnalysisWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        m_controller.deactivate();
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult CoatingAnalysisWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.activate();
        return workbenchTransitionSucceeded();
    }

    void CoatingAnalysisWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept
    {
        m_controller.releaseProjectSession();
    }

    void CoatingAnalysisWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext&) noexcept
    {
        m_controller.deactivate();
    }
}
