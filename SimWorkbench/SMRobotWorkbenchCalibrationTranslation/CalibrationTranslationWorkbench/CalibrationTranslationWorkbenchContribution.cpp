#include "CalibrationTranslationWorkbenchContribution.h"
#include "CalibrationTranslationModuleController.h"
#include "Widgets/ABBTranslationPanel.h"
#include "Widgets/WorkpieceCalibrationPanel.h"
#include "Widgets/PostProcessingPanel.h"
#include "RobotQtViewerDocumentViewRegistry.h"
#include "PlanningDataSource.h"
#include <QTabWidget>
#include <QScrollArea>
namespace robot_qt_viewer {
    bool registerCalibrationTranslationWorkbenchContribution(RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        const QString packageId = QStringLiteral("smrobot.workbench.calibration-translation");
        RobotQtViewerWorkbenchDescriptor descriptor;
        descriptor.kind = RobotQtViewerWorkbenchKind::Browse;
        descriptor.domain = RobotQtViewerWorkbenchDomain::SprayProcess;
        descriptor.rightPanel = RobotQtViewerRightPanelKind::Status;
        descriptor.defaultViewportMode = RobotQtViewerViewportInteractionMode::Browse;
        descriptor.id = QStringLiteral("smrobot.mode.calibration-translation");
        descriptor.displayName = QStringLiteral("Calibration & ABB Translation");
        descriptor.rightPanelTitle = descriptor.displayName;
        return catalog.registerPackage(makeRobotQtViewerWorkbenchPackage(
                   packageId, descriptor.displayName)) &&
            catalog.registerWorkbench(makeRobotQtViewerWorkbench(
                packageId,
                descriptor,
                QStringLiteral("calibrationTranslationWorkbench"),
                66,
                { QStringLiteral("smrobot.feature.calibration-translation") },
                { robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse) }));
    }
    class TranslationLifecycle final : public IRobotQtViewerWorkbenchLifecycle {
    public:
        explicit TranslationLifecycle(smrobot::workbench::spray::translation::CalibrationTranslationModuleController& controller):m_controller(controller){}
        RobotQtViewerWorkbenchTransitionResult prepareDeactivate(const RobotQtViewerWorkbenchTransitionContext&) override { return workbenchTransitionSucceeded(); }
        RobotQtViewerWorkbenchTransitionResult deactivate(const RobotQtViewerWorkbenchTransitionContext&) override { return workbenchTransitionSucceeded(); }
        RobotQtViewerWorkbenchTransitionResult activate(const RobotQtViewerWorkbenchActivationContext&) override { m_controller.refresh(); return workbenchTransitionSucceeded(); }
        void releaseProject(const RobotQtViewerWorkbenchProjectReleaseContext&) noexcept override { m_controller.clear(); }
        void shutdown(const RobotQtViewerWorkbenchShutdownContext&) noexcept override { m_controller.clear(); }
    private: smrobot::workbench::spray::translation::CalibrationTranslationModuleController& m_controller;
    };
    class TranslationLanguage final : public IRobotQtViewerLanguageParticipant {
    public:
        explicit TranslationLanguage(smrobot::workbench::spray::translation::CalibrationTranslationModuleController& c):m_controller(c){}
        void retranslateUi(const RobotQtViewerLocalizationService& localization) noexcept override { m_controller.setLanguageCode(localization.currentLanguageId()); }
    private: smrobot::workbench::spray::translation::CalibrationTranslationModuleController& m_controller;
    };
    RobotQtViewerWorkbenchRuntimeContributionFactoryDesc makeOwnedCalibrationTranslationWorkbenchRuntimeContributionFactory(CalibrationTranslationWorkbenchComposition composition)
    {
        const QString id=QStringLiteral("smrobot.mode.calibration-translation");
        return {id,[id,composition](QWidget* parent)->std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution> {
            if(!composition.documentContext || !composition.documentViewRegistry) return {};
            using namespace smrobot::workbench::spray::translation;
            auto* tabs=new QTabWidget(parent);
            auto* calibration=new WorkpieceCalibrationPanel(tabs);
            auto* abb=new ABBTranslationPanel(tabs);
            auto* post=new PostProcessingPanel(tabs);
            auto addTab=[tabs](QWidget* widget,const QString& title) {
                auto* scroll=new QScrollArea(tabs); scroll->setWidgetResizable(true); scroll->setWidget(widget);
                scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); tabs->addTab(scroll,title);
            };
            addTab(calibration,QStringLiteral("Calibration")); addTab(abb,QStringLiteral("ABB Translation")); addTab(post,QStringLiteral("Post Processing"));
            auto* controller=new CalibrationTranslationModuleController(*composition.documentContext,*calibration,*abb,*post,tabs);
            QObject::connect(controller,&CalibrationTranslationModuleController::statusMessageRequested,tabs,[composition](const QString& text,int ms){ if(composition.showStatus) composition.showStatus(text,ms); });
            auto* source=qobject_cast<robot_qt_viewer::PlanningDataSource*>(composition.freeformDataSource);
            if(source) {
                auto sync=[controller](const robot_qt_viewer::PlanningDataSnapshot& snapshot){controller->synchronize(snapshot.modelObjectId,snapshot.trajectory);};
                QObject::connect(source,&robot_qt_viewer::PlanningDataSource::snapshotChanged,controller,sync); sync(source->snapshot());
            }
            auto* registry=composition.documentViewRegistry;
            registry->registerModule(QStringLiteral("calibrationTranslation"),controller,[controller](const RobotQtViewerEvent& event){controller->handleEvent(event);},{
                RobotQtViewerEventKind::ProjectOpened,
                RobotQtViewerEventKind::ProjectDocumentChanged,
                RobotQtViewerEventKind::ViewportReloaded
            });
            return std::make_unique<RobotQtViewerBasicWorkbenchRuntimeContribution>(id,[tabs](){return tabs;},std::make_unique<TranslationLifecycle>(*controller),RobotQtViewerWorkbenchLifecyclePolicy{},std::make_unique<TranslationLanguage>(*controller),false,RobotQtViewerWorkbenchRuntimeContribution::PanelTitleResolver{},[registry,controller,tabs](){registry->unregisterModule(controller); delete tabs;});
        }};
    }
}
