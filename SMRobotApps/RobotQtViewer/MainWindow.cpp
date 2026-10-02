#include "MainWindow.h"

#include "CollisionConfigWorkbenchShellPort.h"
#include "CollisionConfigWorkbenchLifecycle.h"
#include "CoatingAnalysisWorkbenchLifecycle.h"
#include "DigitalTwinWorkbenchContribution.h"
#include "MotionPlanningWorkbenchLifecycle.h"
#include "RobotRunWorkbenchLifecycle.h"
#include "RobotRunWorkbenchShellPort.h"
#include "RobotQtViewerPlatformConfigurationDialog.h"
#include "RobotQtViewerWorkbenchPlugin.h"
#include "RobotQtViewerSceneExplorerActionRouter.h"
#include "RobotQtViewerTheme.h"
#include "RobotQtViewerToolbarController.h"
#include "RobotQtViewerCollisionWorkbenchServicesAdapter.h"
#include "RobotQtViewerToolSetupAppServicesAdapter.h"
#include "RobotQtViewerViewportEventController.h"
#include "RobotQtViewerViewportPresentationController.h"
#include "RobotQtViewerViewportServicesAdapter.h"
#include <RobotQtViewerFileDialog.h>
#include "ProjectAssemblyDialogService.h"
#include "RobotQtWidgetUtils.h"
#include "RobotViewport.h"
#include "SceneExplorerWorkbenchShellPort.h"
#include "SceneExplorerWorkbenchLifecycle.h"
#include "SceneCollisionTargetResolver.h"
#include "SceneSelectionController.h"
#include "SceneTreeIntentController.h"
#include "SprayProcessWorkbenchContribution.h"
#include "StatusPanelWidget.h"
#include "ToolSetupWorkbenchShellPort.h"
#include "ToolSetupWorkbenchLifecycle.h"

#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectIo.h>
#include <SimulationProject/RuntimePaths.h>

#include <RobotSDK/IRobotLoader.h>
#include <RobotSDK/IRobotModel.h>
#include <RobotSDK/IRobotSdk.h>
#include <RobotSDK/RobotSdkApi.h>

#include <CustomLog/CustomLog.h>

#include <QAction>
#include <QAbstractSpinBox>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDockWidget>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QImage>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPoint>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QProgressBar>
#include <QProcess>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QWidget>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <data_path.h>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#ifndef ROBOT_QT_VIEWER_DEFAULT_PROJECT_PATH
#define ROBOT_QT_VIEWER_DEFAULT_PROJECT_PATH "projects/420.v3.scene.json"
#endif

namespace
{
    using robot_qt_viewer::configureInspectorButton;
    using robot_qt_viewer::makeHorizontallyCompressible;

    double elapsedMilliseconds(const std::chrono::steady_clock::time_point& start)
    {
        const auto elapsed = std::chrono::steady_clock::now() - start;
        return std::chrono::duration<double, std::milli>(elapsed).count();
    }

    simulation_project::ColorDesc viewportBackgroundForTheme(
        robot_qt_viewer::ThemeKind theme)
    {
        switch(theme) {
        case robot_qt_viewer::ThemeKind::Light:
            return simulation_project::ColorDesc{ 0.91, 0.93, 0.95, 1.0 };
        case robot_qt_viewer::ThemeKind::Dark:
            return simulation_project::ColorDesc{ 0.043, 0.059, 0.078, 1.0 };
        case robot_qt_viewer::ThemeKind::Modern:
        default:
            return simulation_project::ColorDesc{ 0.067, 0.082, 0.102, 1.0 };
        }
    }

    std::string sourceTypeFromPath(const QString& path)
    {
        return path.endsWith(".xml", Qt::CaseInsensitive) ? "simscape" : "urdf";
    }

    std::filesystem::path weakCanonicalPath(const std::filesystem::path& path)
    {
        std::error_code error;
        std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
        return error ? path.lexically_normal() : canonical;
    }

    std::string lowercaseAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::string pathToUtf8(const std::filesystem::path& path)
    {
        return path.generic_u8string();
    }

    std::filesystem::path pathFromUtf8(const std::string& path)
    {
        return std::filesystem::u8path(path);
    }

    ProjectSceneInteractionMode toProjectSceneInteractionMode(
        robot_qt_viewer::RobotQtViewerViewportInteractionMode mode)
    {
        switch(mode) {
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::Browse:
            return ProjectSceneInteractionMode::Browse;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::SelectRobot:
            return ProjectSceneInteractionMode::SelectRobot;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::SelectLink:
            return ProjectSceneInteractionMode::SelectLink;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::SelectMount:
            return ProjectSceneInteractionMode::SelectMount;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::SelectAttachment:
            return ProjectSceneInteractionMode::SelectAttachment;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::EditTransformPreview:
            return ProjectSceneInteractionMode::EditTransformPreview;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::EditCollisionProxy:
            return ProjectSceneInteractionMode::EditCollisionProxy;
        case robot_qt_viewer::RobotQtViewerViewportInteractionMode::SelectCollisionTarget:
            return ProjectSceneInteractionMode::SelectCollisionTarget;
        }
        return ProjectSceneInteractionMode::Browse;
    }

    std::string normalizedCollisionRole(const std::string& role)
    {
        return role.empty() ? std::string("Exact") : role;
    }

    bool pathIsInside(const std::filesystem::path& path, const std::filesystem::path& root)
    {
        const std::string pathText = lowercaseAscii(pathToUtf8(weakCanonicalPath(path)));
        std::string rootText = lowercaseAscii(pathToUtf8(weakCanonicalPath(root)));
        if(rootText.empty()) {
            return false;
        }
        if(rootText.back() != '/') {
            rootText.push_back('/');
        }
        return pathText == rootText.substr(0, rootText.size() - 1) ||
            pathText.rfind(rootText, 0) == 0;
    }

    std::string relativePathString(
        const std::filesystem::path& path,
        const std::filesystem::path& root)
    {
        if(!pathIsInside(path, root)) {
            return {};
        }

        std::filesystem::path relative = weakCanonicalPath(path).lexically_relative(weakCanonicalPath(root));
        if(relative.empty()) {
            return {};
        }
        return pathToUtf8(relative);
    }

    std::string makePortableAssetPathForProject(
        const std::filesystem::path& assetPath,
        const std::filesystem::path& projectPath)
    {
        const std::filesystem::path canonicalAsset = weakCanonicalPath(assetPath);
        const std::filesystem::path dataRoot = weakCanonicalPath(
            simulation_project::RuntimePaths::dataRoot());

        std::string dataRelative = relativePathString(canonicalAsset, dataRoot);
        if(!dataRelative.empty()) {
            return pathToUtf8(std::filesystem::path("data") / pathFromUtf8(dataRelative));
        }

        const char* assetRootEnv = std::getenv("SMROBOT_ASSET_DIR");
        if(assetRootEnv != nullptr) {
            std::string envRelative = relativePathString(canonicalAsset, std::filesystem::path(assetRootEnv));
            if(!envRelative.empty()) {
                return envRelative;
            }
        }

        if(!projectPath.empty()) {
            const std::filesystem::path projectDir = weakCanonicalPath(projectPath.parent_path());
            std::string projectRelative = relativePathString(canonicalAsset, projectDir);
            if(!projectRelative.empty()) {
                return projectRelative;
            }
        }

        return pathToUtf8(canonicalAsset);
    }

    std::filesystem::path resolveStoredAssetPathForSave(
        const std::string& storedPath,
        const std::filesystem::path& currentProjectPath)
    {
        std::filesystem::path candidate = pathFromUtf8(storedPath);
        if(candidate.is_absolute()) {
            return candidate;
        }

        if(!currentProjectPath.empty()) {
            const std::filesystem::path fromCurrentProject = currentProjectPath.parent_path() / candidate;
            if(std::filesystem::exists(fromCurrentProject)) {
                return fromCurrentProject;
            }
        }

        const std::filesystem::path fromAppRoot =
            simulation_project::RuntimePaths::applicationRoot() / candidate;
        if(std::filesystem::exists(fromAppRoot)) {
            return fromAppRoot;
        }

        const std::filesystem::path sourceRoot = simulation_project::RuntimePaths::sourceRoot();
        const std::filesystem::path fromSourceRoot = sourceRoot / candidate;
        if(!sourceRoot.empty() && std::filesystem::exists(fromSourceRoot)) {
            return fromSourceRoot;
        }

        return candidate;
    }

    std::string makePortableStoredAssetPathForSave(
        const std::string& storedPath,
        const std::filesystem::path& currentProjectPath,
        const std::filesystem::path& targetProjectPath)
    {
        const std::filesystem::path assetPath = resolveStoredAssetPathForSave(storedPath, currentProjectPath);
        if(assetPath.is_relative() && !std::filesystem::exists(assetPath)) {
            return storedPath;
        }
        return makePortableAssetPathForProject(assetPath, targetProjectPath);
    }

    QString dialogPathFromFilesystem(const std::filesystem::path& path)
    {
        return QString::fromStdWString(path.wstring());
    }

    QString defaultProjectDialogPath(const std::filesystem::path& currentProjectPath)
    {
        if(!currentProjectPath.empty()) {
            return dialogPathFromFilesystem(currentProjectPath.parent_path());
        }
        return dialogPathFromFilesystem(
            simulation_project::RuntimePaths::configRoot() / "projects");
    }

    bool confirmProjectReplacement(
        QWidget* parent,
        const QString& title,
        const std::function<bool(bool)>& resolvePendingChanges,
        const std::function<void()>& saveProject,
        const std::function<bool()>& isProjectDirty)
    {
        if(resolvePendingChanges && !resolvePendingChanges(false)) {
            return false;
        }
        if(!isProjectDirty || !isProjectDirty()) {
            return true;
        }

        const QMessageBox::StandardButton choice = QMessageBox::warning(
            parent,
            title,
            QStringLiteral("The current project has unsaved changes. Save them before continuing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if(choice == QMessageBox::Cancel) {
            return false;
        }
        if(choice == QMessageBox::Discard) {
            return true;
        }

        if(saveProject) {
            saveProject();
        }
        return isProjectDirty ? !isProjectDirty() : true;
    }

    constexpr double kPi = 3.14159265358979323846;

    double toDegrees(double radians)
    {
        return radians * 180.0 / kPi;
    }

    simulation_project::TransformDesc attachmentAssetTcpTransform(
        const simulation_project::AttachmentAssetDesc* asset)
    {
        if(asset == nullptr) {
            return simulation_project::TransformDesc();
        }
        for(const simulation_project::AttachmentFunctionalFrameDesc& frame : asset->functionalFrames) {
            if(frame.primary || frame.frameType == "tcp") {
                return frame.assetMountToFrame;
            }
        }
        return simulation_project::TransformDesc();
    }

    void setAttachmentAssetTcpTransform(
        simulation_project::AttachmentAssetDesc& asset,
        const simulation_project::TransformDesc& transform)
    {
        for(simulation_project::AttachmentFunctionalFrameDesc& frame : asset.functionalFrames) {
            if(frame.primary || frame.frameType == "tcp") {
                frame.assetMountToFrame = transform;
                frame.primary = true;
                return;
            }
        }
        simulation_project::AttachmentFunctionalFrameDesc frame;
        frame.id = asset.id + ".tcp";
        frame.name = "TCP";
        frame.frameType = "tcp";
        frame.assetMountToFrame = transform;
        frame.primary = true;
        asset.functionalFrames.push_back(frame);
    }

    QString collisionSelectionSetMemberText(const simulation_project::CollisionSelectionSetMemberDesc& member)
    {
        const std::string attachmentId = !member.attachmentId.empty()
            ? member.attachmentId
            : std::string();
        if(!attachmentId.empty()) {
            return QString("attachment: %1").arg(QString::fromStdString(attachmentId));
        }
        if(!member.objectId.empty()) {
            return QString("object: %1").arg(QString::fromStdString(member.objectId));
        }
        if(!member.robotId.empty() && !member.linkName.empty()) {
            return QString("link: %1.%2")
                .arg(QString::fromStdString(member.robotId), QString::fromStdString(member.linkName));
        }
        if(!member.robotId.empty()) {
            return QString("robot: %1").arg(QString::fromStdString(member.robotId));
        }
        return "invalid member";
    }

    bool sameCollisionSelectionSetMember(
        const simulation_project::CollisionSelectionSetMemberDesc& a,
        const simulation_project::CollisionSelectionSetMemberDesc& b)
    {
        return a.robotId == b.robotId &&
            a.linkName == b.linkName &&
            a.objectId == b.objectId &&
            a.attachmentId == b.attachmentId;
    }

    QString cameraViewIconResourcePath(const QString& id)
    {
        return QStringLiteral(":/RobotQtViewer/icons/ribbon/camera_view_%1.png").arg(id);
    }

    QToolButton* makeCameraViewButton(QAction* action, QWidget* parent)
    {
        auto* button = new QToolButton(parent);
        button->setProperty("cameraViewControl", true);
        if(action != nullptr) {
            button->setDefaultAction(action);
        }
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(34, 34));
        button->setFixedSize(QSize(42, 42));
        return button;
    }

    QIcon viewportPresentationIcon(bool leavePresentationMode)
    {
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(48, 63, 74), 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        if(!leavePresentationMode) {
            painter.drawLine(10, 25, 10, 10);
            painter.drawLine(10, 10, 25, 10);
            painter.drawLine(39, 10, 54, 10);
            painter.drawLine(54, 10, 54, 25);
            painter.drawLine(10, 39, 10, 54);
            painter.drawLine(10, 54, 25, 54);
            painter.drawLine(39, 54, 54, 54);
            painter.drawLine(54, 39, 54, 54);
        } else {
            painter.drawLine(9, 24, 24, 24);
            painter.drawLine(24, 9, 24, 24);
            painter.drawLine(40, 9, 40, 24);
            painter.drawLine(40, 24, 55, 24);
            painter.drawLine(9, 40, 24, 40);
            painter.drawLine(24, 40, 24, 55);
            painter.drawLine(40, 40, 55, 40);
            painter.drawLine(40, 40, 40, 55);
        }

        return QIcon(pixmap);
    }

}

MainWindow::MainWindow(
    robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry workbenchCatalog,
    robot_qt_viewer::RobotQtViewerPlatformProfile platformProfile,
    robot_qt_viewer::RobotQtViewerResolvedPlatformComposition platformComposition,
    std::filesystem::path platformProfilesDirectory,
    std::filesystem::path platformSelectionPath,
    robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader* workbenchPluginLoader,
    const QString& startupLanguageId,
    QWidget* parent)
    : QMainWindow(parent)
    , m_windowConfig(robot_qt_viewer::defaultRobotQtViewerWindowConfig())
    , m_eventHub(this)
    , m_operationStatusStore(m_eventHub)
    , m_documentViewRegistry(m_eventHub, m_windowConfig)
    , m_documentController(m_projectSession, m_eventHub, this)
    , m_selectionModel(m_eventHub)
    , m_viewportPreviewState(m_eventHub)
    , m_platformProfile(std::move(platformProfile))
    , m_platformComposition(std::move(platformComposition))
    , m_platformProfilesDirectory(std::move(platformProfilesDirectory))
    , m_platformSelectionPath(std::move(platformSelectionPath))
    , m_workbenchPackageRegistry(std::move(workbenchCatalog))
    , m_workbenchPluginLoader(workbenchPluginLoader)
    , m_documentContext(
          m_projectSession,
          m_documentController,
          m_selectionModel,
          m_viewportPreviewState,
          m_eventHub,
          m_operationStatusStore)
    , m_appController(m_documentContext)
    , m_projectPath(m_projectSession.path())
{
    const robot_qt_viewer::RobotQtViewerWorkbenchDescriptor* initialDescriptor =
        m_workbenchPackageRegistry.descriptor(m_platformComposition.defaultWorkbenchId);
    if(initialDescriptor == nullptr) {
        throw std::runtime_error(
            QStringLiteral("Platform default Workbench is not supported by this Viewer: %1")
                .arg(m_platformComposition.defaultWorkbenchId)
                .toStdString());
    }
    m_workbenchManager.setInitialWorkbench(
        m_platformComposition.defaultWorkbenchId, *initialDescriptor);
    m_localization = std::make_unique<robot_qt_viewer::RobotQtViewerLocalizationService>(
        QString::fromStdWString(
            (simulation_project::RuntimePaths::configRoot() / "translations").wstring()),
        this);
    QString localizationError;
    if(!m_localization->reloadCatalogs(&localizationError)) {
        throw std::runtime_error(localizationError.toStdString());
    }
    m_localization->installOnApplication(*qApp);
    m_operationStatusPresenter =
        std::make_unique<robot_qt_viewer::RobotQtViewerOperationStatusPresenter>(
            *m_localization);
    m_languageId = startupLanguageId.trimmed().isEmpty()
        ? m_localization->savedLanguageId()
        : startupLanguageId;
    if(!m_localization->setLanguage(m_languageId, false, &localizationError)) {
        throw std::runtime_error(localizationError.toStdString());
    }
    m_languageId = m_localization->currentLanguageId();
    setWindowTitle(QStringLiteral("%1 - %2")
        .arg(uiText("window.title"), m_platformComposition.displayName));
    applyIndustrialStyle();

    m_sdk = createRobotSdk();
    if(m_sdk != nullptr) {
        m_robotLoader = m_sdk->createRobotLoader();
    }

    m_viewport = new RobotViewport(this);
    m_viewport->setDefaultBackgroundColor(viewportBackgroundForTheme(
        robot_qt_viewer::ThemeManager::savedTheme()));
    m_viewportProjectState =
        std::make_unique<robot_qt_viewer::RobotQtViewerViewportProjectState>();
    m_documentViewport =
        std::make_unique<robot_qt_viewer::RobotQtViewerDocumentViewportAdapter>(
            *m_viewport,
            *m_viewportProjectState);
    m_selectionViewport =
        std::make_unique<robot_qt_viewer::RobotQtViewerSelectionViewportAdapter>(*m_viewport);
    m_assemblyViewport =
        std::make_unique<robot_qt_viewer::RobotQtViewerAssemblyViewportAdapter>(
            *m_viewport,
            *m_viewportProjectState);
    m_collisionViewport =
        std::make_unique<robot_qt_viewer::RobotQtViewerCollisionViewportAdapter>(
            *m_viewport,
            *m_viewportProjectState);
    m_visualizationViewport =
        std::make_unique<robot_qt_viewer::RobotQtViewerVisualizationViewportAdapter>(*m_viewport);
    m_robotRunService =
        std::make_unique<robot_qt_viewer::RobotQtViewerRobotRunServiceAdapter>(*m_viewport);
    m_viewportEventController = new robot_qt_viewer::RobotQtViewerViewportEventController(
        *m_selectionViewport,
        *m_assemblyViewport,
        *m_collisionViewport,
        m_viewportPreviewState,
        [this](const QString& robotId, const QString& linkName) {
            return m_sceneExplorerWorkbenchPort != nullptr &&
                m_sceneExplorerWorkbenchPort->linkFrameVisible(robotId, linkName);
        },
        this);
    m_documentViewRegistry.registerModule(QStringLiteral("viewport"), m_viewportEventController,
        [this](const robot_qt_viewer::RobotQtViewerEvent& event) {
            m_viewportEventController->handleEvent(event);
        });
    m_toolSetupServices =
        std::make_unique<robot_qt_viewer::RobotQtViewerToolSetupAppServicesAdapter>(m_appController);
    m_collisionWorkbenchServices =
        std::make_unique<robot_qt_viewer::RobotQtViewerCollisionWorkbenchServicesAdapter>(m_appController);
    m_sceneExplorerActionRouter =
        std::make_unique<robot_qt_viewer::RobotQtViewerSceneExplorerActionRouter>();
    m_sceneExplorerActionRouter->setParentWidget(this);
    m_sceneExplorerActionRouter->setEnterToolSetupWorkbenchCallback([this]() {
        enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup, QStringLiteral("configureRobotFlange"));
        return m_workbenchManager.activeWorkbench() ==
            robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup;
    });
    m_sceneExplorerActionRouter->setDeleteSelectedEntityCallback([this]() {
        deleteSelectedRobot();
    });
    m_sceneExplorerActionRouter->setReloadViewportCallback([this]() {
        reloadViewportProject();
    });
    m_sceneExplorerActionRouter->setSelectRobotContextCallback(
        [this](const QString& robotId, const QString& linkName) {
            selectRobotContext(robotId, linkName);
        });
    m_sceneExplorerActionRouter->setSelectObjectContextCallback(
        [this](const QString& objectId) {
            selectRobotContext(QString());
            m_appController.setObjectInspectorContext(objectId);
            m_selectionModel.selectSceneObject(objectId, QStringLiteral("sceneExplorerCollisionModel"));
        });
    m_motionPlanningViewport = std::make_unique<robot_qt_viewer::RobotQtViewerMotionPlanningViewportAdapter>(*m_viewport);
    m_documentContext.setMotionPlanningViewport(m_motionPlanningViewport.get());
    m_documentContext.setDocumentViewport(m_documentViewport.get());
    m_documentContext.setSelectionViewport(m_selectionViewport.get());
    m_documentContext.setAssemblyViewport(m_assemblyViewport.get());
    m_documentContext.setCollisionViewport(m_collisionViewport.get());
    m_documentContext.setVisualizationViewport(m_visualizationViewport.get());
    setCentralWidget(m_viewport);
    m_viewportPresentationController =
        std::make_unique<robot_qt_viewer::RobotQtViewerViewportPresentationController>(
            *this,
            *m_viewport);
    connect(m_viewport, &RobotViewport::backgroundDoubleClicked, this, [this]() {
        const bool active = m_viewportPresentationController != nullptr &&
            m_viewportPresentationController->isActive();
        setViewportPresentationMode(!active);
    });
    connect(m_viewport, &RobotViewport::robotLinksAvailable, this, &MainWindow::addRobotLinksToTree);
    connect(m_viewport, &RobotViewport::sceneObjectAvailable, this, &MainWindow::addSceneObjectToTree);
    connect(m_viewport, &RobotViewport::scenePicked, this,
        [this](
            const QString& kind,
            const QString& robotId,
            const QString& linkName,
            const QString& robotMountId,
            const QString& mountedAttachmentId,
            const QString& sceneObjectId) {
            const robot_qt_viewer::SceneExplorerNodeRef node =
                robot_qt_viewer::SceneSelectionController::nodeFromViewportPick(
                    kind,
                    robotId,
                    linkName,
                    robotMountId,
                    mountedAttachmentId,
                    sceneObjectId);
            if(node.kind != robot_qt_viewer::SceneExplorerNodeKind::Unknown) {
                handleSceneExplorerNodeActivated(node, 0);
            }
        });
    connect(m_viewport, &RobotViewport::robotStateUpdated, this, [this]() {
        if(m_robotRunWorkbenchPort != nullptr) {
            m_robotRunWorkbenchPort->handleRobotStateUpdated();
        }
    });

    createActions();
    createPanels();
    initializeWorkbenchContributions();

    m_operationProgressBar = new QProgressBar(this);
    m_operationProgressBar->setRange(0, 100);
    m_operationProgressBar->setFixedWidth(150);
    m_operationProgressBar->setTextVisible(false);
    m_operationProgressBar->hide();
    statusBar()->addPermanentWidget(m_operationProgressBar);

    const std::filesystem::path defaultProjectPath =
        simulation_project::RuntimePaths::configRoot() / ROBOT_QT_VIEWER_DEFAULT_PROJECT_PATH;
    const QString startupOperationId = beginProjectOpenOperation(
        defaultProjectPath,
        QStringLiteral("startup"));
    const robot_qt_viewer::ProjectSessionWorkflowResult startupResult =
        m_appController.loadStartupProject(
            defaultProjectPath,
            QStringLiteral("startup"),
            startupOperationId);
    if(startupResult.success) {
        reloadViewportProject(startupOperationId);
    } else {
        statusBar()->showMessage(QString("Default project load failed: %1").arg(startupResult.message), 5000);
    }
    if(m_workbenchTransitionCoordinator != nullptr) {
        showWorkbenchTransitionResult(
            m_workbenchTransitionCoordinator->initializeActiveWorkbench(
                QStringLiteral("startupWorkbench")));
    }

    const char* version = getSdkVersion();
    m_statusLabel = new QLabel(QString("RobotSDK %1").arg(version ? version : "unknown"), this);
    statusBar()->addPermanentWidget(m_statusLabel);
    statusBar()->showMessage(uiText("status.ready"));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == m_viewport &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateCameraViewOverlayGeometry();
        updateThicknessLegendOverlayGeometry();
    }
    return QMainWindow::eventFilter(watched, event);
}

MainWindow::~MainWindow()
{
    if(m_workbenchTransitionCoordinator != nullptr) {
        m_workbenchTransitionCoordinator->shutdownAll(QStringLiteral("mainWindowDestructor"));
    }
    if(m_sdk != nullptr) {
        if(m_robotModel != nullptr) {
            m_sdk->destroyRobotModel(m_robotModel);
            m_robotModel = nullptr;
        }

        if(m_robotLoader != nullptr) {
            m_sdk->destroyRobotLoader(m_robotLoader);
            m_robotLoader = nullptr;
        }
    }

    destroyRobotSdk(m_sdk);
    m_sdk = nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if(event == nullptr) {
        return;
    }
    if(m_workbenchTransitionCoordinator == nullptr ||
        m_workbenchTransitionCoordinator->shutdownComplete()) {
        QMainWindow::closeEvent(event);
        return;
    }

    const auto prepare = m_workbenchTransitionCoordinator->prepareActiveDeactivation(
        robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ApplicationShutdown,
        QStringLiteral("applicationClose"),
        this);
    if(!prepare.succeeded()) {
        showWorkbenchTransitionResult(prepare);
        event->ignore();
        return;
    }

    if(m_projectSession.isDirty()) {
        const QMessageBox::StandardButton choice = QMessageBox::warning(
            this,
            QStringLiteral("Close RobotQtViewer"),
            QStringLiteral("The current project has unsaved changes. Save them before closing?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if(choice == QMessageBox::Cancel) {
            m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
            event->ignore();
            return;
        }
        if(choice == QMessageBox::Save) {
            saveProject();
            if(m_projectSession.isDirty()) {
                m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
                event->ignore();
                return;
            }
        }
    }

    const auto deactivate = m_workbenchTransitionCoordinator->deactivateActive(
        robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ApplicationShutdown,
        QStringLiteral("applicationClose"),
        this,
        true);
    if(!deactivate.succeeded()) {
        showWorkbenchTransitionResult(deactivate);
        event->ignore();
        return;
    }
    m_workbenchTransitionCoordinator->shutdownAll(QStringLiteral("applicationClose"));
    event->accept();
    QMainWindow::closeEvent(event);
}

void MainWindow::applyIndustrialStyle()
{
    if(qApp != nullptr) {
        const robot_qt_viewer::ThemeKind theme = robot_qt_viewer::ThemeManager::savedTheme();
        robot_qt_viewer::ThemeManager::apply(*qApp, theme);
        robot_qt_viewer::ThemeManager::applyNativeWindowFrame(*this, theme);
    }
}

void MainWindow::applyTheme(const QString& themeName)
{
    if(qApp == nullptr) {
        return;
    }

    const robot_qt_viewer::ThemeKind theme =
        robot_qt_viewer::ThemeManager::themeFromName(themeName);
    robot_qt_viewer::ThemeManager::apply(*qApp, theme);
    robot_qt_viewer::ThemeManager::applyNativeWindowFrame(*this, theme);
    if(m_viewport != nullptr) {
        m_viewport->setDefaultBackgroundColor(viewportBackgroundForTheme(theme));
    }
    robot_qt_viewer::ThemeManager::save(theme);
}

void MainWindow::applyLanguage(const QString& languageName)
{
    if(m_languageCoordinator == nullptr) {
        return;
    }
    QString errorMessage;
    if(!m_languageCoordinator->switchLanguage(languageName, &errorMessage)) {
        statusBar()->showMessage(errorMessage, 8000);
        return;
    }
    m_languageId = m_localization->currentLanguageId();
}

QString MainWindow::uiText(const QString& key) const
{
    return m_localization == nullptr ? key : m_localization->text(key, key);
}

QString MainWindow::beginProjectOpenOperation(
    const std::filesystem::path& path,
    const QString& sourceId)
{
    return m_operationStatusStore.begin(
        sourceId,
        QStringLiteral("status.operation.projectOpen.title"),
        QStringLiteral("Opening %1"),
        QString::fromStdWString(path.filename().wstring()),
        3,
        true);
}

void MainWindow::refreshOperationStatusPresentation(const QString& focusOperationId)
{
    if(m_operationStatusPresenter == nullptr) {
        return;
    }

    const robot_qt_viewer::RobotQtViewerOperationStatusViewModel viewModel =
        m_operationStatusPresenter->build(m_operationStatusStore.history(), focusOperationId);
    if(!viewModel.currentMessage.isEmpty()) {
        statusBar()->showMessage(viewModel.currentMessage, viewModel.timeoutMs);
    }
    if(m_operationProgressBar != nullptr) {
        m_operationProgressBar->setVisible(viewModel.progressVisible);
        if(viewModel.progressVisible) {
            m_operationProgressBar->setRange(
                viewModel.progressMinimum,
                viewModel.progressMaximum);
            m_operationProgressBar->setValue(viewModel.progressValue);
        }
    }
    if(m_statusPanelWidget != nullptr) {
        m_statusPanelWidget->setStatusItems(viewModel.historyItems);
    }

    // Project loading is currently synchronous on the GUI thread. Repaint only
    // the presentation surface so each published phase is visible without
    // dispatching nested user-input or business events.
    statusBar()->repaint();
    if(m_operationProgressBar != nullptr && m_operationProgressBar->isVisible()) {
        m_operationProgressBar->repaint();
    }
}

void MainWindow::retranslateUi(
    const robot_qt_viewer::RobotQtViewerLocalizationService&) noexcept
{
    setWindowTitle(QStringLiteral("%1 - %2")
        .arg(uiText("window.title"), m_platformComposition.displayName));

    refreshOperationStatusPresentation();

    if(m_fileMenu != nullptr) {
        m_fileMenu->setTitle(uiText("menu.file"));
    }
    if(m_viewMenu != nullptr) {
        m_viewMenu->setTitle(uiText("menu.view"));
    }
    if(m_windowMenu != nullptr) {
        m_windowMenu->setTitle(uiText("menu.window"));
    }
    if(m_themeMenu != nullptr) {
        m_themeMenu->setTitle(uiText("menu.theme"));
    }
    if(m_languageMenu != nullptr) {
        m_languageMenu->setTitle(uiText("menu.language"));
    }
    if(m_cameraViewMenu != nullptr) {
        m_cameraViewMenu->setTitle(uiText("menu.cameraViews"));
    }
    if(m_environmentMenu != nullptr) {
        m_environmentMenu->setTitle(uiText("menu.environment"));
    }
    if(m_bottomPanelDock != nullptr) {
        m_bottomPanelDock->setWindowTitle(uiText("action.robotRunDetails"));
    }
    if(m_toolbarController != nullptr) {
        robot_qt_viewer::RobotQtViewerToolbarTexts texts;
        texts.toolbarTitle = uiText("toolbar.simulation");
        texts.sceneGroup = uiText("toolbar.sceneEdit");
        texts.robotEditGroup = uiText("toolbar.robotEdit");
        texts.viewGroup = uiText("toolbar.view");
        texts.workbenchGroup = uiText("toolbar.workbenches");
        m_toolbarController->retranslate(texts);
    }

    auto setAction = [this](QAction* action, const QString& text) {
        if(m_toolbarController != nullptr) {
            m_toolbarController->setActionTextAndToolTip(action, text);
            return;
        }
        if(action != nullptr) {
            action->setText(text);
            action->setToolTip(text);
            action->setStatusTip(text);
        }
    };

    for(auto it = m_themeActions.begin(); it != m_themeActions.end(); ++it) {
        it.value()->setText(uiText(QString("theme.%1").arg(it.key())));
    }
    for(auto it = m_languageActions.begin(); it != m_languageActions.end(); ++it) {
        it.value()->setText(m_localization->languageNativeName(it.key()));
        it.value()->setChecked(it.key() == m_localization->currentLanguageId());
    }

    if(m_loadRobotAction != nullptr) {
        setAction(m_loadRobotAction, uiText("action.loadRobot"));
    }
    if(m_newProjectAction != nullptr) {
        setAction(m_newProjectAction, uiText("action.newProject"));
    }
    if(m_openProjectAction != nullptr) {
        setAction(m_openProjectAction, uiText("action.openProject"));
    }
    if(m_saveProjectAction != nullptr) {
        setAction(m_saveProjectAction, uiText("action.saveProject"));
    }
    if(m_saveProjectAsAction != nullptr) {
        setAction(m_saveProjectAsAction, uiText("action.saveProjectAs"));
    }
    if(m_saveProjectAsV3Action != nullptr) {
        setAction(m_saveProjectAsV3Action, uiText("action.saveProjectAsV3"));
    }
    if(m_importRobotPackageAction != nullptr) {
        setAction(m_importRobotPackageAction, uiText("action.importRobotPackage"));
    }
    if(m_exportRobotPackageAction != nullptr) {
        setAction(m_exportRobotPackageAction, uiText("action.exportRobotPackage"));
    }
    if(m_saveCollisionOverridesAction != nullptr) {
        setAction(m_saveCollisionOverridesAction, uiText("action.saveCollisionOverrides"));
    }
    if(m_saveCollisionSidecarAction != nullptr) {
        setAction(m_saveCollisionSidecarAction, uiText("action.saveCollisionSidecar"));
    }
    if(m_exportCollisionUrdfAction != nullptr) {
        setAction(m_exportCollisionUrdfAction, uiText("action.exportCollisionUrdf"));
    }
    if(m_importRobotAction != nullptr) {
        setAction(m_importRobotAction, uiText("action.importRobot"));
    }
    if(m_importObjectAction != nullptr) {
        setAction(m_importObjectAction, uiText("action.importObject"));
    }
    if(m_addCameraAction != nullptr) {
        setAction(m_addCameraAction, uiText("action.addCamera"));
    }
    if(m_importPointCloudAction != nullptr) {
        setAction(m_importPointCloudAction, uiText("action.importPointCloud"));
    }
    if(m_deleteRobotAction != nullptr) {
        setAction(m_deleteRobotAction, uiText("action.deleteSelectedItem"));
    }
    if(m_saveImageAction != nullptr) {
        setAction(m_saveImageAction, uiText("action.saveImage"));
    }
    if(m_resetCameraAction != nullptr) {
        setAction(m_resetCameraAction, uiText("action.viewOrientation"));
    }
    updateViewportPresentationAction();
    for(auto it = m_cameraViewActions.begin(); it != m_cameraViewActions.end(); ++it) {
        setAction(it.value(), uiText(QString("action.cameraView.%1").arg(it.key())));
    }
    for(auto it = m_environmentActions.begin(); it != m_environmentActions.end(); ++it) {
        setAction(it.value(), uiText(QString("action.environment.%1").arg(it.key())));
    }
    if(m_collisionGeometryAction != nullptr) {
        setAction(m_collisionGeometryAction, uiText("action.collisionGeometry"));
    }
    if(m_collisionQueriesAction != nullptr) {
        setAction(m_collisionQueriesAction, uiText("action.collisionQueries"));
    }
    if(m_robotRunDetailsAction != nullptr) {
        setAction(m_robotRunDetailsAction, uiText("action.robotRunDetails"));
    }
    if(m_configurePlatformAction != nullptr) {
        setAction(m_configurePlatformAction, uiText("action.configurePlatform"));
    }
    if(m_browseWorkbenchAction != nullptr) {
        setAction(m_browseWorkbenchAction, uiText("action.projectAssemblyWorkbench"));
    }
    if(m_motionWorkbenchAction != nullptr) {
        setAction(m_motionWorkbenchAction, uiText("action.robotRunWorkbench"));
    }
    if(m_toolSetupWorkbenchAction != nullptr) {
        setAction(m_toolSetupWorkbenchAction, uiText("action.toolSetupWorkbench"));
    }
    if(m_addLinkMountAction != nullptr) {
        setAction(m_addLinkMountAction, uiText("action.addLinkMount"));
    }
    if(m_collisionWorkbenchAction != nullptr) {
        setAction(m_collisionWorkbenchAction, uiText("action.collisionConfigWorkbench"));
    }
    if(m_trajectoryPlanningWorkbenchAction != nullptr) {
        setAction(m_trajectoryPlanningWorkbenchAction,
            uiText("action.trajectoryPlanningWorkbench"));
    }
    if(m_sprayProcessWorkbenchAction != nullptr) {
        setAction(m_sprayProcessWorkbenchAction, uiText("action.sprayProcessWorkbench"));
    }
    if(m_coatingAnalysisWorkbenchAction != nullptr) {
        setAction(m_coatingAnalysisWorkbenchAction,
            uiText("action.coatingAnalysisWorkbench"));
    }
    if(m_digitalTwinWorkbenchAction != nullptr) {
        setAction(m_digitalTwinWorkbenchAction, uiText("action.digitalTwinWorkbench"));
    }
}

void MainWindow::createActions()
{
    m_fileMenu = menuBar()->addMenu(QString());
    m_viewMenu = menuBar()->addMenu(QString());
    m_windowMenu = menuBar()->addMenu(QString());
    m_themeMenu = m_viewMenu->addMenu(QString());
    m_languageMenu = m_viewMenu->addMenu(QString());
    m_environmentMenu = new QMenu(this);

    QActionGroup* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    const robot_qt_viewer::ThemeKind savedTheme = robot_qt_viewer::ThemeManager::savedTheme();
    for(const QString& themeName : { QString("Modern"), QString("Dark"), QString("Light") }) {
        QAction* themeAction = m_themeMenu->addAction(QString());
        themeAction->setCheckable(true);
        themeAction->setActionGroup(themeGroup);
        themeAction->setChecked(
            robot_qt_viewer::ThemeManager::themeFromName(themeName) == savedTheme);
        connect(themeAction, &QAction::triggered, this, [this, themeName]() {
            applyTheme(themeName);
        });
        m_themeActions.insert(themeName, themeAction);
    }

    QActionGroup* languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    for(const robot_qt_viewer::RobotQtViewerLanguageInfo& language :
        m_localization->availableLanguages()) {
        QAction* languageAction = m_languageMenu->addAction(QString());
        languageAction->setCheckable(true);
        languageAction->setActionGroup(languageGroup);
        languageAction->setChecked(language.id == m_localization->currentLanguageId());
        connect(languageAction, &QAction::triggered, this, [this, languageId = language.id]() {
            applyLanguage(languageId);
        });
        m_languageActions.insert(language.id, languageAction);
    }

    m_loadRobotAction = new QAction(this);
    connect(m_loadRobotAction, &QAction::triggered, this, &MainWindow::importRobot);

    m_newProjectAction = new QAction(this);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::newProject);

    m_openProjectAction = new QAction(this);
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);

    m_saveProjectAction = new QAction(this);
    connect(m_saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);

    m_saveProjectAsAction = new QAction(this);
    connect(m_saveProjectAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    m_saveProjectAsV3Action = new QAction(this);
    connect(m_saveProjectAsV3Action, &QAction::triggered, this, &MainWindow::saveProjectAsV3);

    m_importRobotPackageAction = new QAction(this);
    connect(m_importRobotPackageAction, &QAction::triggered, this, &MainWindow::importRobotPackage);

    m_exportRobotPackageAction = new QAction(this);
    connect(m_exportRobotPackageAction, &QAction::triggered, this, &MainWindow::exportSelectedRobotPackage);

    m_saveCollisionOverridesAction = new QAction(this);
    connect(m_saveCollisionOverridesAction, &QAction::triggered, this, [this]() {
        if(m_collisionWorkbenchPort != nullptr) {
            m_collisionWorkbenchPort->requestSaveOverridesToProject();
        }
    });

    m_saveCollisionSidecarAction = new QAction(this);
    connect(m_saveCollisionSidecarAction, &QAction::triggered, this, [this]() {
        if(m_collisionWorkbenchPort != nullptr) {
            m_collisionWorkbenchPort->requestSaveOverridesAsSidecar(this);
        }
    });

    m_exportCollisionUrdfAction = new QAction(this);
    connect(m_exportCollisionUrdfAction, &QAction::triggered, this, [this]() {
        if(m_collisionWorkbenchPort != nullptr) {
            m_collisionWorkbenchPort->requestExportRobotUrdfWithCollision(this);
        }
    });

    m_importRobotAction = new QAction(this);
    connect(m_importRobotAction, &QAction::triggered, this, &MainWindow::importRobot);

    m_importObjectAction = new QAction(this);
    connect(m_importObjectAction, &QAction::triggered, this, &MainWindow::importObject);

    m_addCameraAction = new QAction(this);
    m_addCameraAction->setObjectName(QStringLiteral("addCameraDefinitionAction"));
    connect(m_addCameraAction, &QAction::triggered, this, &MainWindow::addCameraDefinition);

    m_importPointCloudAction = new QAction(this);
    connect(m_importPointCloudAction, &QAction::triggered, this, &MainWindow::importPointCloud);

    m_deleteRobotAction = new QAction(this);
    connect(m_deleteRobotAction, &QAction::triggered, this, &MainWindow::deleteSelectedRobot);

    m_saveImageAction = new QAction(this);
    connect(m_saveImageAction, &QAction::triggered, this, [this]() {
        saveViewportImage();
    });

    m_resetCameraAction = new QAction(this);
    connect(m_resetCameraAction, &QAction::triggered, this, &MainWindow::showCameraViewPalette);

    m_viewportPresentationAction = new QAction(this);
    m_viewportPresentationAction->setCheckable(true);
    m_viewportPresentationAction->setShortcut(QKeySequence(Qt::Key_F11));
    m_viewportPresentationAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_viewportPresentationAction, &QAction::triggered, this, [this](bool checked) {
        setViewportPresentationMode(checked);
    });

    m_cameraViewMenu = new QMenu(this);
    auto addCameraViewAction = [this](const QString& id, ProjectSceneCameraView view) {
        QAction* action = m_cameraViewMenu->addAction(QString());
        action->setIcon(QIcon(cameraViewIconResourcePath(id)));
        connect(action, &QAction::triggered, this, [this, view]() {
            m_viewport->setCameraView(view);
        });
        m_cameraViewActions.insert(id, action);
    };
    addCameraViewAction(QStringLiteral("home"), ProjectSceneCameraView::Home);
    addCameraViewAction(QStringLiteral("isometric"), ProjectSceneCameraView::Isometric);
    m_cameraViewMenu->addSeparator();
    addCameraViewAction(QStringLiteral("front"), ProjectSceneCameraView::Front);
    addCameraViewAction(QStringLiteral("back"), ProjectSceneCameraView::Back);
    addCameraViewAction(QStringLiteral("left"), ProjectSceneCameraView::Left);
    addCameraViewAction(QStringLiteral("right"), ProjectSceneCameraView::Right);
    addCameraViewAction(QStringLiteral("top"), ProjectSceneCameraView::Top);
    addCameraViewAction(QStringLiteral("bottom"), ProjectSceneCameraView::Bottom);

    auto* environmentGroup = new QActionGroup(this);
    environmentGroup->setExclusive(true);
    ProjectSceneEnvironmentPreset savedEnvironment = ProjectSceneEnvironmentPreset::Factory;
    const QString savedEnvironmentId = QSettings().value(
        QStringLiteral("view/environmentPreset"),
        QStringLiteral("factory")).toString();
    parseProjectSceneEnvironmentPreset(savedEnvironmentId.toStdString(), savedEnvironment);
    auto addEnvironmentAction = [this, environmentGroup, savedEnvironment](
        const QString& id,
        ProjectSceneEnvironmentPreset preset) {
        QAction* action = m_environmentMenu->addAction(QString());
        action->setCheckable(true);
        action->setActionGroup(environmentGroup);
        action->setChecked(savedEnvironment == preset);
        connect(action, &QAction::triggered, this, [this, id, preset]() {
            m_viewport->setEnvironmentPreset(preset);
            QSettings().setValue(QStringLiteral("view/environmentPreset"), id);
        });
        m_environmentActions.insert(id, action);
    };
    addEnvironmentAction(QStringLiteral("studio"), ProjectSceneEnvironmentPreset::Studio);
    addEnvironmentAction(QStringLiteral("factory"), ProjectSceneEnvironmentPreset::Factory);
    addEnvironmentAction(QStringLiteral("workshop"), ProjectSceneEnvironmentPreset::Workshop);
    addEnvironmentAction(QStringLiteral("home"), ProjectSceneEnvironmentPreset::Home);
    m_environmentMenu->addSeparator();
    addEnvironmentAction(QStringLiteral("none"), ProjectSceneEnvironmentPreset::None);
    m_viewport->setEnvironmentPreset(savedEnvironment);

    m_collisionGeometryAction = new QAction(this);
    m_collisionGeometryAction->setCheckable(true);
    m_collisionGeometryAction->setChecked(false);
    connect(m_collisionGeometryAction, &QAction::toggled, this, [this](bool visible) {
        m_appController.setCollisionGeometryVisible(visible, QStringLiteral("collisionGeometryVisibility"));
        m_viewport->setCollisionGeometryVisible(visible);
    });

    m_collisionQueriesAction = new QAction(this);
    m_collisionQueriesAction->setCheckable(true);
    m_collisionQueriesAction->setChecked(false);
    connect(m_collisionQueriesAction, &QAction::toggled, this, [this](bool enabled) {
        m_viewport->setCollisionQueriesEnabled(enabled);
        if(m_statusLabel != nullptr) {
            m_statusLabel->setText(enabled
                ? QStringLiteral("Collision detection enabled")
                : QStringLiteral("Collision detection paused"));
        }
    });

    m_robotRunDetailsAction = new QAction(this);
    m_robotRunDetailsAction->setCheckable(true);
    m_robotRunDetailsAction->setChecked(false);
    connect(m_robotRunDetailsAction, &QAction::toggled, this, [this](bool checked) {
        setRobotRunDetailsRequested(checked);
    });

    m_configurePlatformAction = new QAction(this);
    connect(m_configurePlatformAction, &QAction::triggered,
        this, &MainWindow::configureSimulationPlatform);

    auto* workbenchGroup = new QActionGroup(this);
    workbenchGroup->setExclusive(true);

    m_browseWorkbenchAction = new QAction(this);
    m_browseWorkbenchAction->setCheckable(true);
    m_browseWorkbenchAction->setActionGroup(workbenchGroup);
    m_browseWorkbenchAction->setChecked(true);
    connect(m_browseWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse, QStringLiteral("browseAction"));
    });

    m_motionWorkbenchAction = new QAction(this);
    m_motionWorkbenchAction->setCheckable(true);
    m_motionWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_motionWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::Motion, QStringLiteral("motionAction"));
    });

    m_toolSetupWorkbenchAction = new QAction(this);
    m_toolSetupWorkbenchAction->setCheckable(true);
    m_toolSetupWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_toolSetupWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup,
            QStringLiteral("toolSetupAction"));
    });

    m_addLinkMountAction = new QAction(this);
    connect(m_addLinkMountAction, &QAction::triggered, this, [this]() {
        if(!currentSceneExplorerNodeIsLink()) {
            return;
        }
        if(!prepareEditSessionTransition(
               robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::TaskHandoff,
               QStringLiteral("addLinkMountAction"))) {
            updateWorkbenchActions();
            return;
        }
        if(m_workbenchManager.activeWorkbench() != robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) {
            enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup, QStringLiteral("addLinkMountAction"));
        }
        if(m_workbenchManager.activeWorkbench() != robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) {
            return;
        }
        if(m_toolSetupWorkbenchPort != nullptr) {
            m_toolSetupWorkbenchPort->createRobotMountForSelectedLink();
        }
    });

    m_collisionWorkbenchAction = new QAction(this);
    m_collisionWorkbenchAction->setCheckable(true);
    m_collisionWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_collisionWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::Collision, QStringLiteral("collisionAction"));
    });

    m_trajectoryPlanningWorkbenchAction = new QAction(this);
    m_trajectoryPlanningWorkbenchAction->setCheckable(true);
    m_trajectoryPlanningWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_trajectoryPlanningWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::TrajectoryPlanning,
            QStringLiteral("motionPlanningAction"));
    });

    m_sprayProcessWorkbenchAction = new QAction(this);
    m_sprayProcessWorkbenchAction->setCheckable(true);
    m_sprayProcessWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_sprayProcessWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::SprayProcess,
            QStringLiteral("sprayProcessAction"));
    });

    m_coatingAnalysisWorkbenchAction = new QAction(this);
    m_coatingAnalysisWorkbenchAction->setCheckable(true);
    m_coatingAnalysisWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_coatingAnalysisWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::CoatingAnalysis,
            QStringLiteral("coatingAnalysisAction"));
    });

    m_digitalTwinWorkbenchAction = new QAction(this);
    m_digitalTwinWorkbenchAction->setCheckable(true);
    m_digitalTwinWorkbenchAction->setActionGroup(workbenchGroup);
    connect(m_digitalTwinWorkbenchAction, &QAction::triggered, this, [this]() {
        enterWorkbench(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::DigitalTwin,
            QStringLiteral("digitalTwinAction"));
    });

    if(m_workbenchPluginLoader != nullptr) {
        for(const QString& workbenchId : m_workbenchPluginLoader->loadedWorkbenchIds()) {
            const robot_qt_viewer::RobotQtViewerWorkbenchDesc* workbench =
                m_workbenchPackageRegistry.workbench(workbenchId);
            if(workbench == nullptr) {
                continue;
            }
            auto* action = new QAction(workbench->descriptor.displayName, this);
            action->setObjectName(workbench->toolbarActionId.isEmpty()
                ? workbenchId
                : workbench->toolbarActionId);
            action->setCheckable(true);
            action->setActionGroup(workbenchGroup);
            connect(action, &QAction::triggered, this, [this, workbenchId]() {
                enterWorkbench(workbenchId, QStringLiteral("pluginWorkbenchAction"));
            });
            m_dynamicWorkbenchActions.insert(workbenchId, action);
        }
    }

    m_fileMenu->addAction(m_newProjectAction);
    m_fileMenu->addAction(m_openProjectAction);
    m_fileMenu->addAction(m_saveProjectAction);
    m_fileMenu->addAction(m_saveProjectAsAction);
    m_fileMenu->addAction(m_saveProjectAsV3Action);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_importRobotPackageAction);
    m_fileMenu->addAction(m_exportRobotPackageAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_saveCollisionOverridesAction);
    m_fileMenu->addAction(m_saveCollisionSidecarAction);
    m_fileMenu->addAction(m_exportCollisionUrdfAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_importRobotAction);
    m_fileMenu->addAction(m_importObjectAction);
    m_fileMenu->addAction(m_addCameraAction);
    m_fileMenu->addAction(m_importPointCloudAction);
    m_fileMenu->addAction(m_deleteRobotAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_loadRobotAction);
    m_fileMenu->addAction(m_saveImageAction);
    m_viewMenu->addAction(m_viewportPresentationAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_resetCameraAction);
    m_viewMenu->addMenu(m_cameraViewMenu);
    m_viewMenu->addMenu(m_environmentMenu);
    m_viewMenu->addAction(m_collisionGeometryAction);
    m_viewMenu->addAction(m_collisionQueriesAction);
    if(!m_dynamicWorkbenchActions.isEmpty()) {
        m_viewMenu->addSeparator();
        for(const QString& workbenchId : m_platformComposition.enabledWorkbenchIds) {
            if(QAction* action = m_dynamicWorkbenchActions.value(workbenchId, nullptr)) {
                m_viewMenu->addAction(action);
            }
        }
    }
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_configurePlatformAction);

    m_toolbarController = new robot_qt_viewer::RobotQtViewerToolbarController(*this, this);
    robot_qt_viewer::RobotQtViewerToolbarActions toolbarActions;
    toolbarActions.newProject = m_newProjectAction;
    toolbarActions.openProject = m_openProjectAction;
    toolbarActions.saveProject = m_saveProjectAction;
    toolbarActions.saveProjectAs = m_saveProjectAsAction;
    toolbarActions.saveCollisionOverrides = m_saveCollisionOverridesAction;
    toolbarActions.importRobot = m_importRobotAction;
    toolbarActions.importObject = m_importObjectAction;
    toolbarActions.addCamera = m_addCameraAction;
    toolbarActions.importPointCloud = m_importPointCloudAction;
    toolbarActions.deleteSelectedItem = m_deleteRobotAction;
    toolbarActions.saveImage = m_saveImageAction;
    toolbarActions.resetCamera = m_resetCameraAction;
    toolbarActions.collisionGeometry = m_collisionGeometryAction;
    toolbarActions.collisionQueries = m_collisionQueriesAction;
    toolbarActions.browseWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse) ? m_browseWorkbenchAction : nullptr;
    toolbarActions.motionWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::Motion) ? m_motionWorkbenchAction : nullptr;
    toolbarActions.toolSetupWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) ? m_toolSetupWorkbenchAction : nullptr;
    toolbarActions.addLinkMount = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) ? m_addLinkMountAction : nullptr;
    toolbarActions.collisionWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::Collision) ? m_collisionWorkbenchAction : nullptr;
    toolbarActions.trajectoryPlanningWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::TrajectoryPlanning)
        ? m_trajectoryPlanningWorkbenchAction : nullptr;
    toolbarActions.sprayProcessWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::SprayProcess)
        ? m_sprayProcessWorkbenchAction : nullptr;
    toolbarActions.coatingAnalysisWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::CoatingAnalysis)
        ? m_coatingAnalysisWorkbenchAction : nullptr;
    toolbarActions.digitalTwinWorkbench = m_workbenchPackageRegistry.hasWorkbench(
        robot_qt_viewer::RobotQtViewerWorkbenchKind::DigitalTwin)
        ? m_digitalTwinWorkbenchAction : nullptr;
    QStringList workbenchActionOrder;
    for(const QString& workbenchId : m_platformComposition.enabledWorkbenchIds) {
        const robot_qt_viewer::RobotQtViewerWorkbenchDesc* workbenchDesc =
            m_workbenchPackageRegistry.registeredWorkbench(workbenchId);
        if(workbenchDesc != nullptr && !workbenchDesc->toolbarActionId.isEmpty()) {
            workbenchActionOrder.push_back(workbenchDesc->toolbarActionId);
            if(QAction* dynamicAction = m_dynamicWorkbenchActions.value(workbenchId, nullptr)) {
                toolbarActions.dynamicWorkbenchActions.insert(
                    workbenchDesc->toolbarActionId, dynamicAction);
            }
        }
    }
    m_toolbarController->build(toolbarActions, workbenchActionOrder);
    createCameraViewOverlay();

    retranslateUi(*m_localization);
    updateWorkbenchActions();
}

void MainWindow::configureSimulationPlatform()
{
    robot_qt_viewer::RobotQtViewerPlatformConfigurationDialog dialog(
        m_workbenchPackageRegistry,
        m_platformProfilesDirectory,
        m_platformProfile.id,
        this);
    if(dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString targetProfileId = dialog.selectedProfileId();
    if(targetProfileId.isEmpty()) {
        return;
    }
    const bool restartRequired =
        targetProfileId != m_platformProfile.id || dialog.configurationChanged();
    if(!restartRequired) {
        persistPlatformConfiguration(targetProfileId);
        return;
    }
    LOG_INFO("rs2026") << "Product Profile switch requested: current="
        << m_platformProfile.id.toStdString()
        << ", target=" << targetProfileId.toStdString();
    restartWithPlatformConfiguration(targetProfileId);
}

void MainWindow::restartWithPlatformConfiguration(const QString& profileId)
{
    if(profileId.isEmpty()) {
        return;
    }
    if(m_workbenchTransitionCoordinator != nullptr) {
        const auto prepare = m_workbenchTransitionCoordinator->prepareActiveDeactivation(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ApplicationShutdown,
            QStringLiteral("platformConfigurationRestart"),
            this);
        if(!prepare.succeeded()) {
            showWorkbenchTransitionResult(prepare);
            return;
        }
    }

    if(m_projectSession.isDirty()) {
        const QMessageBox::StandardButton choice = QMessageBox::warning(
            this,
            m_localization->text(
                QStringLiteral("platform.restart.title"),
                QStringLiteral("Apply Product Profile")),
            m_localization->text(
                QStringLiteral("platform.restart.unsavedProject"),
                QStringLiteral("The current project has unsaved changes. Save them before restarting?")),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if(choice == QMessageBox::Cancel) {
            if(m_workbenchTransitionCoordinator != nullptr) {
                m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
            }
            return;
        }
        if(choice == QMessageBox::Save) {
            saveProject();
            if(m_projectSession.isDirty()) {
                if(m_workbenchTransitionCoordinator != nullptr) {
                    m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
                }
                return;
            }
        }
    }

    if(!persistPlatformConfiguration(profileId)) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
        }
        return;
    }

    if(m_workbenchTransitionCoordinator != nullptr) {
        const auto deactivate = m_workbenchTransitionCoordinator->deactivateActive(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ApplicationShutdown,
            QStringLiteral("platformConfigurationRestart"),
            this,
            true);
        if(!deactivate.succeeded()) {
            showWorkbenchTransitionResult(
                deactivate,
                m_localization->text(
                    QStringLiteral("platform.restart.savedForNextStart"),
                    QStringLiteral("The Product Profile was saved and will be used on the next restart.")));
            return;
        }
    }

    QStringList restartArguments = QCoreApplication::arguments();
    if(!restartArguments.isEmpty()) {
        restartArguments.removeFirst();
    }
    for(int index = 0; index < restartArguments.size();) {
        if(restartArguments[index] == QStringLiteral("--platform-profile")) {
            restartArguments.removeAt(index);
            if(index < restartArguments.size()) {
                restartArguments.removeAt(index);
            }
            continue;
        }
        ++index;
    }
    restartArguments.push_back(QStringLiteral("--platform-profile"));
    restartArguments.push_back(profileId);
    const bool started = QProcess::startDetached(
        QCoreApplication::applicationFilePath(),
        restartArguments,
        QCoreApplication::applicationDirPath());
    if(!started) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            showWorkbenchTransitionResult(
                m_workbenchTransitionCoordinator->activateCommittedWorkbench(
                    robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::Rollback,
                    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::Rollback,
                    QStringLiteral("platformConfigurationRestartFailed")));
        }
        QMessageBox::critical(
            this,
            m_localization->text(
                QStringLiteral("platform.restart.profileTitle"),
                QStringLiteral("Product Profile")),
            m_localization->text(
                QStringLiteral("platform.restart.failed"),
                QStringLiteral("The Product Profile was saved, but RobotQtViewer could not restart. Restart it manually to apply the change.")));
        return;
    }

    LOG_INFO("rs2026") << "Product Profile restart process launched: target="
        << profileId.toStdString();

    if(m_workbenchTransitionCoordinator != nullptr) {
        m_workbenchTransitionCoordinator->shutdownAll(
            QStringLiteral("platformConfigurationRestart"));
    }
    QCoreApplication::quit();
}

bool MainWindow::persistPlatformConfiguration(const QString& profileId)
{
    QString saveError;
    robot_qt_viewer::RobotQtViewerPlatformSelection selection;
    selection.profileId = profileId;
    if(!robot_qt_viewer::RobotQtViewerPlatformProfileIo::saveSelection(
           m_platformSelectionPath, selection, &saveError)) {
        QMessageBox::critical(
            this,
            m_localization->text(
                QStringLiteral("platform.restart.profileTitle"),
                QStringLiteral("Product Profile")),
            saveError);
        return false;
    }
    return true;
}
void MainWindow::showCameraViewPalette()
{
    QMenu palette(this);
    palette.setObjectName(QStringLiteral("RobotQtViewerCameraViewPalette"));

    auto* container = new QWidget(&palette);
    auto* grid = new QGridLayout(container);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);

    auto addButton = [&](int row, int column, QAction* action) {
        auto* button = new QToolButton(container);
        button->setProperty("cameraViewControl", true);
        if(action != nullptr) {
            button->setDefaultAction(action);
        }
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(42, 42));
        connect(button, &QToolButton::clicked, &palette, &QMenu::close);
        grid->addWidget(button, row, column);
    };

    addButton(0, 0, m_cameraViewActions.value(QStringLiteral("home")));
    addButton(0, 1, m_cameraViewActions.value(QStringLiteral("top")));
    addButton(0, 2, m_cameraViewActions.value(QStringLiteral("isometric")));
    addButton(1, 0, m_cameraViewActions.value(QStringLiteral("left")));
    addButton(1, 1, m_cameraViewActions.value(QStringLiteral("front")));
    addButton(1, 2, m_cameraViewActions.value(QStringLiteral("right")));
    addButton(2, 0, m_cameraViewActions.value(QStringLiteral("back")));
    addButton(2, 1, m_cameraViewActions.value(QStringLiteral("bottom")));

    auto* widgetAction = new QWidgetAction(&palette);
    widgetAction->setDefaultWidget(container);
    palette.addAction(widgetAction);
    palette.exec(QCursor::pos());
}

void MainWindow::createCameraViewOverlay()
{
    if(m_viewport == nullptr || m_cameraViewOverlay != nullptr) {
        return;
    }

    auto* overlay = new QFrame(m_viewport);
    overlay->setObjectName(QStringLiteral("RobotQtViewerCameraViewOverlay"));
    overlay->setAttribute(Qt::WA_StyledBackground, true);

    auto* overlayLayout = new QHBoxLayout(overlay);
    overlayLayout->setContentsMargins(4, 4, 4, 4);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(makeCameraViewButton(m_viewportPresentationAction, overlay));
    overlayLayout->addWidget(makeCameraViewButton(m_resetCameraAction, overlay));

    m_cameraViewOverlay = overlay;
    m_viewport->installEventFilter(this);
    updateCameraViewOverlayGeometry();
    m_cameraViewOverlay->show();
}

void MainWindow::updateCameraViewOverlayGeometry()
{
    if(m_viewport == nullptr || m_cameraViewOverlay == nullptr) {
        return;
    }

    m_cameraViewOverlay->adjustSize();
    const int margin = 14;
    const QSize size = m_cameraViewOverlay->sizeHint();
    const int x = std::max(margin, m_viewport->width() - size.width() - margin);
    m_cameraViewOverlay->move(x, margin);
    m_cameraViewOverlay->raise();
}

void MainWindow::setViewportPresentationMode(bool active)
{
    if(m_viewportPresentationController == nullptr) {
        return;
    }

    m_viewportPresentationController->setActive(active);
    updateViewportPresentationAction();
    updateCameraViewOverlayGeometry();
    updateThicknessLegendOverlayGeometry();
}

void MainWindow::updateViewportPresentationAction()
{
    if(m_viewportPresentationAction == nullptr) {
        return;
    }

    const bool active = m_viewportPresentationController != nullptr &&
        m_viewportPresentationController->isActive();
    const QSignalBlocker blocker(m_viewportPresentationAction);
    m_viewportPresentationAction->setChecked(active);
    m_viewportPresentationAction->setIcon(viewportPresentationIcon(active));

    const QString text = uiText(active
        ? QStringLiteral("action.exitViewportFullscreen")
        : QStringLiteral("action.enterViewportFullscreen"));
    m_viewportPresentationAction->setText(text);
    m_viewportPresentationAction->setIconText(text);
    m_viewportPresentationAction->setToolTip(text);
    m_viewportPresentationAction->setStatusTip(text);
}

void MainWindow::updateThicknessLegendOverlayGeometry()
{
    if(m_viewport == nullptr || m_thicknessLegendOverlay == nullptr) {
        return;
    }

    const int margin = 16;
    const int desiredHeight = std::max(320, std::min(520, m_viewport->height() * 3 / 5));
    const int availableHeight = std::max(1, m_viewport->height() - margin * 2);
    m_thicknessLegendOverlay->resize(
        m_thicknessLegendOverlay->width(),
        std::min(desiredHeight, availableHeight));
    const int viewportX = std::max(0,
        m_viewport->width() - m_thicknessLegendOverlay->width() - margin);
    const int viewportY = std::max(0, (m_viewport->height() - m_thicknessLegendOverlay->height()) / 2);
    const QPoint overlayPosition = m_viewport->mapTo(this, QPoint(viewportX, viewportY));
    m_thicknessLegendOverlay->move(overlayPosition);
    m_thicknessLegendOverlay->raise();
}

void MainWindow::enterWorkbench(
    robot_qt_viewer::RobotQtViewerWorkbenchKind kind,
    const QString& sourceId)
{
    enterWorkbench(robot_qt_viewer::robotQtViewerWorkbenchId(kind), sourceId);
}

void MainWindow::enterWorkbench(
    const QString& workbenchId,
    const QString& sourceId)
{
    const robot_qt_viewer::RobotQtViewerWorkbenchDesc* workbench =
        m_workbenchPackageRegistry.workbench(workbenchId);
    if(m_workbenchTransitionCoordinator == nullptr ||
        workbench == nullptr || !m_workbenchPackageRegistry.isWorkbenchReady(workbenchId)) {
        statusBar()->showMessage(
            QString("Workbench package is not available: %1")
                .arg(workbench == nullptr ? workbenchId : workbench->descriptor.displayName),
            3000);
        updateWorkbenchActions();
        return;
    }

    const robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult transition =
        m_workbenchTransitionCoordinator->requestTransition(workbenchId, sourceId, this);
    if(!transition.succeeded()) {
        showWorkbenchTransitionResult(
            transition,
            QStringLiteral("Finish or cancel the current task first."));
        updateWorkbenchActions();
        return;
    }

    robot_qt_viewer::RobotQtViewerEvent taskEvent;
    taskEvent.kind = robot_qt_viewer::RobotQtViewerEventKind::TaskStateChanged;
    taskEvent.sourceId = sourceId;
    m_eventHub.publish(taskEvent);
    m_viewportPreviewState.clearTaskPreview(sourceId);

    updateWorkbenchActions();
    if(m_sceneExplorerWorkbenchPort != nullptr) {
        m_sceneExplorerWorkbenchPort->setWorkbenchDescriptor(m_workbenchManager.activeDescriptor());
    }
    if(m_viewport != nullptr) {
        m_viewport->setInteractionMode(toProjectSceneInteractionMode(m_workbenchManager.viewportMode()));
    }
    updateTaskPanel();
    updateRobotRunDetailsDockVisibility();
    robot_qt_viewer::RobotQtViewerEvent event;
    event.kind = robot_qt_viewer::RobotQtViewerEventKind::StatusMessageRequested;
    event.sourceId = sourceId;
    event.message = QString("Workbench: %1").arg(workbench->descriptor.displayName);
    m_eventHub.publish(event);
}

void MainWindow::showWorkbenchTransitionResult(
    const robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult& result,
    const QString& fallbackMessage)
{
    updateWorkbenchActions();
    if(result.succeeded()) {
        if(!result.message.isEmpty()) {
            statusBar()->showMessage(result.message, 3000);
        }
        return;
    }
    const QString message = !result.message.isEmpty()
        ? result.message
        : fallbackMessage;
    statusBar()->showMessage(
        message.isEmpty() ? QStringLiteral("Workbench transition failed.") : message,
        5000);
}

void MainWindow::updateWorkbenchActions()
{
    const robot_qt_viewer::RobotQtViewerWorkbenchKind kind = m_workbenchManager.activeWorkbench();
    const QString activeWorkbenchId = m_workbenchManager.activeWorkbenchId();
    const bool transitionAvailable = m_workbenchTransitionCoordinator == nullptr ||
        (!m_workbenchTransitionCoordinator->transitionInProgress() &&
            m_workbenchTransitionCoordinator->lifecycleState(kind) ==
                robot_qt_viewer::RobotQtViewerWorkbenchLifecycleState::Active);
    const auto modeAvailable = [this, transitionAvailable](
                                   robot_qt_viewer::RobotQtViewerWorkbenchKind candidate) {
        return transitionAvailable &&
            m_workbenchPackageRegistry.isWorkbenchReady(candidate) &&
            (m_workbenchTransitionCoordinator == nullptr ||
                m_workbenchTransitionCoordinator->lifecycleState(candidate) !=
                    robot_qt_viewer::RobotQtViewerWorkbenchLifecycleState::Failed);
    };
    if(m_browseWorkbenchAction != nullptr) {
        m_browseWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse));
        m_browseWorkbenchAction->setChecked(kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse);
    }
    if(m_motionWorkbenchAction != nullptr) {
        m_motionWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::Motion));
        m_motionWorkbenchAction->setChecked(kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::Motion);
    }
    if(m_toolSetupWorkbenchAction != nullptr) {
        m_toolSetupWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup));
        m_toolSetupWorkbenchAction->setChecked(
            kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup);
    }
    if(m_addLinkMountAction != nullptr) {
        m_addLinkMountAction->setEnabled(
            currentSceneExplorerNodeIsLink() &&
            modeAvailable(robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup));
    }
    if(m_addCameraAction != nullptr) {
        m_addCameraAction->setEnabled(
            kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse && transitionAvailable);
    }
    if(m_collisionWorkbenchAction != nullptr) {
        m_collisionWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::Collision));
        m_collisionWorkbenchAction->setChecked(kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::Collision);
    }
    if(m_trajectoryPlanningWorkbenchAction != nullptr) {
        m_trajectoryPlanningWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::TrajectoryPlanning));
        m_trajectoryPlanningWorkbenchAction->setChecked(
            kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::TrajectoryPlanning);
    }
    if(m_sprayProcessWorkbenchAction != nullptr) {
        m_sprayProcessWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::SprayProcess));
        m_sprayProcessWorkbenchAction->setChecked(
            kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::SprayProcess);
    }
    if(m_coatingAnalysisWorkbenchAction != nullptr) {
        m_coatingAnalysisWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::CoatingAnalysis));
        m_coatingAnalysisWorkbenchAction->setChecked(
            kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::CoatingAnalysis);
    }
    if(m_digitalTwinWorkbenchAction != nullptr) {
        m_digitalTwinWorkbenchAction->setEnabled(modeAvailable(
            robot_qt_viewer::RobotQtViewerWorkbenchKind::DigitalTwin));
        m_digitalTwinWorkbenchAction->setChecked(
            kind == robot_qt_viewer::RobotQtViewerWorkbenchKind::DigitalTwin);
    }
    for(auto it = m_dynamicWorkbenchActions.begin();
        it != m_dynamicWorkbenchActions.end(); ++it) {
        QAction* action = it.value();
        if(action == nullptr) {
            continue;
        }
        action->setEnabled(
            transitionAvailable && m_workbenchPackageRegistry.isWorkbenchReady(it.key()) &&
            (m_workbenchTransitionCoordinator == nullptr ||
                m_workbenchTransitionCoordinator->lifecycleState(it.key()) !=
                    robot_qt_viewer::RobotQtViewerWorkbenchLifecycleState::Failed));
        action->setChecked(activeWorkbenchId == it.key());
    }
}

void MainWindow::setRobotRunDetailsRequested(bool requested)
{
    m_robotRunDetailsRequested = requested;
    if(m_robotRunDetailsAction != nullptr && m_robotRunDetailsAction->isChecked() != requested) {
        QSignalBlocker blocker(m_robotRunDetailsAction);
        m_robotRunDetailsAction->setChecked(requested);
    }
    updateRobotRunDetailsDockVisibility();
}

void MainWindow::updateRobotRunDetailsDockVisibility()
{
    if(m_bottomPanelDock == nullptr) {
        return;
    }

    const bool inRobotRunWorkbench =
        m_workbenchManager.activeWorkbench() == robot_qt_viewer::RobotQtViewerWorkbenchKind::Motion;
    const bool visible = m_robotRunDetailsRequested && inRobotRunWorkbench;
    m_bottomPanelDock->setVisible(visible);
    if(m_robotRunDetailsAction != nullptr) {
        QSignalBlocker blocker(m_robotRunDetailsAction);
        m_robotRunDetailsAction->setEnabled(inRobotRunWorkbench);
        m_robotRunDetailsAction->setChecked(visible);
    }
}

void MainWindow::updateTaskPanel()
{
    if(m_taskPanelStack == nullptr) {
        return;
    }

    const QString activeWorkbenchId = m_workbenchManager.activeWorkbenchId();
    const robot_qt_viewer::RobotQtViewerWorkbenchDescriptor& descriptor =
        m_workbenchManager.activeDescriptor();
    QWidget* panel = m_workbenchContributionHost != nullptr
        ? m_workbenchContributionHost->panelForWorkbench(activeWorkbenchId)
        : nullptr;
    if(panel == nullptr) {
        panel = m_statusPanelWidget;
    }
    if(panel != nullptr) {
        m_taskPanelStack->setCurrentWidget(panel);
    }
    if(m_taskPanelDock != nullptr) {
        m_taskPanelDock->setWindowTitle(
            m_workbenchContributionHost != nullptr
                ? m_workbenchContributionHost->panelTitleForWorkbench(
                    activeWorkbenchId, descriptor.rightPanelTitle)
                : descriptor.rightPanelTitle);
    }
}

void MainWindow::newProject()
{
    if(!confirmProjectReplacement(
        this,
        QStringLiteral("New project"),
        [this](bool) {
            if(m_workbenchTransitionCoordinator == nullptr) {
                return true;
            }
            const auto result = m_workbenchTransitionCoordinator->prepareActiveDeactivation(
                robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
                QStringLiteral("newProject"),
                this);
            showWorkbenchTransitionResult(result);
            return result.succeeded();
        },
        [this]() {
            saveProject();
        },
        [this]() {
            return m_projectSession.isDirty();
        })) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
        }
        statusBar()->showMessage(QStringLiteral("Project replacement canceled."), 3000);
        return;
    }

    if(m_workbenchTransitionCoordinator != nullptr) {
        const auto deactivate = m_workbenchTransitionCoordinator->deactivateActive(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
            QStringLiteral("newProject"),
            this,
            true);
        if(!deactivate.succeeded()) {
            showWorkbenchTransitionResult(deactivate);
            return;
        }
    }

    const robot_qt_viewer::ProjectSessionWorkflowResult result =
        m_appController.resetProject(QStringLiteral("newProject"));
    if(!result.success) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            showWorkbenchTransitionResult(
                m_workbenchTransitionCoordinator->activateCommittedWorkbench(
                    robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::Rollback,
                    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::Rollback,
                    QStringLiteral("newProjectRollback")));
        }
        statusBar()->showMessage(result.message, 5000);
        return;
    }
    if(m_workbenchTransitionCoordinator != nullptr) {
        m_workbenchTransitionCoordinator->releaseProject(QStringLiteral("newProject"));
    }
    if(result.shouldReloadViewport) {
        reloadViewportProject();
    }
    if(m_workbenchTransitionCoordinator != nullptr) {
        showWorkbenchTransitionResult(
            m_workbenchTransitionCoordinator->activateCommittedWorkbench(
                robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::FreshProject,
                robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectLoaded,
                QStringLiteral("newProject")));
    }
    statusBar()->showMessage(result.message, 3000);
}

void MainWindow::openProject()
{
    const QString fileName = robot_qt_viewer::getOpenFileName(
        QStringLiteral("shell.project.open"),
        this,
        "Open project",
        defaultProjectDialogPath(m_projectPath),
        "System Project (*.sys.json);;Scene Project (*.scene.v3.json *.scene.json);;JSON Files (*.json);;All Files (*.*)");

    if(fileName.isEmpty()) {
        return;
    }

    if(!confirmProjectReplacement(
        this,
        QStringLiteral("Open project"),
        [this](bool) {
            if(m_workbenchTransitionCoordinator == nullptr) {
                return true;
            }
            const auto result = m_workbenchTransitionCoordinator->prepareActiveDeactivation(
                robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
                QStringLiteral("openProject"),
                this);
            showWorkbenchTransitionResult(result);
            return result.succeeded();
        },
        [this]() {
            saveProject();
        },
        [this]() {
            return m_projectSession.isDirty();
        })) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            m_workbenchTransitionCoordinator->cancelPreparedDeactivation();
        }
        statusBar()->showMessage(QStringLiteral("Open project canceled."), 3000);
        return;
    }

    if(m_workbenchTransitionCoordinator != nullptr) {
        const auto deactivate = m_workbenchTransitionCoordinator->deactivateActive(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
            QStringLiteral("openProject"),
            this,
            true);
        if(!deactivate.succeeded()) {
            showWorkbenchTransitionResult(deactivate);
            return;
        }
    }

    const std::filesystem::path path = fileName.toStdWString();
    const QString operationId = beginProjectOpenOperation(path, QStringLiteral("openProject"));
    const robot_qt_viewer::ProjectSessionWorkflowResult result =
        m_appController.loadProject(path, QStringLiteral("openProject"), operationId);
    if(!result.success) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            showWorkbenchTransitionResult(
                m_workbenchTransitionCoordinator->activateCommittedWorkbench(
                    robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::Rollback,
                    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::Rollback,
                    QStringLiteral("openProjectRollback")));
        }
        refreshOperationStatusPresentation(operationId);
        return;
    }

    if(m_workbenchTransitionCoordinator != nullptr) {
        m_workbenchTransitionCoordinator->releaseProject(QStringLiteral("openProject"));
    }

    if(result.shouldReloadViewport) {
        reloadViewportProject(operationId);
    }
    if(m_workbenchTransitionCoordinator != nullptr) {
        showWorkbenchTransitionResult(
            m_workbenchTransitionCoordinator->activateCommittedWorkbench(
                robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::FreshProject,
                robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectLoaded,
                QStringLiteral("openProject")));
    }
    refreshOperationStatusPresentation(operationId);
}

bool MainWindow::openProjectPathForProfiling(
    const std::filesystem::path& path,
    int exitDelayMs,
    int repeatCount,
    bool enableCollisionAfterLoad,
    const QString& profileGenerateObjectCoacdId,
    bool setGeneratedCollisionCurrent,
    const std::filesystem::path& profileSavePath,
    const QString& profileEnvironmentPresetId,
    const std::filesystem::path& profileScreenshotPath)
{
    const int remainingRepeats = repeatCount > 1 ? repeatCount : 1;
    std::cout << "\n"
        << "+------------------------------------------------------------------------------+\n"
        << "| RobotQtViewer Project Load Profile                                           |\n"
        << "+------------------------------------------------------------------------------+\n"
        << "| project: " << path.generic_u8string() << "\n"
        << "| repeatRemaining: " << remainingRepeats << "\n"
        << "+------------------------------------------------------------------------------+\n";

    if(m_workbenchTransitionCoordinator != nullptr) {
        const auto deactivate = m_workbenchTransitionCoordinator->deactivateActive(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectReplacing,
            QStringLiteral("profileProject"),
            this);
        if(!deactivate.succeeded()) {
            showWorkbenchTransitionResult(deactivate);
            return false;
        }
    }

    const QString operationId = beginProjectOpenOperation(path, QStringLiteral("profileProject"));
    const robot_qt_viewer::ProjectSessionWorkflowResult result =
        m_appController.loadProject(path, QStringLiteral("profileProject"), operationId);
    if(!result.success) {
        if(m_workbenchTransitionCoordinator != nullptr) {
            showWorkbenchTransitionResult(
                m_workbenchTransitionCoordinator->activateCommittedWorkbench(
                    robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::Rollback,
                    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::Rollback,
                    QStringLiteral("profileProjectRollback")));
        }
        std::cout << "| result : FAILED - " << result.message.toStdString() << "\n"
            << "+------------------------------------------------------------------------------+\n";
        refreshOperationStatusPresentation(operationId);
        return false;
    }

    if(m_workbenchTransitionCoordinator != nullptr) {
        m_workbenchTransitionCoordinator->releaseProject(QStringLiteral("profileProject"));
    }

    if(result.shouldReloadViewport) {
        reloadViewportProject(operationId);
    }
    if(m_workbenchTransitionCoordinator != nullptr) {
        const auto activate = m_workbenchTransitionCoordinator->activateCommittedWorkbench(
            robot_qt_viewer::RobotQtViewerWorkbenchActivationKind::FreshProject,
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::ProjectLoaded,
            QStringLiteral("profileProject"));
        if(!activate.succeeded()) {
            showWorkbenchTransitionResult(activate);
            return false;
        }
    }
    refreshOperationStatusPresentation(operationId);
    if(!profileGenerateObjectCoacdId.isEmpty()) {
        const auto coacdStart = std::chrono::steady_clock::now();
        const QString guiAttachmentPrefix = QStringLiteral("gui-attachment:");
        const bool useGuiAttachmentPath =
            profileGenerateObjectCoacdId.startsWith(guiAttachmentPrefix);
        bool generated = false;
        std::size_t generatedPartCount = 0;
        QString profileTarget = profileGenerateObjectCoacdId;
        if(useGuiAttachmentPath) {
            profileTarget = profileGenerateObjectCoacdId.mid(guiAttachmentPrefix.size());
            if(m_toolSetupWorkbenchPort != nullptr) {
                m_toolSetupWorkbenchPort->selectToolAttachmentById(profileTarget.toStdString());
            }
            if(m_collisionWorkbenchServices != nullptr) {
                std::cout << "| Profile GUI selection        | attachment="
                    << m_collisionWorkbenchServices->selectedToolAttachmentId().toStdString()
                    << " robot=" << m_collisionWorkbenchServices->selectedRobotId().toStdString()
                    << " link=" << m_collisionWorkbenchServices->selectedLinkName().toStdString()
                    << "\n";
            }
            if(m_collisionWorkbenchPort != nullptr) {
                m_collisionWorkbenchPort->showCollisionModelConfiguration();
                m_collisionWorkbenchPort->generateCollisionCoacd();
            }
            const QString statusMessage = statusBar()->currentMessage();
            generated = statusMessage.startsWith(QStringLiteral("Generated "));
            bool generatedPartCountOk = false;
            const int parsedGeneratedPartCount =
                statusMessage.section(QLatin1Char(' '), 1, 1).toInt(&generatedPartCountOk);
            if(generatedPartCountOk && parsedGeneratedPartCount >= 0) {
                generatedPartCount = static_cast<std::size_t>(parsedGeneratedPartCount);
            }
            std::cout << "| Profile GUI COACD status     | "
                << statusMessage.toStdString() << "\n";
            if(generated && setGeneratedCollisionCurrent &&
                m_collisionWorkbenchPort != nullptr) {
                m_collisionWorkbenchPort->setSelectedCollisionVariantCurrent();
                std::cout << "| Profile GUI Set Current      | "
                    << statusBar()->currentMessage().toStdString()
                    << " dirty=" << (m_projectSession.isDirty() ? "true" : "false")
                    << "\n";
            }
        } else {
            std::vector<simulation_project::ObjectCollisionElementOverrideDesc> elements;
            generated =
                m_collisionViewport != nullptr &&
                m_collisionViewport->generateObjectCollisionCoacdFromVisual(
                    profileTarget,
                    elements);
            generatedPartCount = elements.size();
        }
        const auto coacdMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - coacdStart).count();
        std::cout << "| Profile object COACD         | "
            << std::right << std::setw(10) << coacdMs
            << " ms | object=" << profileTarget.toStdString()
            << " ok=" << (generated ? "true" : "false")
            << " parts=" << generatedPartCount
            << "\n";
    }
    if(!profileSavePath.empty()) {
        const robot_qt_viewer::ProjectSessionWorkflowResult saveResult =
            m_appController.saveProject(
                profileSavePath,
                false,
                QStringLiteral("profileSaveProject"));
        std::cout << "| Profile project save         | ok="
            << (saveResult.success ? "true" : "false")
            << " dirty=" << (m_projectSession.isDirty() ? "true" : "false")
            << " path=" << profileSavePath.generic_u8string()
            << " detail=" << saveResult.message.toStdString()
            << "\n";
    }
    if(enableCollisionAfterLoad) {
        const auto collisionStart = std::chrono::steady_clock::now();
        const bool changed = m_viewport->setCollisionQueriesEnabled(true);
        const auto collisionMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - collisionStart).count();
        std::cout << "| Profile enable collision       | "
            << std::right << std::setw(10) << collisionMs
            << " ms | changed=" << (changed ? "true" : "false") << "\n";
        if(m_collisionQueriesAction != nullptr) {
            QSignalBlocker blocker(m_collisionQueriesAction);
            m_collisionQueriesAction->setChecked(true);
        }
    }
    if(!profileEnvironmentPresetId.isEmpty()) {
        ProjectSceneEnvironmentPreset preset = ProjectSceneEnvironmentPreset::Factory;
        if(!parseProjectSceneEnvironmentPreset(
               profileEnvironmentPresetId.toStdString(), preset)) {
            std::cout << "| Profile environment          | FAILED - unknown preset: "
                << profileEnvironmentPresetId.toStdString() << "\n";
            return false;
        }
        m_viewport->setEnvironmentPreset(preset);
        const auto action = m_environmentActions.constFind(profileEnvironmentPresetId.toLower());
        if(action != m_environmentActions.constEnd()) {
            QSignalBlocker blocker(*action);
            (*action)->setChecked(true);
        }
        std::cout << "| Profile environment          | "
            << projectSceneEnvironmentPresetId(preset) << "\n";
    }
    statusBar()->showMessage(result.message, 5000);

    if(remainingRepeats > 1) {
        QTimer::singleShot(
            0,
            this,
            [this, path, exitDelayMs, remainingRepeats, enableCollisionAfterLoad,
             profileGenerateObjectCoacdId, setGeneratedCollisionCurrent, profileSavePath,
             profileEnvironmentPresetId, profileScreenshotPath]() {
            openProjectPathForProfiling(
                path,
                exitDelayMs,
                remainingRepeats - 1,
                enableCollisionAfterLoad,
                profileGenerateObjectCoacdId,
                setGeneratedCollisionCurrent,
                profileSavePath,
                profileEnvironmentPresetId,
                profileScreenshotPath);
        });
    } else if(!profileScreenshotPath.empty()) {
        QTimer::singleShot(std::max(exitDelayMs, 0), this, [this, profileScreenshotPath]() {
            std::error_code error;
            if(!profileScreenshotPath.parent_path().empty()) {
                std::filesystem::create_directories(profileScreenshotPath.parent_path(), error);
            }
            const QImage image = m_viewport->grabFramebuffer();
            const QString outputPath = QString::fromUtf8(
                profileScreenshotPath.generic_u8string().c_str());
            const bool saved = !error && !image.isNull() && image.save(outputPath, "PNG");
            std::cout << "| Profile viewport screenshot  | ok="
                << (saved ? "true" : "false")
                << " size=" << image.width() << "x" << image.height()
                << " path=" << profileScreenshotPath.generic_u8string() << "\n";
            qApp->exit(saved ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    } else if(exitDelayMs >= 0) {
        QTimer::singleShot(exitDelayMs, qApp, &QCoreApplication::quit);
    }
    return true;
}

void MainWindow::saveProject()
{
    if(m_projectSession.requiresSaveAs() || m_projectPath.empty()) {
        saveProjectAs();
        return;
    }
    saveProjectToPath(m_projectPath);
}

void MainWindow::saveProjectAs()
{
    const QString fileName = robot_qt_viewer::getSaveFileName(
        QStringLiteral("shell.project.save"),
        this,
        "Save project",
        m_projectPath.empty() ? defaultProjectDialogPath(m_projectPath) : QString::fromStdWString(m_projectPath.wstring()),
        "System Project (*.sys.json);;Scene Project (*.scene.v3.json *.scene.json);;JSON Files (*.json);;All Files (*.*)");

    if(fileName.isEmpty()) {
        return;
    }

    saveProjectToPath(std::filesystem::path(fileName.toStdWString()));
}

void MainWindow::saveProjectAsV3()
{
    const QString fileName = robot_qt_viewer::getSaveFileName(
        QStringLiteral("shell.project.saveSystem"),
        this,
        "Save project as system",
        m_projectPath.empty() ? defaultProjectDialogPath(m_projectPath) : QString::fromStdWString(m_projectPath.wstring()),
        "System Project (*.sys.json);;v3 Scene Project (*.scene.v3.json);;JSON Files (*.json);;All Files (*.*)");

    if(fileName.isEmpty()) {
        return;
    }

    saveProjectToPath(std::filesystem::path(fileName.toStdWString()), true);
}

void MainWindow::importRobotPackage()
{
    const QString fileName = robot_qt_viewer::ProjectAssemblyDialogService::selectRobotPackageForImport(
        this,
        defaultProjectDialogPath(m_projectPath));

    if(fileName.isEmpty()) {
        return;
    }

    std::unordered_set<std::string> existingRobotIds;
    for(const simulation_project::RobotDesc& robot : m_appController.document().robots) {
        existingRobotIds.insert(robot.id);
    }

    const std::filesystem::path packagePath(fileName.toStdWString());
    const robot_qt_viewer::ProjectMutationResult mutationResult = m_documentController.mutateProject(
        QStringLiteral("importRobotPackage"),
        robot_qt_viewer::ProjectDirtyPolicy::UserEdit,
        [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& errorMessage) {
            if(!simulation_project::importRobotPackageDocument(
                   packagePath,
                   m_projectPath,
                   service.document(),
                   &errorMessage)) {
                return false;
            }
            changed = true;
            return true;
        });
    if(!mutationResult.success) {
        statusBar()->showMessage(QString("Import robot package failed: %1").arg(mutationResult.message), 8000);
        return;
    }

    QString importedRobotId;
    for(const simulation_project::RobotDesc& robot : m_appController.document().robots) {
        if(existingRobotIds.find(robot.id) == existingRobotIds.end()) {
            importedRobotId = QString::fromStdString(robot.id);
            break;
        }
    }

    reloadViewportProject();
    if(!importedRobotId.isEmpty()) {
        selectRobotContext(importedRobotId);
    }
    statusBar()->showMessage(QString("Imported robot package: %1").arg(fileName), 5000);
}

void MainWindow::exportSelectedRobotPackage()
{
    const QString robotId = m_appController.selectedRobotId();
    if(robotId.isEmpty()) {
        statusBar()->showMessage("Select a robot before exporting a robot package.", 4000);
        return;
    }

    std::filesystem::path suggestedPath;
    if(!m_projectPath.empty()) {
        suggestedPath = m_projectPath.parent_path() / (robotId.toStdString() + ".rbt.json");
    }

    const QString fileName = robot_qt_viewer::ProjectAssemblyDialogService::selectRobotPackageForExport(
        this,
        suggestedPath.empty() ? defaultProjectDialogPath(m_projectPath) : QString::fromStdWString(suggestedPath.wstring()));

    if(fileName.isEmpty()) {
        return;
    }

    std::string errorMessage;
    if(!simulation_project::saveRobotPackageDocument(
           std::filesystem::path(fileName.toStdWString()),
           m_projectPath,
           m_appController.document(),
           robotId.toStdString(),
           &errorMessage)) {
        statusBar()->showMessage(QString("Export robot package failed: %1").arg(QString::fromStdString(errorMessage)), 8000);
        return;
    }

    statusBar()->showMessage(QString("Exported robot package: %1").arg(fileName), 5000);
}

bool MainWindow::saveProjectToPath(const std::filesystem::path& path, bool saveAsV3)
{
    const robot_qt_viewer::ProjectSessionWorkflowResult result =
        m_appController.saveProject(path, saveAsV3, QStringLiteral("saveProject"));
    if(!result.success) {
        statusBar()->showMessage(result.message, 5000);
        return false;
    }
    statusBar()->showMessage(result.message, 3000);
    return true;
}

void MainWindow::importRobot()
{
    const QString fileName = robot_qt_viewer::ProjectAssemblyDialogService::selectRobotForImport(this);

    if(fileName.isEmpty()) {
        return;
    }

    const std::filesystem::path path = fileName.toStdWString();
    const robot_qt_viewer::SceneEntityImportResult importResult =
        m_appController.sceneEntityWorkflow().importRobotFromPath(path, sourceTypeFromPath(fileName));
    if(!importResult.success) {
        statusBar()->showMessage(importResult.message, 8000);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Importing %1...").arg(fileName));
    QApplication::processEvents();
    bool loaded = false;
    QString importError;
    try {
        loaded = reloadViewportProject();
    } catch(const std::exception& e) {
        importError = QString::fromLocal8Bit(e.what());
        statusBar()->showMessage(QString("Import failed: %1").arg(importError), 8000);
        LOG_ERROR("rs2026") << "Import robot failed: " << e.what();
    } catch(...) {
        importError = "unknown error";
        statusBar()->showMessage("Import failed: unknown error", 8000);
        LOG_ERROR("rs2026") << "Import robot failed: unknown error";
    }
    QApplication::restoreOverrideCursor();
    if(!loaded) {
        if(importError.isEmpty() && m_viewport != nullptr && !m_viewport->lastError().isEmpty()) {
            importError = m_viewport->lastError();
        }
        m_appController.sceneEntityWorkflow().restoreImportState(importResult);
        reloadViewportProject();
        const QString error = !importError.isEmpty()
            ? importError
            : QString("Failed to rebuild viewport scene.");
        statusBar()->showMessage(QString("Import failed: %1").arg(error), 8000);
        return;
    }
    m_documentController.publishDocumentChanged(QStringLiteral("importRobot"), false);
    statusBar()->showMessage(QString("Imported %1").arg(fileName), 5000);
}

void MainWindow::importObject()
{
    const QString fileName = robot_qt_viewer::ProjectAssemblyDialogService::selectObjectForImport(this);

    if(fileName.isEmpty()) {
        return;
    }

    const std::filesystem::path path = fileName.toStdWString();
    const robot_qt_viewer::SceneEntityImportResult importResult =
        m_appController.sceneEntityWorkflow().importSceneObjectFromPath(path, "workpiece");
    if(!importResult.success) {
        statusBar()->showMessage(importResult.message, 8000);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Importing object %1...").arg(fileName));
    QApplication::processEvents();
    bool loaded = false;
    QString importError;
    try {
        loaded = reloadViewportProject();
    } catch(const std::exception& e) {
        importError = QString::fromLocal8Bit(e.what());
        statusBar()->showMessage(QString("Import object failed: %1").arg(importError), 8000);
        LOG_ERROR("rs2026") << "Import object failed: " << e.what();
    } catch(...) {
        importError = "unknown error";
        statusBar()->showMessage("Import object failed: unknown error", 8000);
        LOG_ERROR("rs2026") << "Import object failed: unknown error";
    }
    QApplication::restoreOverrideCursor();
    if(!loaded) {
        if(importError.isEmpty() && m_viewport != nullptr && !m_viewport->lastError().isEmpty()) {
            importError = m_viewport->lastError();
        }
        m_appController.sceneEntityWorkflow().restoreImportState(importResult);
        reloadViewportProject();
        const QString error = !importError.isEmpty()
            ? importError
            : QString("Failed to rebuild viewport scene.");
        statusBar()->showMessage(QString("Import object failed: %1").arg(error), 8000);
        return;
    }
    m_documentController.publishDocumentChanged(QStringLiteral("importObject"), false);
    statusBar()->showMessage(QString("Imported object %1 from %2").arg(
        importResult.entityId,
        importResult.storedPath), 5000);
}

void MainWindow::addCameraDefinition()
{
    if(m_sceneExplorerWorkbenchPort == nullptr) {
        statusBar()->showMessage(QStringLiteral("Project Assembly is not available."), 3000);
        return;
    }
    m_sceneExplorerWorkbenchPort->createCameraDefinition(this);
}

void MainWindow::importPointCloud()
{
    const QString fileName = robot_qt_viewer::ProjectAssemblyDialogService::selectPointCloudForImport(this);

    if(fileName.isEmpty()) {
        return;
    }

    const std::filesystem::path path = fileName.toStdWString();
    const robot_qt_viewer::SceneEntityImportResult importResult =
        m_appController.sceneEntityWorkflow().importPointCloudFromPath(path, "pcd");
    if(!importResult.success) {
        statusBar()->showMessage(importResult.message, 8000);
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QString("Importing point cloud %1...").arg(fileName));
    QApplication::processEvents();
    bool loaded = false;
    QString importError;
    try {
        loaded = reloadViewportProject();
    } catch(const std::exception& e) {
        importError = QString::fromLocal8Bit(e.what());
        statusBar()->showMessage(QString("Import point cloud failed: %1").arg(importError), 8000);
        LOG_ERROR("rs2026") << "Import point cloud failed: " << e.what();
    } catch(...) {
        importError = "unknown error";
        statusBar()->showMessage("Import point cloud failed: unknown error", 8000);
        LOG_ERROR("rs2026") << "Import point cloud failed: unknown error";
    }
    QApplication::restoreOverrideCursor();
    if(!loaded) {
        if(importError.isEmpty() && m_viewport != nullptr && !m_viewport->lastError().isEmpty()) {
            importError = m_viewport->lastError();
        }
        m_appController.sceneEntityWorkflow().restoreImportState(importResult);
        reloadViewportProject();
        const QString error = !importError.isEmpty()
            ? importError
            : QString("Failed to rebuild viewport scene.");
        statusBar()->showMessage(QString("Import point cloud failed: %1").arg(error), 8000);
        return;
    }
    m_documentController.publishDocumentChanged(QStringLiteral("importPointCloud"), false);
    statusBar()->showMessage(QString("Imported point cloud %1 from %2").arg(
        importResult.entityId,
        importResult.storedPath), 5000);
}

void MainWindow::deleteSelectedRobot()
{
    if(m_sceneExplorerWorkbenchPort == nullptr) {
        statusBar()->showMessage("No robot selected.", 3000);
        return;
    }

    const robot_qt_viewer::SceneExplorerNodeRef node = m_sceneExplorerWorkbenchPort->currentNode();
    if(node.id.isEmpty()) {
        statusBar()->showMessage("No scene item selected.", 3000);
        return;
    }

    robot_qt_viewer::SceneEntityKind entityKind = robot_qt_viewer::SceneEntityKind::Robot;
    if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::Robot) {
        entityKind = robot_qt_viewer::SceneEntityKind::Robot;
    } else if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::Object) {
        entityKind = robot_qt_viewer::SceneEntityKind::Object;
    } else if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::PointCloud) {
        entityKind = robot_qt_viewer::SceneEntityKind::PointCloud;
    } else {
        statusBar()->showMessage("Select a robot, object, or point cloud to delete.", 3000);
        return;
    }

    if(!robot_qt_viewer::ProjectAssemblyDialogService::confirmDelete(this, node.kind, node.id)) {
        return;
    }

    const robot_qt_viewer::SceneEntityDeleteResult deleteResult =
        m_appController.sceneEntityWorkflow().deleteEntity(entityKind, node.id);
    if(!deleteResult.success) {
        statusBar()->showMessage(deleteResult.message, 3000);
        return;
    }

    if(entityKind == robot_qt_viewer::SceneEntityKind::Robot) {
        reloadViewportProject();
    } else if(m_assemblyViewport != nullptr && m_collisionViewport != nullptr) {
        if(m_assemblyViewport->removeSceneObject(node.id)) {
            m_collisionViewport->rebuildCollisionDetectorsFromDocument(m_appController.document());
        }
    }
    statusBar()->showMessage(deleteResult.message, 4000);
}

void MainWindow::renamePointCloud(
    const robot_qt_viewer::SceneTreeIntentController::ContextMenuAction& action)
{
    if(action.node.id.isEmpty()) {
        statusBar()->showMessage("No point cloud selected.", 3000);
        return;
    }

    bool ok = false;
    const QString currentName = action.displayName.isEmpty()
        ? action.node.name
        : action.displayName;
    const QString newName = robot_qt_viewer::ProjectAssemblyDialogService::requestPointCloudName(
        this,
        currentName,
        &ok);
    if(!ok) {
        return;
    }

    const robot_qt_viewer::SceneEntityMutationResult result =
        m_appController.sceneEntityWorkflow().renamePointCloud(action.node.id, newName);
    if(!result.success) {
        statusBar()->showMessage(result.message, 5000);
        return;
    }

    statusBar()->showMessage(result.message, 3000);
}

void MainWindow::handleSceneTreeContextMenuAction(
    const robot_qt_viewer::SceneTreeIntentController::ContextMenuAction& action)
{
    if(m_sceneExplorerWorkbenchPort != nullptr &&
        !m_sceneExplorerWorkbenchPort->resolvePendingTransformPreviewIfTargetChanges(action.node, this)) {
        refreshSceneExplorerViewModel();
        return;
    }

    const bool sameEditedMount =
        action.node.kind == robot_qt_viewer::SceneExplorerNodeKind::RobotMount &&
        m_toolSetupWorkbenchPort != nullptr &&
        action.node.id == m_toolSetupWorkbenchPort->currentMountId();
    const bool switchingFromFrameEditor =
        m_workbenchManager.activeWorkbench() == robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup &&
        !sameEditedMount;
    if(switchingFromFrameEditor &&
        !prepareEditSessionTransition(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::TaskHandoff,
            QStringLiteral("sceneExplorerContextAction"),
            false)) {
        refreshSceneExplorerViewModel();
        return;
    }
    if(switchingFromFrameEditor && action.node.kind != robot_qt_viewer::SceneExplorerNodeKind::RobotMount) {
        enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse, QStringLiteral("sceneExplorerSelection"));
    }

    if(action.kind == robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::RenamePointCloud) {
        renamePointCloud(action);
        return;
    }
    if(action.kind == robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::ShowLinkFrame) {
        const bool visible =
            m_sceneExplorerWorkbenchPort != nullptr
                ? m_sceneExplorerWorkbenchPort->toggleLinkFrameVisible(action.node.id, action.node.linkName)
                : false;
        selectRobotContext(action.node.id, action.node.linkName);
        robot_qt_viewer::RobotQtViewerViewportPreviewPayload preview;
        preview.setRobotMountFrameVisibility = true;
        preview.selectedLinkFrameVisible = visible;
        preview.mountFrameVisible = false;
        m_viewportPreviewState.mutate(preview, QStringLiteral("sceneExplorerShowLinkFrame"));
        statusBar()->showMessage(
            QString("%1 link frame: %2.%3")
                .arg(visible ? QStringLiteral("Showing") : QStringLiteral("Hiding"),
                    action.node.id,
                    action.node.linkName),
            3000);
        return;
    }
    if(m_sceneExplorerActionRouter == nullptr) {
        return;
    }
    m_sceneExplorerActionRouter->handleAction(
        action,
        [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        });
    switch(action.kind) {
    case robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::ConfigureRobotFlange:
    case robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::AddObjectFrame:
    case robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::EditObjectFrame:
    case robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::BindItemToMount:
    case robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::BindObjectToMount:
        updateTaskPanel();
        if(m_taskPanelDock != nullptr) {
            m_taskPanelDock->show();
            m_taskPanelDock->raise();
        }
        break;
    default:
        break;
    }
    if(action.kind ==
            robot_qt_viewer::SceneTreeIntentController::ContextMenuActionKind::ConfigureCollisionModel &&
        robot_qt_viewer::sceneExplorerNodeKindCanConfigureCollisionModel(action.node.kind)) {
        rememberCollisionModelConfigurationTarget(action.node);
    }
}

void MainWindow::createPanels()
{
    QDockWidget* resultDock = new QDockWidget("Scene Edit Panel", this);
    m_taskPanelDock = resultDock;
    QWidget* resultPanel = new QWidget(resultDock);
    resultPanel->setMinimumWidth(0);
    QVBoxLayout* resultLayout = new QVBoxLayout(resultPanel);
    resultLayout->setContentsMargins(12, 10, 12, 10);
    resultLayout->setSpacing(8);

    m_taskPanelStack = new QStackedWidget(resultPanel);
    m_taskPanelStack->setMinimumWidth(0);
    makeHorizontallyCompressible(m_taskPanelStack);

    m_statusPanelWidget = new StatusPanelWidget(m_taskPanelStack);
    m_documentViewRegistry.registerModule(QStringLiteral("status"), m_statusPanelWidget,
        [this](const robot_qt_viewer::RobotQtViewerEvent& event) {
            if(event.kind == robot_qt_viewer::RobotQtViewerEventKind::OperationStatusChanged) {
                refreshOperationStatusPresentation(event.operationStatus.operationId);
            } else if(event.kind == robot_qt_viewer::RobotQtViewerEventKind::StatusMessageRequested) {
                statusBar()->showMessage(event.message, event.timeoutMs > 0 ? event.timeoutMs : 2500);
            }
        });
    refreshOperationStatusPresentation();

    m_taskPanelStack->addWidget(m_statusPanelWidget);
    for(QPushButton* button : resultPanel->findChildren<QPushButton*>()) {
        configureInspectorButton(button);
    }
    for(QAbstractSpinBox* spin : resultPanel->findChildren<QAbstractSpinBox*>()) {
        makeHorizontallyCompressible(spin);
    }

    resultLayout->addWidget(m_taskPanelStack, 1);
    resultPanel->setLayout(resultLayout);
    resultDock->setWidget(resultPanel);
    resultDock->setMinimumWidth(300);
    addDockWidget(Qt::RightDockWidgetArea, resultDock);
    m_windowMenu->addAction(m_taskPanelDock->toggleViewAction());
    if(m_workbenchPackageRegistry.hasWorkbench(
           robot_qt_viewer::RobotQtViewerWorkbenchKind::Motion)) {
        m_windowMenu->addAction(m_robotRunDetailsAction);
    }
    applyInitialPanelLayout();
    QTimer::singleShot(0, this, [this]() {
        applyInitialPanelLayout();
    });
    m_documentController.publishDocumentChanged(QStringLiteral("createPanelsInitialView"), false);
    updateWorkbenchActions();
    updateTaskPanel();
    updateRobotRunDetailsDockVisibility();
}

void MainWindow::initializeWorkbenchContributions()
{
    using Kind = robot_qt_viewer::RobotQtViewerWorkbenchKind;
    m_workbenchContributionHost =
        std::make_unique<robot_qt_viewer::RobotQtViewerWorkbenchContributionHost>(
            m_workbenchPackageRegistry,
            m_editSessionCoordinator);

    QString registrationError;
    auto registerFactory = [this, &registrationError](
                               const robot_qt_viewer::
                                   RobotQtViewerWorkbenchRuntimeContributionFactoryDesc& factory) {
        if(registrationError.isEmpty() &&
            !m_workbenchContributionHost->registerFactory(factory, &registrationError)) {
            return false;
        }
        return registrationError.isEmpty();
    };
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::Browse)) {
        robot_qt_viewer::SceneExplorerWorkbenchComposition composition;
        composition.documentContext = &m_documentContext;
        composition.documentViewRegistry = &m_documentViewRegistry;
        composition.sceneEntityWorkflow = &m_appController.sceneEntityWorkflow();
        composition.assemblyViewport = m_assemblyViewport.get();
        composition.mainWindow = this;
        composition.statusPanel = m_statusPanelWidget;
        composition.dockTitle = QStringLiteral("Scene Explorer");
        composition.activeWorkbenchDescriptor = [this]() {
            return m_workbenchManager.activeDescriptor();
        };
        composition.nodeActivated = [this](
                                        const robot_qt_viewer::SceneExplorerNodeRef& node,
                                        int column) {
            handleSceneExplorerNodeActivated(node, column);
        };
        composition.nodeDoubleActivated = [this](
                                              const robot_qt_viewer::SceneExplorerNodeRef& node,
                                              int) {
            if(m_sceneExplorerWorkbenchPort == nullptr) {
                return;
            }
            if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::RobotMount) {
                robot_qt_viewer::SceneTreeIntentController::ContextMenuAction action;
                action.kind = robot_qt_viewer::SceneTreeIntentController::
                    ContextMenuActionKind::ConfigureRobotFlange;
                action.node = node;
                handleSceneTreeContextMenuAction(action);
                return;
            }
            if(node.kind != robot_qt_viewer::SceneExplorerNodeKind::Object &&
                node.kind != robot_qt_viewer::SceneExplorerNodeKind::ObjectFrame) {
                return;
            }
            if(m_workbenchManager.activeWorkbench() == Kind::ToolSetup &&
                !prepareEditSessionTransition(
                    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::TaskHandoff,
                    QStringLiteral("sceneExplorerTransformDoubleClick"),
                    false)) {
                refreshSceneExplorerViewModel();
                return;
            }
            if(m_workbenchManager.activeWorkbench() == Kind::ToolSetup) {
                enterWorkbench(
                    Kind::Browse,
                    QStringLiteral("sceneExplorerTransformDoubleClick"));
            }
            if(!m_sceneExplorerWorkbenchPort->resolvePendingTransformPreviewIfTargetChanges(
                   node,
                   this)) {
                refreshSceneExplorerViewModel();
                return;
            }
            const robot_qt_viewer::SceneSelectionIntent intent =
                m_sceneExplorerWorkbenchPort->selectionIntentForNode(node);
            if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::Object) {
                if(intent.kind != robot_qt_viewer::SceneSelectionIntentKind::SelectSceneObject) {
                    return;
                }
                m_selectionModel.selectSceneObject(
                    intent.itemId,
                    QStringLiteral("sceneExplorerObjectEdit"));
                const robot_qt_viewer::RobotQtViewerSelectionPayload committed =
                    m_selectionModel.payload();
                if(committed.objectId != intent.itemId ||
                    !committed.objectFrameId.isEmpty()) {
                    refreshSceneExplorerViewModel();
                    return;
                }
            } else {
                if(intent.kind != robot_qt_viewer::SceneSelectionIntentKind::SelectObjectFrame) {
                    return;
                }
                m_selectionModel.selectObjectFrame(
                    intent.itemId,
                    intent.linkName,
                    QStringLiteral("sceneExplorerObjectFrameEdit"));
                const robot_qt_viewer::RobotQtViewerSelectionPayload committed =
                    m_selectionModel.payload();
                if(committed.objectId != intent.itemId ||
                    committed.objectFrameId != intent.linkName) {
                    refreshSceneExplorerViewModel();
                    return;
                }
            }
            m_appController.setObjectInspectorContext(intent.itemId);
            m_sceneExplorerWorkbenchPort->focusTransformTask(node);
            statusBar()->showMessage(intent.statusMessage, 3000);
            updateTaskPanel();
        };
        composition.contextActionRequested = [this](
                                                 const robot_qt_viewer::
                                                     SceneTreeIntentController::ContextMenuAction& action) {
            handleSceneTreeContextMenuAction(action);
        };
        composition.showStatus = [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        };
        composition.selectionChanged = [this]() {
            refreshSelectedLinkMaterialSummary();
        };
        composition.bindDock = [this](QDockWidget* dock) {
            m_sceneExplorerDock = dock;
            if(dock != nullptr) {
                m_windowMenu->addAction(dock->toggleViewAction());
                applyInitialPanelLayout();
            }
        };
        composition.bindShellPort = [this](
                                                robot_qt_viewer::SceneExplorerWorkbenchShellPort* port) {
            m_sceneExplorerWorkbenchPort = port;
            if(m_sceneExplorerActionRouter != nullptr) {
                m_sceneExplorerActionRouter->setSceneExplorerPort(port);
            }
        };
        registerFactory(
            robot_qt_viewer::makeOwnedSceneExplorerWorkbenchRuntimeContributionFactory(
                std::move(composition)));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::Motion)) {
        robot_qt_viewer::RobotRunWorkbenchComposition composition;
        composition.documentContext = &m_documentContext;
        composition.documentViewRegistry = &m_documentViewRegistry;
        composition.runService = m_robotRunService.get();
        composition.mainWindow = this;
        composition.detailsTitle = uiText("action.robotRunDetails");
        composition.showStatus = [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        };
        composition.collisionQueriesChanged = [this](bool enabled) {
            if(m_collisionQueriesAction == nullptr) {
                return;
            }
            QSignalBlocker blocker(m_collisionQueriesAction);
            m_collisionQueriesAction->setChecked(enabled);
        };
        composition.collisionGeometryVisibilityChanged = [this](bool visible) {
            if(m_collisionGeometryAction == nullptr) {
                return;
            }
            QSignalBlocker blocker(m_collisionGeometryAction);
            m_collisionGeometryAction->setChecked(visible);
        };
        composition.detailsVisibilityChanged = [this](bool visible) {
            if(m_robotRunDetailsAction == nullptr) {
                return;
            }
            QSignalBlocker blocker(m_robotRunDetailsAction);
            m_robotRunDetailsAction->setChecked(visible);
            if(!visible) {
                m_robotRunDetailsRequested = false;
            }
        };
        composition.bindDetailsDock = [this](QDockWidget* dock) {
            m_bottomPanelDock = dock;
            if(dock != nullptr) {
                updateRobotRunDetailsDockVisibility();
            }
        };
        composition.bindShellPort = [this](
                                                robot_qt_viewer::RobotRunWorkbenchShellPort* port) {
            m_robotRunWorkbenchPort = port;
        };
        registerFactory(
            robot_qt_viewer::makeOwnedRobotRunWorkbenchRuntimeContributionFactory(
                std::move(composition)));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::ToolSetup)) {
        robot_qt_viewer::ToolSetupWorkbenchComposition composition;
        composition.documentContext = &m_documentContext;
        composition.documentViewRegistry = &m_documentViewRegistry;
        composition.appServices = m_toolSetupServices.get();
        composition.selectedRobotId = [this]() {
            return m_appController.selectedRobotId();
        };
        composition.selectedLinkName = [this]() {
            return m_appController.selectedLinkName();
        };
        composition.focusMountFrame = [this](
                                                const QString& robotId,
                                                const QString& linkName,
                                                const QString& mountId) {
            focusSceneExplorerMountFrame(robotId, linkName, mountId);
        };
        composition.focusLink = [this](const QString& robotId, const QString& linkName) {
            selectRobotContext(robotId, linkName);
            updateTaskPanel();
        };
        composition.refreshSelectionDependentViews = [this]() {
            refreshSelectedLinkMaterialSummary();
        };
        composition.showStatus = [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        };
        composition.taskDirtyChanged = [this](bool dirty) {
            if(m_workbenchManager.activeWorkbench() != Kind::ToolSetup) {
                return;
            }
            m_workbenchManager.setSessionDirty(dirty);
            m_workbenchManager.setSessionCanExit(!dirty);
        };
        composition.saveProject = [this]() {
            saveProject();
        };
        composition.requestTaskExit = [this]() {
            if(m_workbenchTransitionCoordinator == nullptr) {
                return;
            }
            const auto result = m_workbenchTransitionCoordinator->requestReturn(
                m_platformComposition.defaultWorkbenchId,
                QStringLiteral("toolSetupDone"),
                this);
            showWorkbenchTransitionResult(result);
            updateWorkbenchActions();
            updateTaskPanel();
            if(!result.succeeded() || m_sceneExplorerWorkbenchPort == nullptr) {
                return;
            }
            const robot_qt_viewer::SceneExplorerNodeRef node =
                m_sceneExplorerWorkbenchPort->currentNode();
            if(node.kind != robot_qt_viewer::SceneExplorerNodeKind::RobotMount) {
                updateTaskPanel();
                return;
            }
            const robot_qt_viewer::SceneSelectionIntent intent =
                m_sceneExplorerWorkbenchPort->selectionIntentForNode(node);
            if(intent.kind == robot_qt_viewer::SceneSelectionIntentKind::SelectRobotMount) {
                selectRobotContext(intent.robotId, intent.linkName, intent.mountId);
            }
            updateTaskPanel();
        };
        composition.bindShellPort = [this](
                                                robot_qt_viewer::ToolSetupWorkbenchShellPort* port) {
            m_toolSetupWorkbenchPort = port;
            if(m_sceneExplorerActionRouter != nullptr) {
                m_sceneExplorerActionRouter->setToolSetupPort(port);
            }
        };
        registerFactory(
            robot_qt_viewer::makeOwnedToolSetupWorkbenchRuntimeContributionFactory(
                std::move(composition)));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::Collision)) {
        robot_qt_viewer::CollisionConfigWorkbenchComposition composition;
        composition.documentContext = &m_documentContext;
        composition.documentViewRegistry = &m_documentViewRegistry;
        composition.appServices = m_collisionWorkbenchServices.get();
        composition.showStatus = [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        };
        composition.reloadViewport = [this]() {
            reloadViewportProject();
        };
        composition.saveProjectAs = [this]() {
            saveProjectAs();
        };
        composition.setTaskPanelTitle = [this](const QString& title) {
            if(m_taskPanelDock != nullptr) {
                m_taskPanelDock->setWindowTitle(title);
            }
        };
        composition.bindShellPort = [this](
                                                robot_qt_viewer::CollisionConfigWorkbenchShellPort* port) {
            m_collisionWorkbenchPort = port;
            if(m_sceneExplorerActionRouter != nullptr) {
                m_sceneExplorerActionRouter->setCollisionWorkbenchPort(port);
            }
        };
        registerFactory(
            robot_qt_viewer::makeOwnedCollisionConfigWorkbenchRuntimeContributionFactory(
                std::move(composition)));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::TrajectoryPlanning)) {
        robot_qt_viewer::MotionPlanningWorkbenchComposition composition;
        composition.documentContext = &m_documentContext;
        composition.documentViewRegistry = &m_documentViewRegistry;
        composition.showStatus = [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        };
        registerFactory(
            robot_qt_viewer::makeOwnedMotionPlanningWorkbenchRuntimeContributionFactory(
                std::move(composition)));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::SprayProcess)) {
        registerFactory(robot_qt_viewer::makeSprayProcessWorkbenchRuntimeContributionFactory(
            *m_statusPanelWidget));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::CoatingAnalysis)) {
        robot_qt_viewer::PaintingAnalysisWorkbenchComposition composition;
        composition.documentContext = &m_documentContext;
        composition.documentViewRegistry = &m_documentViewRegistry;
        composition.overlayParent = this;
        composition.tooltipViewport = m_viewport;
        composition.showStatus = [this](const QString& message, int timeoutMs) {
            statusBar()->showMessage(message, timeoutMs);
        };
        composition.connectSurfaceHover =
            [this](QObject& owner,
                robot_qt_viewer::PaintingAnalysisWorkbenchComposition::SurfaceHoverHandler handler) {
                connect(
                    m_viewport,
                    &RobotViewport::surfaceScalarHovered,
                    &owner,
                    [handler = std::move(handler)](
                        const QString& objectId,
                        double valueMeters,
                        const QPoint& position,
                        bool hit) {
                        handler(objectId, valueMeters, position, hit);
                    });
            };
        composition.bindLegendOverlay = [this](QWidget* overlay) {
            m_thicknessLegendOverlay = overlay;
            if(overlay != nullptr && overlay->isVisible()) {
                updateThicknessLegendOverlayGeometry();
            }
        };
        registerFactory(
            robot_qt_viewer::makeOwnedPaintingAnalysisWorkbenchRuntimeContributionFactory(
                std::move(composition)));
    }
    if(m_workbenchPackageRegistry.hasWorkbench(Kind::DigitalTwin)) {
        registerFactory(robot_qt_viewer::makeDigitalTwinWorkbenchRuntimeContributionFactory(
            *m_statusPanelWidget));
    }
    if(m_workbenchPluginLoader != nullptr) {
        for(const auto& factory : m_workbenchPluginLoader->runtimeContributionFactories()) {
            registerFactory(factory);
        }
    }

    if(registrationError.isEmpty() &&
        !m_workbenchContributionHost->instantiateEnabled(
            m_platformComposition.enabledWorkbenchIds,
            m_taskPanelStack,
            &registrationError)) {
        m_workbenchContributionHost->clear();
    }
    if(registrationError.isEmpty()) {
        for(const QString& workbenchId :
                m_workbenchContributionHost->instantiatedWorkbenchIds()) {
            QWidget* const panel =
                m_workbenchContributionHost->panelForWorkbench(workbenchId);
            if(panel != nullptr && m_taskPanelStack->indexOf(panel) < 0) {
                m_taskPanelStack->addWidget(panel);
            }
        }
    }

    QString validationError;
    if(!registrationError.isEmpty() ||
        !m_workbenchPackageRegistry.validateEnabledWorkbenches(&validationError) ||
        !m_workbenchPackageRegistry.validateEnabledWorkbenchLanguages(&validationError)) {
        const QString message = !registrationError.isEmpty()
            ? registrationError
            : validationError;
        throw std::runtime_error(message.toStdString());
    }

    m_workbenchTransitionCoordinator =
        std::make_unique<robot_qt_viewer::RobotQtViewerWorkbenchTransitionCoordinator>(
            m_workbenchPackageRegistry,
            m_workbenchManager,
            m_eventHub);
    m_selectionModel.setEditSessionCoordinator(&m_editSessionCoordinator, this);
    m_languageCoordinator =
        std::make_unique<robot_qt_viewer::RobotQtViewerLanguageCoordinator>(
            *m_localization,
            m_workbenchPackageRegistry);
    m_languageCoordinator->registerShellParticipant(*this);
    QString languageError;
    if(!m_languageCoordinator->retranslateCurrentLanguage(&languageError)) {
        statusBar()->showMessage(languageError, 8000);
    }
    updateWorkbenchActions();
}

void MainWindow::applyInitialPanelLayout()
{
    if(m_sceneExplorerDock == nullptr || m_taskPanelDock == nullptr) {
        return;
    }
    resizeDocks({ m_sceneExplorerDock, m_taskPanelDock }, { 399, 479 }, Qt::Horizontal);
}

void MainWindow::addRobotLinksToTree(
    const QString& robotId,
    const QString& robotName,
    const QStringList& links,
    const QStringList& joints,
    const QStringList& movableJoints,
    const QStringList& movableJointTypes)
{
    if(m_robotRunWorkbenchPort != nullptr) {
        m_robotRunWorkbenchPort->setRobotRuntime(robotId, movableJoints, movableJointTypes);
    }

    if(m_sceneExplorerWorkbenchPort != nullptr) {
        m_sceneExplorerWorkbenchPort->setRobotRuntime(
            robotId,
            robotName,
            links,
            joints,
            movableJoints,
            movableJointTypes);
    }

    if(m_appController.selectedRobotId().isEmpty()) {
        selectRobotContext(robotId);
    }
}

void MainWindow::addSceneObjectToTree(const QString& objectId, const QString& objectName)
{
    if(m_sceneExplorerWorkbenchPort != nullptr) {
        m_sceneExplorerWorkbenchPort->setSceneObjectRuntime(objectId, objectName);
    }
}

void MainWindow::refreshSceneExplorerViewModel()
{
    if(m_sceneExplorerWorkbenchPort == nullptr) {
        return;
    }

    m_sceneExplorerWorkbenchPort->refreshViewModel();
}

bool MainWindow::prepareEditSessionTransition(
    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause cause,
    const QString& sourceId,
    bool restoreEditorTarget)
{
    robot_qt_viewer::RobotQtViewerEditTransitionRequest request;
    request.cause = cause;
    request.sourceId = sourceId;
    request.promptParent = this;
    request.restoreEditorTarget = restoreEditorTarget;
    const robot_qt_viewer::RobotQtViewerWorkbenchTransitionResult result =
        m_editSessionCoordinator.prepareTransition(request);
    updateWorkbenchActions();
    if(!result.succeeded() && !result.message.isEmpty()) {
        statusBar()->showMessage(result.message, 5000);
    }
    return result.succeeded();
}

bool MainWindow::currentSceneExplorerNodeIsLink() const
{
    if(m_sceneExplorerWorkbenchPort == nullptr) {
        return false;
    }
    return m_sceneExplorerWorkbenchPort->currentNode().kind == robot_qt_viewer::SceneExplorerNodeKind::Link;
}

void MainWindow::focusSceneExplorerMountFrame(
    const QString& robotId,
    const QString& linkName,
    const QString& mountId)
{
    if(m_sceneExplorerWorkbenchPort == nullptr || mountId.isEmpty()) {
        return;
    }

    refreshSceneExplorerViewModel();
    robot_qt_viewer::SceneExplorerNodeRef node;
    node.kind = robot_qt_viewer::SceneExplorerNodeKind::RobotMount;
    node.id = mountId;
    node.linkName = linkName;
    m_sceneExplorerWorkbenchPort->selectNode(node);
    updateWorkbenchActions();
}

void MainWindow::handleSceneExplorerNodeActivated(const robot_qt_viewer::SceneExplorerNodeRef& node, int)
{
    if(m_viewport == nullptr || m_sceneExplorerWorkbenchPort == nullptr) {
        return;
    }

    if(handleCollisionModelConfigurationNodeActivated(node)) {
        return;
    }

    const bool sameEditedMount =
        node.kind == robot_qt_viewer::SceneExplorerNodeKind::RobotMount &&
        m_toolSetupWorkbenchPort != nullptr &&
        node.id == m_toolSetupWorkbenchPort->currentMountId();
    const bool switchingFromFrameEditor =
        m_workbenchManager.activeWorkbench() == robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup &&
        !sameEditedMount;
    if(switchingFromFrameEditor &&
        !prepareEditSessionTransition(
            robot_qt_viewer::RobotQtViewerWorkbenchTransitionCause::SelectionChange,
            QStringLiteral("sceneExplorerSelection"),
            false)) {
        refreshSceneExplorerViewModel();
        return;
    }
    if(switchingFromFrameEditor && node.kind != robot_qt_viewer::SceneExplorerNodeKind::RobotMount) {
        enterWorkbench(robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse, QStringLiteral("sceneExplorerSelection"));
    }
    if(!m_sceneExplorerWorkbenchPort->resolvePendingTransformPreviewIfTargetChanges(node, this)) {
        refreshSceneExplorerViewModel();
        return;
    }

    const robot_qt_viewer::SceneSelectionIntent intent =
        m_sceneExplorerWorkbenchPort->selectionIntentForNode(node);
    updateWorkbenchActions();
    switch(intent.kind) {
    case robot_qt_viewer::SceneSelectionIntentKind::Clear:
        if(!selectRobotContext(QString())) {
            return;
        }
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectSceneObject:
        m_selectionModel.selectSceneObject(intent.itemId, QStringLiteral("sceneExplorer"));
        if(m_selectionModel.payload().objectId != intent.itemId ||
            !m_selectionModel.payload().objectFrameId.isEmpty()) {
            refreshSceneExplorerViewModel();
            return;
        }
        m_appController.setObjectInspectorContext(intent.itemId);
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectObjectFrame:
        if(m_sceneExplorerWorkbenchPort != nullptr &&
            !m_sceneExplorerWorkbenchPort->resolvePendingTransformPreviewIfTargetChanges(
                robot_qt_viewer::SceneExplorerNodeRef(),
                this)) {
            refreshSceneExplorerViewModel();
            return;
        }
        if(m_sceneExplorerWorkbenchPort != nullptr) {
            m_sceneExplorerWorkbenchPort->setWorkbenchDescriptor(m_workbenchManager.activeDescriptor());
        }
        m_selectionModel.selectObjectFrame(
            intent.itemId,
            intent.linkName,
            QStringLiteral("sceneExplorerObjectFrame"));
        if(m_selectionModel.payload().objectId != intent.itemId ||
            m_selectionModel.payload().objectFrameId != intent.linkName) {
            refreshSceneExplorerViewModel();
            return;
        }
        m_appController.setObjectInspectorContext(intent.itemId);
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectToolAsset:
        m_selectionModel.selectToolAsset(intent.itemId, QStringLiteral("sceneExplorer"));
        if(m_selectionModel.payload().assetId != intent.itemId) {
            refreshSceneExplorerViewModel();
            return;
        }
        m_appController.setToolAssetInspectorContext(intent.itemId);
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectRobotJoint:
        m_selectionModel.selectRobotJoint(
            intent.robotId,
            intent.jointName,
            QStringLiteral("sceneExplorerRobotJoint"));
        if(m_selectionModel.payload().robotId != intent.robotId ||
            m_selectionModel.payload().jointName != intent.jointName) {
            refreshSceneExplorerViewModel();
            return;
        }
        m_appController.setRobotLinkInspectorContext(intent.robotId, QString());
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectRobotMount:
        if(m_workbenchManager.activeWorkbench() == robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) {
            enterWorkbench(
                robot_qt_viewer::RobotQtViewerWorkbenchKind::Browse,
                QStringLiteral("sceneExplorerMountSelection"));
            if(m_workbenchManager.activeWorkbench() == robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) {
                refreshSceneExplorerViewModel();
                return;
            }
        }
        if(!selectRobotContext(intent.robotId, intent.linkName, intent.mountId)) {
            return;
        }
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectMountedAttachment:
        if(m_toolSetupWorkbenchPort != nullptr) {
            m_toolSetupWorkbenchPort->selectToolAttachmentById(intent.itemId.toStdString());
        }
        if(m_selectionModel.payload().attachmentId != intent.itemId) {
            refreshSceneExplorerViewModel();
            return;
        }
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::SelectRobotLink:
        if(!selectRobotContext(intent.robotId, intent.linkName)) {
            return;
        }
        statusBar()->showMessage(intent.statusMessage, 3000);
        updateTaskPanel();
        return;
    case robot_qt_viewer::SceneSelectionIntentKind::None:
        if(!intent.statusMessage.isEmpty()) {
            statusBar()->showMessage(intent.statusMessage, 4000);
        }
        updateTaskPanel();
        return;
    }
}

bool MainWindow::handleCollisionModelConfigurationNodeActivated(
    const robot_qt_viewer::SceneExplorerNodeRef& node)
{
    if(m_workbenchManager.activeWorkbench() != robot_qt_viewer::RobotQtViewerWorkbenchKind::Collision ||
        m_collisionWorkbenchPort == nullptr ||
        !m_collisionWorkbenchPort->isCollisionModelConfigurationActive()) {
        return false;
    }

    if(!robot_qt_viewer::sceneExplorerNodeKindCanConfigureCollisionModel(node.kind)) {
        statusBar()->showMessage(
            QStringLiteral("Finish or cancel collision model configuration before selecting this item."),
            3000);
        if(m_hasCollisionModelConfigurationNode && m_sceneExplorerWorkbenchPort != nullptr) {
            m_sceneExplorerWorkbenchPort->selectNode(m_collisionModelConfigurationNode);
        }
        refreshSceneExplorerViewModel();
        return true;
    }

    if(!m_collisionWorkbenchPort->canHandoffCollisionModelConfiguration()) {
        statusBar()->showMessage(QStringLiteral("Collision model configuration has pending changes."), 3000);
        if(m_hasCollisionModelConfigurationNode && m_sceneExplorerWorkbenchPort != nullptr) {
            m_sceneExplorerWorkbenchPort->selectNode(m_collisionModelConfigurationNode);
        }
        refreshSceneExplorerViewModel();
        return true;
    }

    m_collisionWorkbenchPort->cancelCollisionModelConfigurationForHandoff(
        QStringLiteral("sceneExplorerCollisionModelTargetHandoff"));
    if(!selectCollisionModelConfigurationTarget(node, QStringLiteral("sceneExplorerCollisionModelTargetHandoff"))) {
        statusBar()->showMessage(
            QStringLiteral("Select a robot, link, object, or attachment to configure collision geometry."),
            3000);
        refreshSceneExplorerViewModel();
        return true;
    }

    rememberCollisionModelConfigurationTarget(node);
    m_collisionWorkbenchPort->showCollisionModelConfiguration();
    statusBar()->showMessage(QStringLiteral("Collision model configuration switched."), 3000);
    updateWorkbenchActions();
    updateTaskPanel();
    refreshSceneExplorerViewModel();
    return true;
}

bool MainWindow::selectCollisionModelConfigurationTarget(
    const robot_qt_viewer::SceneExplorerNodeRef& node,
    const QString& sourceId)
{
    if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::Robot ||
        node.kind == robot_qt_viewer::SceneExplorerNodeKind::Link) {
        const QString linkName = collisionModelConfigurationLinkName(node);
        if(node.id.isEmpty() || linkName.isEmpty()) {
            return false;
        }
        selectRobotContext(node.id, linkName);
        return true;
    }

    if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::Object) {
        if(node.id.isEmpty()) {
            return false;
        }
        selectRobotContext(QString());
        m_appController.setObjectInspectorContext(node.id);
        m_selectionModel.selectSceneObject(node.id, sourceId);
        return true;
    }

    if(node.kind == robot_qt_viewer::SceneExplorerNodeKind::ToolAttachment) {
        if(node.id.isEmpty() || m_toolSetupWorkbenchPort == nullptr) {
            return false;
        }
        m_toolSetupWorkbenchPort->selectToolAttachmentById(node.id.toStdString());
        return true;
    }

    return false;
}

QString MainWindow::collisionModelConfigurationLinkName(
    const robot_qt_viewer::SceneExplorerNodeRef& node) const
{
    QString linkName = node.linkName;
    if(linkName.isEmpty() &&
        (node.kind == robot_qt_viewer::SceneExplorerNodeKind::Robot ||
            node.kind == robot_qt_viewer::SceneExplorerNodeKind::Link) &&
        m_sceneExplorerWorkbenchPort != nullptr) {
        const QHash<QString, QStringList> robotLinksById =
            m_sceneExplorerWorkbenchPort->robotLinksByRobotId();
        const QStringList robotLinks = robotLinksById.value(node.id);
        if(!robotLinks.isEmpty()) {
            linkName = robotLinks.first();
        }
    }
    return linkName;
}

void MainWindow::rememberCollisionModelConfigurationTarget(
    const robot_qt_viewer::SceneExplorerNodeRef& node)
{
    m_collisionModelConfigurationNode = node;
    m_hasCollisionModelConfigurationNode =
        robot_qt_viewer::sceneExplorerNodeKindCanConfigureCollisionModel(node.kind) &&
        !node.id.isEmpty();
}

bool MainWindow::selectRobotContext(
    const QString& robotId,
    const QString& preferredLinkName,
    const QString& preferredMountId)
{
    robot_qt_viewer::SceneRobotSelectionContext selectionContext{
        robotId,
        preferredLinkName,
        preferredMountId
    };
    const bool explicitMountSelection = !preferredMountId.isEmpty();
    const bool explicitLinkSelection = !preferredLinkName.isEmpty() && preferredMountId.isEmpty();
    if(!explicitLinkSelection && m_sceneExplorerWorkbenchPort != nullptr) {
        selectionContext =
            m_sceneExplorerWorkbenchPort->robotSelectionContext(robotId, preferredLinkName, preferredMountId);
    }

    const QString mountId = selectionContext.mountId;
    const QString linkName = selectionContext.linkName;
    const QString selectedRobotId = selectionContext.robotId;
    if(!mountId.isEmpty()) {
        m_selectionModel.selectRobotMount(
            selectedRobotId,
            linkName,
            mountId,
            QStringLiteral("selectRobotContext"));
    } else {
        m_selectionModel.selectRobotLink(selectedRobotId, linkName, QStringLiteral("selectRobotContext"));
    }

    const robot_qt_viewer::RobotQtViewerSelectionPayload committed = m_selectionModel.payload();
    const bool selectionCommitted = !mountId.isEmpty()
        ? committed.robotId == selectedRobotId && committed.linkName == linkName &&
            committed.mountId == mountId
        : committed.robotId == selectedRobotId && committed.linkName == linkName &&
            committed.mountId.isEmpty();
    if(!selectionCommitted) {
        refreshSceneExplorerViewModel();
        return false;
    }
    if(!mountId.isEmpty()) {
        m_appController.setRobotMountInspectorContext(selectedRobotId, linkName, mountId);
    } else {
        m_appController.setRobotLinkInspectorContext(selectedRobotId, linkName);
    }

    refreshSelectedLinkMaterialSummary();
    if(explicitMountSelection &&
        m_toolSetupWorkbenchPort != nullptr &&
        m_workbenchManager.activeWorkbench() == robot_qt_viewer::RobotQtViewerWorkbenchKind::ToolSetup) {
        m_toolSetupWorkbenchPort->updateToolFrameVisibility();
    }
    updateWorkbenchActions();
    return true;
}

void MainWindow::clearInspectorSelectionContext()
{
    m_selectionModel.clear(QStringLiteral("clearInspectorSelectionContext"));
    const robot_qt_viewer::RobotQtViewerSelectionPayload committed = m_selectionModel.payload();
    if(committed.robotId.isEmpty() && committed.objectId.isEmpty() &&
        committed.mountId.isEmpty() && committed.attachmentId.isEmpty() &&
        committed.assetId.isEmpty()) {
        m_appController.clearInspectorSelectionContext();
    }
}

void MainWindow::setActiveCollisionDetectorContext(const QString& detectorId)
{
    m_selectionModel.setCollisionDetector(detectorId, QStringLiteral("collisionWorkbench"));
    if(m_selectionModel.payload().collisionDetectorId == detectorId) {
        m_appController.setActiveCollisionDetectorContext(detectorId);
    }
}

void MainWindow::setMarkedCollisionPairAContext(const QString& robotId, const QString& linkName)
{
    m_selectionModel.setCollisionPairA(robotId, linkName, QStringLiteral("collisionWorkbench"));
    const robot_qt_viewer::RobotQtViewerSelectionPayload committed = m_selectionModel.payload();
    if(committed.collisionPairRobotA == robotId && committed.collisionPairLinkA == linkName) {
        m_appController.setMarkedCollisionPairAContext(robotId, linkName);
    }
}

void MainWindow::refreshSelectedLinkMaterialSummary()
{
    if(m_sceneExplorerWorkbenchPort == nullptr) {
        return;
    }

    if(m_appController.selectedRobotId().isEmpty() || m_appController.selectedLinkName().isEmpty()) {
        m_sceneExplorerWorkbenchPort->setSummaryText(m_appController.selectedRobotId().isEmpty()
            ? "No robot selected"
            : QString("Robot: %1\nSelect a link to inspect material color.").arg(m_appController.selectedRobotId()));
        return;
    }

    const std::vector<ProjectScene::RobotLinkMaterialInfo> materials =
        m_viewport != nullptr
            ? m_viewport->robotLinkMaterials(m_appController.selectedRobotId(), m_appController.selectedLinkName())
            : std::vector<ProjectScene::RobotLinkMaterialInfo>();

    QStringList lines;
    lines << QString("Robot: %1").arg(m_appController.selectedRobotId());
    lines << QString("Link: %1").arg(m_appController.selectedLinkName());
    lines << "Material:";

    if(materials.empty()) {
        lines << "  no visual material information";
    } else {
        const int maxRows = std::min<int>(static_cast<int>(materials.size()), 4);
        for(int i = 0; i < maxRows; ++i) {
            const ProjectScene::RobotLinkMaterialInfo& info = materials[static_cast<std::size_t>(i)];
            const QString part = info.partUid.empty()
                ? QString("visual %1").arg(i + 1)
                : QString::fromUtf8(info.partUid.c_str());
            const QString mesh = info.meshPath.empty()
                ? QString("<none>")
                : QString::fromUtf8(info.meshPath.c_str());
            lines << QString("  %1").arg(part);
            lines << QString("    source: %1").arg(QString::fromUtf8(info.source.c_str()));
            lines << QString("    color: rgba(%1, %2, %3, %4)")
                .arg(info.r, 0, 'f', 3)
                .arg(info.g, 0, 'f', 3)
                .arg(info.b, 0, 'f', 3)
                .arg(info.a, 0, 'f', 3);
            lines << QString("    override: %1").arg(QString::fromUtf8(info.overrideState.c_str()));
            lines << QString("    mesh: %1").arg(mesh);
        }
        if(static_cast<int>(materials.size()) > maxRows) {
            lines << QString("  ... %1 more visual parts").arg(static_cast<int>(materials.size()) - maxRows);
        }
    }

    m_sceneExplorerWorkbenchPort->setSummaryText(lines.join('\n'));
}

void MainWindow::loadRobot()
{
    if(m_robotLoader == nullptr || m_sdk == nullptr) {
        statusBar()->showMessage("RobotSDK is not available.", 3000);
        return;
    }

    const QString fileName = robot_qt_viewer::ProjectAssemblyDialogService::selectRobotForImport(this);

    if(fileName.isEmpty()) {
        return;
    }

    smrobotgen2::sdk::RobotSourceType sourceType = smrobotgen2::sdk::RobotSourceType::Urdf;
    if(fileName.endsWith(".xml", Qt::CaseInsensitive)) {
        sourceType = smrobotgen2::sdk::RobotSourceType::Simscape;
    }

    const QByteArray localFileName = fileName.toLocal8Bit();
    smrobotgen2::sdk::LoadRobotResult result = m_robotLoader->load(
        sourceType,
        localFileName.constData());

    if(!smrobotgen2::sdk::succeeded(result.status) || result.model == nullptr) {
        const char* message = result.status.message != nullptr ? result.status.message : "Unknown error";
        statusBar()->showMessage(QString("Load failed: %1").arg(message), 5000);
        return;
    }

    if(m_robotModel != nullptr) {
        m_sdk->destroyRobotModel(m_robotModel);
    }
    m_robotModel = result.model;

    updateRobotPanel(*m_robotModel);
    m_viewport->setRobotSummary(m_robotModel->name(), m_robotModel->linkCount(), m_robotModel->jointCount());
    statusBar()->showMessage(QString("Loaded %1").arg(m_robotModel->name()), 5000);
}

void MainWindow::updateRobotPanel(const smrobotgen2::sdk::IRobotModel& model)
{
    if(m_sceneExplorerWorkbenchPort != nullptr) {
        m_sceneExplorerWorkbenchPort->setSummaryText(QString("Name: %1\nRoot: %2\nLinks: %3\nJoints: %4\nDOF: %5")
            .arg(model.name())
            .arg(model.rootLink())
            .arg(model.linkCount())
            .arg(model.jointCount())
            .arg(model.dof()));
    }

    if(m_statusPanelWidget == nullptr) {
        return;
    }

    QStringList statusItems;
    statusItems << QString("Loaded robot: %1").arg(model.name());
    statusItems << QString("Root link: %1").arg(model.rootLink());
    statusItems << QString("Links: %1").arg(model.linkCount());
    statusItems << QString("Joints: %1").arg(model.jointCount());
    statusItems << QString("DOF: %1").arg(model.dof());

    const std::size_t previewCount = model.linkCount() < 8 ? model.linkCount() : 8;
    for(std::size_t i = 0; i < previewCount; ++i) {
        statusItems << QString("Link %1: %2").arg(i).arg(model.linkName(i));
    }
    m_statusPanelWidget->setStatusItems(statusItems);
}

void MainWindow::saveViewportImage()
{
    QString selectedFilter;
    const QString fileName = robot_qt_viewer::getSaveFileName(
        QStringLiteral("shell.viewportImage.save"),
        this,
        "Save viewport image",
        QString(),
        "PNG Image (*.png);;BMP Image (*.bmp)",
        &selectedFilter);

    if(fileName.isEmpty()) {
        return;
    }

    QString outputPath = fileName;
    if(!outputPath.endsWith(".png", Qt::CaseInsensitive) &&
        !outputPath.endsWith(".bmp", Qt::CaseInsensitive)) {
        outputPath += selectedFilter.contains("BMP", Qt::CaseInsensitive) ? ".bmp" : ".png";
    }

    const char* imageFormat = outputPath.endsWith(".bmp", Qt::CaseInsensitive) ? "BMP" : "PNG";
    const QImage image = m_viewport->grabFramebuffer();
    if(!image.isNull() && image.save(outputPath, imageFormat)) {
        statusBar()->showMessage(QString("Saved %1").arg(outputPath), 3000);
    } else {
        statusBar()->showMessage(QString("Failed to save %1").arg(outputPath), 3000);
    }
}

bool MainWindow::reloadViewportProject(const QString& operationId)
{
    const QString previousRobotId = m_appController.selectedRobotId();
    const QString previousLinkName = m_appController.selectedLinkName();
    const QString previousMountId = m_toolSetupWorkbenchPort != nullptr
        ? m_toolSetupWorkbenchPort->currentMountId()
        : QString();
    const QString previousAttachmentId = m_toolSetupWorkbenchPort != nullptr
        ? m_toolSetupWorkbenchPort->currentAttachmentId()
        : QString();
    const QString configuredCollisionDetectorId = m_collisionWorkbenchPort != nullptr
        ? m_collisionWorkbenchPort->currentCollisionDetectorId()
        : QString();
    const QString previousCollisionDetectorId = !configuredCollisionDetectorId.isEmpty()
        ? configuredCollisionDetectorId
        : m_appController.inspectorContext().activeCollisionDetectorId();
    if(m_robotRunWorkbenchPort != nullptr) {
        m_robotRunWorkbenchPort->clearRuntime();
    }
    clearInspectorSelectionContext();

    if(m_sceneExplorerWorkbenchPort != nullptr) {
        m_sceneExplorerWorkbenchPort->clearRuntime();
    }

    const robot_qt_viewer::ViewportReloadWorkflowResult reloadResult =
        m_appController.reloadViewport(
            QStringLiteral("reloadViewportProject"),
            operationId);
    if(!reloadResult.success) {
        if(operationId.isEmpty()) {
            statusBar()->showMessage(reloadResult.errorMessage, 8000);
        } else {
            refreshOperationStatusPresentation(operationId);
        }
        return false;
    }
    if(!previousCollisionDetectorId.isEmpty()) {
        m_viewport->setActiveCollisionDetector(previousCollisionDetectorId);
        setActiveCollisionDetectorContext(previousCollisionDetectorId);
    }
    if(m_collisionGeometryAction != nullptr) {
        QSignalBlocker blocker(m_collisionGeometryAction);
        m_collisionGeometryAction->setChecked(reloadResult.showCollisionGeometry);
    }
    if(m_collisionQueriesAction != nullptr) {
        QSignalBlocker blocker(m_collisionQueriesAction);
        m_collisionQueriesAction->setChecked(false);
    }
    const bool restoreRobotContext =
        !previousRobotId.isEmpty() &&
        m_appController.hasRobot(previousRobotId);
    if(restoreRobotContext) {
        selectRobotContext(previousRobotId, previousLinkName, previousMountId);
        if(!previousAttachmentId.isEmpty()) {
            m_selectionModel.selectMountedAttachment(
                previousAttachmentId,
                previousRobotId,
                previousLinkName,
                previousMountId,
                QStringLiteral("reloadViewportProject"));
            robot_qt_viewer::RobotQtViewerViewportPreviewPayload preview;
            preview.setActiveMountedAttachment = true;
            preview.activeMountedAttachmentId = previousAttachmentId;
            m_viewportPreviewState.mutate(preview, QStringLiteral("reloadViewportProject"));
        }
    }
    m_viewport->update();
    LOG_DEBUG("rs2026") << "MainWindow reloadViewportProject: elapsedMs=" << reloadResult.elapsedMs
        << ", robots=" << reloadResult.robotCount
        << ", objects=" << reloadResult.objectCount
        << ", detectors=" << reloadResult.detectorCount;
    return true;
}
