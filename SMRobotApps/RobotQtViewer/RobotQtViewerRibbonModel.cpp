#include "RobotQtViewerRibbonModel.h"

namespace robot_qt_viewer
{
    RobotQtViewerRibbonModel makeDefaultRobotQtViewerRibbonModel(
        const RobotQtViewerRibbonTexts& texts,
        const QStringList& workbenchActionOrder)
    {
        RobotQtViewerRibbonModel model;
        model.toolbarTitle = texts.toolbarTitle;

        RobotQtViewerRibbonPageSpec homePage;
        homePage.pageId = QStringLiteral("home");
        homePage.title = texts.homePage;

        RobotQtViewerRibbonGroupSpec projectGroup;
        projectGroup.groupId = QStringLiteral("project");
        projectGroup.title = texts.projectGroup;
        projectGroup.actions = {
            { QStringLiteral("newProject"), QStringLiteral("document-new"), QStyle::SP_FileIcon },
            { QStringLiteral("openProject"), QStringLiteral("document-open"), QStyle::SP_DialogOpenButton },
            { QStringLiteral("saveProject"), QStringLiteral("document-save"), QStyle::SP_DialogSaveButton },
            { QStringLiteral("saveProjectAs"), QStringLiteral("document-save-as"), QStyle::SP_DialogSaveButton }
        };

        RobotQtViewerRibbonGroupSpec sceneEditGroup;
        sceneEditGroup.groupId = QStringLiteral("sceneEdit");
        sceneEditGroup.title = texts.sceneGroup;
        sceneEditGroup.actions = {
            { QStringLiteral("importRobot"), QStringLiteral("list-add"), QStyle::SP_ComputerIcon },
            { QStringLiteral("importObject"), QStringLiteral("insert-object"), QStyle::SP_DirIcon },
            { QStringLiteral("importPointCloud"), QStringLiteral("document-import"), QStyle::SP_FileIcon },
            { QStringLiteral("deleteSelectedItem"), QStringLiteral("edit-delete"), QStyle::SP_TrashIcon }
        };

        RobotQtViewerRibbonGroupSpec robotEditGroup;
        robotEditGroup.groupId = QStringLiteral("robotEdit");
        robotEditGroup.title = texts.robotEditGroup;
        robotEditGroup.actions = {
            { QStringLiteral("toolSetupWorkbench"), QStringLiteral("preferences-system"), QStyle::SP_DialogApplyButton }
        };

        RobotQtViewerRibbonGroupSpec viewGroup;
        viewGroup.groupId = QStringLiteral("view");
        viewGroup.title = texts.viewGroup;
        viewGroup.actions = {
            { QStringLiteral("saveImage"), QStringLiteral("image-x-generic"), QStyle::SP_FileIcon },
            { QStringLiteral("resetCamera"), QStringLiteral("view-refresh"), QStyle::SP_BrowserReload }
        };

        RobotQtViewerRibbonGroupSpec modeGroup;
        modeGroup.groupId = QStringLiteral("modes");
        modeGroup.title = texts.workbenchGroup;
        const QStringList defaultWorkbenchOrder = {
            QStringLiteral("projectAssemblyWorkbench"),
            QStringLiteral("collisionConfigWorkbench"),
            QStringLiteral("robotRunWorkbench"),
            QStringLiteral("motionPlanningWorkbench"),
            QStringLiteral("sprayProcessWorkbench"),
            QStringLiteral("coatingAnalysisWorkbench"),
            QStringLiteral("digitalTwinWorkbench")
        };
        const auto workbenchAction = [](const QString& actionId) {
            if(actionId == QStringLiteral("projectAssemblyWorkbench")) {
                return RobotQtViewerRibbonActionSpec{
                    actionId, QStringLiteral("view-list-details"), QStyle::SP_FileDialogDetailedView };
            }
            if(actionId == QStringLiteral("collisionConfigWorkbench")) {
                return RobotQtViewerRibbonActionSpec{
                    actionId, QStringLiteral("dialog-warning"), QStyle::SP_MessageBoxWarning };
            }
            if(actionId == QStringLiteral("robotRunWorkbench")) {
                return RobotQtViewerRibbonActionSpec{
                    actionId, QStringLiteral("media-playback-start"), QStyle::SP_MediaPlay };
            }
            if(actionId == QStringLiteral("motionPlanningWorkbench")) {
                return RobotQtViewerRibbonActionSpec{
                    actionId, QStringLiteral("go-next"), QStyle::SP_ArrowRight };
            }
            if(actionId == QStringLiteral("sprayProcessWorkbench")) {
                return RobotQtViewerRibbonActionSpec{
                    actionId, QStringLiteral("format-fill-color"), QStyle::SP_DialogApplyButton };
            }
            if(actionId == QStringLiteral("coatingAnalysisWorkbench")) {
                return RobotQtViewerRibbonActionSpec{
                    actionId, QStringLiteral("view-statistics"), QStyle::SP_FileDialogInfoView };
            }
            return RobotQtViewerRibbonActionSpec{
                actionId, QStringLiteral("network-connect"), QStyle::SP_ComputerIcon };
        };
        const QStringList& requestedOrder = workbenchActionOrder.isEmpty()
            ? defaultWorkbenchOrder
            : workbenchActionOrder;
        for(const QString& actionId : requestedOrder) {
            if(defaultWorkbenchOrder.contains(actionId)) {
                modeGroup.actions.push_back(workbenchAction(actionId));
            }
        }

        homePage.groups = { projectGroup, modeGroup, sceneEditGroup, robotEditGroup, viewGroup };
        model.pages = { homePage };
        return model;
    }
}
