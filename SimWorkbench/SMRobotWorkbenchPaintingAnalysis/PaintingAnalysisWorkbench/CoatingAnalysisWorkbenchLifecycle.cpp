#include "CoatingAnalysisWorkbenchLifecycle.h"

#include "CoatingAnalysisModuleController.h"
#include "CoatingAnalysisPanel.h"
#include "CoatingAnalysisTreePanel.h"
#include "CoatingAnalysisInfoPanel.h"
#include "CoatingAnalysisVisibilityBar.h"
#include "PlanningDataSource.h"
#include <QVBoxLayout>
#include "ThicknessLegendWidget.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentViewRegistry.h"

#include <QFrame>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolTip>

namespace robot_qt_viewer
{
    class CoatingLanguage final : public IRobotQtViewerLanguageParticipant {
    public:
        explicit CoatingLanguage(CoatingAnalysisModuleController& controller, ThicknessLegendWidget& legend):m_controller(controller),m_legend(legend){}
        void retranslateUi(const RobotQtViewerLocalizationService& localization) noexcept override {
            m_controller.setLanguageCode(localization.currentLanguageId()); m_legend.setLanguageCode(localization.currentLanguageId());
        }
    private: CoatingAnalysisModuleController& m_controller; ThicknessLegendWidget& m_legend;
    };

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
                auto* container = new QWidget(panel);
                auto* layout = new QVBoxLayout(container);
                auto* analysisPanel = new CoatingAnalysisPanel(container);
                auto* visibilityBar = new CoatingAnalysisVisibilityBar(container);
                auto* left = new QWidget(panelParent);
                auto* leftLayout = new QVBoxLayout(left);
                auto* treePanel = new CoatingAnalysisTreePanel(left);
                auto* infoPanel = new CoatingAnalysisInfoPanel(left);
                leftLayout->addWidget(treePanel, 1);
                leftLayout->addWidget(infoPanel, 2);
                layout->addWidget(analysisPanel, 1);
                layout->addWidget(visibilityBar);
                if(composition.installLeftPanel) composition.installLeftPanel(left);

                auto* controller = new CoatingAnalysisModuleController(
                    *analysisPanel,
                    *treePanel,
                    *infoPanel,
                    *visibilityBar,
                    *composition.documentContext,
                    panel);
                if(composition.bindViewportInteractions) composition.bindViewportInteractions(*controller);
                auto* legend = new ThicknessLegendWidget(composition.overlayParent);
                legend->hide();
                panel->setWidget(container);
                auto* source = qobject_cast<robot_qt_viewer::PlanningDataSource*>(composition.dataSource);
                if(source) {
                    auto synchronize = [controller](const robot_qt_viewer::PlanningDataSnapshot& snapshot) {
                        controller->synchronizePlanningData(snapshot);
                    };
                    QObject::connect(source, &robot_qt_viewer::PlanningDataSource::analysisSnapshotChanged, controller, synchronize);
                    synchronize(source->analysisSnapshot());
                }
                QObject::connect(legend, &ThicknessLegendWidget::rangeChangeRequested,
                    controller, &CoatingAnalysisModuleController::setThicknessDisplayRange);
                QObject::connect(legend, &ThicknessLegendWidget::automaticRangeRequested,
                    controller, &CoatingAnalysisModuleController::resetThicknessDisplayRange);
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
                        double maximumMicrometers, bool relativeError, bool editingEnabled) {
                        legend->setRange(minimumMicrometers, maximumMicrometers);
                        legend->setRelativeErrorMode(relativeError);
                        legend->setEditingEnabled(editingEnabled);
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
                            double x, double y, double z,
                            const QPoint& position,
                            bool hit) {
                            controller->handleSurfaceScalarHover(
                                objectId,
                                valueMeters,
                                x, y, z,
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
                auto language = std::make_unique<CoatingLanguage>(*controller,*legend);
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
                         left,
                         legend,
                         bindLegend = composition.bindLegendOverlay]() {
                            registry->unregisterModule(controller);
                            if(bindLegend) {
                                bindLegend(nullptr);
                            }
                            delete left;
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
