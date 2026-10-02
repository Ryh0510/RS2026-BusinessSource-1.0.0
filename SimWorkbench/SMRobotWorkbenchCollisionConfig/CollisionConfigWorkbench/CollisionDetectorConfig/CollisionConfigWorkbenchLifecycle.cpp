#include "CollisionConfigWorkbenchLifecycle.h"

#include "CollisionWorkbenchModuleController.h"
#include "CollisionWorkbenchPanel.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentViewRegistry.h"

#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

namespace robot_qt_viewer
{
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeOwnedCollisionConfigWorkbenchRuntimeContributionFactory(
        CollisionConfigWorkbenchComposition composition)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::Collision);
        return {
            workbenchId,
            [workbenchId, composition = std::move(composition)](QWidget* panelParent) {
                if(panelParent == nullptr || composition.documentContext == nullptr ||
                    composition.documentViewRegistry == nullptr ||
                    composition.appServices == nullptr) {
                    return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>();
                }

                auto* scrollArea = new QScrollArea(panelParent);
                scrollArea->setMinimumWidth(0);
                scrollArea->setWidgetResizable(true);
                scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                scrollArea->setFrameShape(QFrame::NoFrame);
                auto* content = new QWidget(scrollArea);
                auto* layout = new QVBoxLayout(content);
                layout->setContentsMargins(10, 10, 10, 10);
                layout->setSpacing(8);
                auto* panel = new CollisionWorkbenchPanel(content);
                layout->addWidget(panel, 1);
                content->setLayout(layout);
                scrollArea->setWidget(content);

                auto* controller = new CollisionWorkbenchModuleController(
                    *panel,
                    *composition.documentContext,
                    *composition.appServices,
                    scrollArea);
                QObject::connect(controller, &CollisionWorkbenchModuleController::statusMessageRequested,
                    scrollArea,
                    [callback = composition.showStatus](const QString& message, int timeoutMs) {
                        if(callback) {
                            callback(message, timeoutMs);
                        }
                    });
                QObject::connect(controller, &CollisionWorkbenchModuleController::refreshInspectorRequested,
                    scrollArea,
                    [controller]() {
                        controller->refreshInspector(QString());
                    });
                QObject::connect(
                    controller,
                    &CollisionWorkbenchModuleController::refreshSelectionDependentViewsRequested,
                    scrollArea,
                    [controller, scrollArea]() {
                        QTimer::singleShot(0, scrollArea, [controller]() {
                            controller->refreshCollisionElementList(QString());
                            controller->refreshCollisionDetectorList();
                        });
                    });
                QObject::connect(
                    controller,
                    &CollisionWorkbenchModuleController::saveCollisionOverridesSidecarRequested,
                    scrollArea,
                    [controller, scrollArea]() {
                        controller->requestSaveOverridesAsSidecar(scrollArea);
                    });
                QObject::connect(
                    controller,
                    &CollisionWorkbenchModuleController::exportCollisionUrdfRequested,
                    scrollArea,
                    [controller, scrollArea]() {
                        controller->requestExportRobotUrdfWithCollision(scrollArea);
                    });
                QObject::connect(controller, &CollisionWorkbenchModuleController::viewportReloadRequested,
                    scrollArea,
                    [callback = composition.reloadViewport]() {
                        if(callback) {
                            callback();
                        }
                    });
                QObject::connect(controller, &CollisionWorkbenchModuleController::projectSaveAsRequested,
                    scrollArea,
                    [callback = composition.saveProjectAs]() {
                        if(callback) {
                            callback();
                        }
                    });
                QObject::connect(panel, &CollisionWorkbenchPanel::rightPanelTitleChanged,
                    scrollArea,
                    [callback = composition.setTaskPanelTitle](const QString& title) {
                        if(callback) {
                            callback(title);
                        }
                    });

                RobotQtViewerDocumentViewRegistry* const registry =
                    composition.documentViewRegistry;
                registry->registerModule(QStringLiteral("collisionWorkbench"), controller,
                    [controller](const RobotQtViewerEvent& event) {
                        controller->handleEvent(event);
                    });
                if(composition.bindShellPort) {
                    composition.bindShellPort(controller);
                }

                auto language = std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(*panel);
                return std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>(
                    std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                        workbenchId,
                        [scrollArea]() { return scrollArea; },
                        std::make_unique<CollisionConfigWorkbenchLifecycle>(*controller),
                        RobotQtViewerWorkbenchLifecyclePolicy{
                            RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                            RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                        std::move(language),
                        false,
                        RobotQtViewerWorkbenchRuntimeContribution::PanelTitleResolver{},
                        [registry,
                         controller,
                         scrollArea,
                         bindPort = composition.bindShellPort]() {
                            registry->unregisterModule(controller);
                            if(bindPort) {
                                bindPort(nullptr);
                            }
                            delete scrollArea;
                        }));
            }
        };
    }

    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    makeCollisionConfigWorkbenchRuntimeContributionFactory(
        CollisionWorkbenchModuleController& controller,
        QWidget& taskPanel,
        QObject& languageRoot)
    {
        const QString workbenchId = robotQtViewerWorkbenchId(
            RobotQtViewerWorkbenchKind::Collision);
        return {
            workbenchId,
            [workbenchId, &controller, &taskPanel, &languageRoot](QWidget*) {
                auto language =
                    std::make_unique<RobotQtViewerWidgetLanguageParticipant>();
                language->addRoot(languageRoot);
                return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(
                    workbenchId,
                    [&taskPanel]() { return &taskPanel; },
                    std::make_unique<CollisionConfigWorkbenchLifecycle>(controller),
                    RobotQtViewerWorkbenchLifecyclePolicy{
                        RobotQtViewerWorkbenchExecutionPolicy::NoOwnedExecution,
                        RobotQtViewerWorkbenchReactivationPolicy::RestoreUiOnly },
                    std::move(language));
            }
        };
    }

    bool registerCollisionConfigWorkbenchContribution(
        RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.collision-config");
        if(!catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
               packageId, QStringLiteral("Collision Config"))) ||
            !catalog.registerWorkbench(makeRobotQtViewerWorkbench(
               packageId,
               RobotQtViewerWorkbenchKind::Collision,
               QStringLiteral("collisionConfigWorkbench"),
               40,
               { QStringLiteral("smrobot.feature.collision-config") },
               { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }))) {
            return false;
        }
        return catalog.registerFeature(makeRobotQtViewerWorkbenchFeature(
            QStringLiteral("smrobot.feature.collision-config"),
            QStringLiteral("Collision Configuration"),
            packageId,
            { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision) }));
    }

    CollisionConfigWorkbenchLifecycle::CollisionConfigWorkbenchLifecycle(
        CollisionWorkbenchModuleController& controller)
        : m_controller(controller)
    {
    }

    RobotQtViewerWorkbenchTransitionResult
    CollisionConfigWorkbenchLifecycle::prepareDeactivate(
        const RobotQtViewerWorkbenchTransitionContext&)
    {
        if(m_controller.isCollisionModelConfigurationActive() &&
            !m_controller.canHandoffCollisionModelConfiguration()) {
            return workbenchTransitionRejected(
                QStringLiteral("Collision model configuration has pending changes."),
                QStringLiteral("collision_config.pending_changes"));
        }
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult CollisionConfigWorkbenchLifecycle::deactivate(
        const RobotQtViewerWorkbenchTransitionContext& context)
    {
        clearConfigurationPreview(context.sourceId);
        m_controller.setWorkbenchActive(false);
        return workbenchTransitionSucceeded();
    }

    RobotQtViewerWorkbenchTransitionResult CollisionConfigWorkbenchLifecycle::activate(
        const RobotQtViewerWorkbenchActivationContext&)
    {
        m_controller.setWorkbenchActive(true);
        m_controller.showDetectorConfiguration();
        return workbenchTransitionSucceeded();
    }

    void CollisionConfigWorkbenchLifecycle::releaseProject(
        const RobotQtViewerWorkbenchProjectReleaseContext& context) noexcept
    {
        m_controller.setWorkbenchActive(false);
        clearConfigurationPreview(context.sourceId);
    }

    void CollisionConfigWorkbenchLifecycle::shutdown(
        const RobotQtViewerWorkbenchShutdownContext& context) noexcept
    {
        m_controller.setWorkbenchActive(false);
        clearConfigurationPreview(context.sourceId);
    }

    void CollisionConfigWorkbenchLifecycle::clearConfigurationPreview(
        const QString& sourceId) noexcept
    {
        if(m_controller.isCollisionModelConfigurationActive()) {
            m_controller.cancelCollisionModelConfigurationForHandoff(
                sourceId.isEmpty()
                    ? QStringLiteral("collisionWorkbenchLifecycle")
                    : sourceId);
        }
    }
}
