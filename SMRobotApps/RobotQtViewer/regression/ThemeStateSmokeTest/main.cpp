#include "RobotQtViewerTheme.h"
#include "RobotQtViewerPlatformConfigurationDialog.h"
#include "RobotQtWidgetUtils.h"
#include "ToolSetupViewModel.h"
#include "ToolSetupWidget.h"
#include "MotionControlWidget.h"
#include "CameraPreviewWidget.h"

#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFile>
#include <QHash>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QSet>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if(!condition) {
            throw std::runtime_error(message);
        }
    }

    std::uint64_t renderDigest(QWidget& widget)
    {
        widget.ensurePolished();
        const QSize size = widget.size().expandedTo(widget.sizeHint());
        widget.resize(size);
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        widget.render(&image);

        std::uint64_t digest = 1469598103934665603ULL;
        for(int y = 0; y < image.height(); ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for(int x = 0; x < image.width(); ++x) {
                digest ^= static_cast<std::uint64_t>(row[x]);
                digest *= 1099511628211ULL;
            }
        }
        return digest;
    }

    void requireThemeResources()
    {
        const char* resources[] = {
            ":/RobotQtViewer/icons/theme/arrow-dark.xpm",
            ":/RobotQtViewer/icons/theme/arrow-light.xpm",
            ":/RobotQtViewer/icons/theme/check.xpm",
            ":/RobotQtViewer/icons/theme/minus.xpm",
            ":/RobotQtViewer/icons/theme/radio-dot.xpm"
        };
        for(const char* resource : resources) {
            require(QFile::exists(QString::fromLatin1(resource)), "theme resource is missing");
        }
    }

    void verifyTheme(
        QApplication& application,
        robot_qt_viewer::ThemeKind theme)
    {
        using namespace robot_qt_viewer;

        ThemeManager::apply(application, theme);
        const QString styleSheet = application.styleSheet();
        require(styleSheet.contains(QStringLiteral("uiActionRole=\"primary\"")),
            "primary action selector is missing");
        require(styleSheet.contains(QStringLiteral("uiActionRole=\"destructive\"")),
            "destructive action selector is missing");
        require(styleSheet.contains(QStringLiteral("QCheckBox::indicator:checked")),
            "checked indicator selector is missing");
        require(styleSheet.contains(QStringLiteral("QComboBox::down-arrow")),
            "combo arrow selector is missing");

        QWidget root;
        root.resize(420, 420);
        auto* layout = new QVBoxLayout(&root);

        QPushButton standard(QStringLiteral("Action"), &root);
        QPushButton primary(QStringLiteral("Action"), &root);
        QPushButton accent(QStringLiteral("Action"), &root);
        QPushButton destructive(QStringLiteral("Action"), &root);
        configureActionButton(&standard, UiActionRole::Standard);
        configureActionButton(&primary, UiActionRole::Primary);
        configureActionButton(&accent, UiActionRole::Accent);
        configureActionButton(&destructive, UiActionRole::Destructive);
        layout->addWidget(&standard);
        layout->addWidget(&primary);
        layout->addWidget(&accent);
        layout->addWidget(&destructive);

        QComboBox combo(&root);
        combo.addItems({ QStringLiteral("First"), QStringLiteral("Second") });
        configureInspectorCombo(&combo);
        layout->addWidget(&combo);

        QComboBox entityCombo(&root);
        entityCombo.addItem(QStringLiteral("Short mount"), QStringLiteral("short"));
        entityCombo.addItem(
            QStringLiteral("Robot Cell Alpha / Manipulator With A Very Long Name / Link 6 / Camera Mount"),
            QStringLiteral("long_mount"));
        configureInspectorEntityCombo(&entityCombo);
        layout->addWidget(&entityCombo);

        QCheckBox check(QStringLiteral("Enabled"), &root);
        configureInspectorToggle(&check);
        layout->addWidget(&check);
        QRadioButton radio(QStringLiteral("Selected"), &root);
        configureInspectorToggle(&radio);
        radio.setChecked(true);
        layout->addWidget(&radio);

        QDialogButtonBox buttons(
            QDialogButtonBox::Ok |
            QDialogButtonBox::Cancel |
            QDialogButtonBox::Discard,
            &root);
        configureDialogButtonBox(&buttons);
        layout->addWidget(&buttons);

        root.show();
        application.processEvents();

        require(primary.property("uiActionRole").toString() == QStringLiteral("primary"),
            "primary action role was not applied");
        require(accent.property("uiActionRole").toString() == QStringLiteral("accent"),
            "accent action role was not applied");
        require(destructive.property("uiActionRole").toString() == QStringLiteral("destructive"),
            "destructive action role was not applied");
        require(check.property("uiControlRole").toString() == QStringLiteral("toggle"),
            "toggle role was not applied");
        require(buttons.button(QDialogButtonBox::Ok)->property("uiActionRole").toString() ==
                QStringLiteral("primary"),
            "dialog accept role was not promoted");
        require(buttons.button(QDialogButtonBox::Cancel)->property("uiActionRole").toString() ==
                QStringLiteral("standard"),
            "dialog reject role is not standard");
        require(buttons.button(QDialogButtonBox::Discard)->property("uiActionRole").toString() ==
                QStringLiteral("destructive"),
            "dialog destructive role was not applied");
        require(entityCombo.isEditable() && entityCombo.insertPolicy() == QComboBox::NoInsert,
            "entity selector is not searchable without inserting arbitrary values");
        require(entityCombo.completer() != nullptr &&
                entityCombo.completer()->filterMode() == Qt::MatchContains &&
                entityCombo.completer()->caseSensitivity() == Qt::CaseInsensitive,
            "entity selector does not provide case-insensitive contains filtering");
        require(entityCombo.minimumHeight() >= 30,
            "entity selector does not preserve a usable click target");
        entityCombo.setCurrentIndex(1);
        application.processEvents();
        require(entityCombo.toolTip().contains(QStringLiteral("Camera Mount")),
            "entity selector does not expose the full selected value as a tooltip");
        entityCombo.showPopup();
        application.processEvents();
        require(entityCombo.view() != nullptr &&
                entityCombo.view()->window()->width() >= entityCombo.width(),
            "entity selector popup is narrower than its closed control");
        entityCombo.hidePopup();
        require(renderDigest(standard) != renderDigest(primary),
            "primary action is visually indistinguishable from a standard action");
        require(renderDigest(standard) != renderDigest(destructive),
            "destructive action is visually indistinguishable from a standard action");
        const std::uint64_t uncheckedDigest = renderDigest(check);
        check.setChecked(true);
        application.processEvents();
        require(uncheckedDigest != renderDigest(check),
            "checked and unchecked controls are visually indistinguishable");

        standard.setEnabled(false);
        application.processEvents();
        require(renderDigest(primary) != renderDigest(standard),
            "disabled and primary actions are visually indistinguishable");
    }

    void verifyToolSetupCommandBar(QApplication& application)
    {
        ToolSetupWidget widget;
        widget.resize(360, 560);
        widget.show();

        ToolSetupPanelView selectionView;
        selectionView.mountFrameMode = ToolSetupMountFrameMode::Selection;
        selectionView.mountItems.push_back({ QStringLiteral("Flange"), QStringLiteral("flange") });
        selectionView.selectedMountId = QStringLiteral("flange");
        selectionView.mountItemsEnabled = true;
        selectionView.attachAssetEnabled = false;
        widget.setDocumentView(selectionView);
        application.processEvents();

        auto* commandBar = widget.findChild<QFrame*>(QStringLiteral("toolSetupCommandBar"));
        auto* applyTask = widget.findChild<QPushButton*>(
            QStringLiteral("toolSetupApplyTaskButton"));
        auto* cancelTask = widget.findChild<QPushButton*>(
            QStringLiteral("toolSetupCancelTaskButton"));
        require(commandBar != nullptr && applyTask != nullptr && cancelTask != nullptr,
            "Tool Setup fixed task command bar is missing");
        auto* saveProject = widget.findChild<QPushButton*>(
            QStringLiteral("toolSetupSaveProjectButton"));
        auto* done = widget.findChild<QPushButton*>(
            QStringLiteral("toolSetupDoneButton"));
        require(saveProject != nullptr && done != nullptr,
            "Tool Setup persistent Save/Done command bar is missing");
        require(saveProject->isVisible() && done->isVisible(),
            "Tool Setup Save/Done commands are hidden in selection mode");
        require(commandBar->isVisible() && applyTask->isVisible() && cancelTask->isVisible(),
            "Tool Setup task commands are not discoverable in selection mode");
        const auto isInsidePanel = [&widget](const QWidget* child) {
            return widget.rect().contains(
                QRect(child->mapTo(&widget, QPoint(0, 0)), child->size()));
        };
        for(QPushButton* button : { applyTask, cancelTask, saveProject, done }) {
            require(button->width() >= 100 && button->height() >= 30,
                "Tool Setup command has no usable clickable geometry");
            require(isInsidePanel(button),
                "Tool Setup command is outside the visible task panel");
        }
        require(!applyTask->isEnabled() && !cancelTask->isEnabled(),
            "Tool Setup task commands are incorrectly enabled in selection mode");
        auto* setupTabs = widget.findChild<QTabWidget*>(QStringLiteral("toolSetupTaskTabs"));
        require(setupTabs != nullptr && setupTabs->count() == 4,
            "Tool Setup does not separate mounts, instances, definitions, and diagnostics");
        require(setupTabs->tabText(0) == QStringLiteral("Mounts") &&
                setupTabs->tabText(1) == QStringLiteral("Installed Devices") &&
                setupTabs->tabText(2) == QStringLiteral("Definitions") &&
                setupTabs->tabText(3) == QStringLiteral("Diagnostics"),
            "Tool Setup task tabs do not expose the intended workflow hierarchy");
        require(widget.findChild<QPushButton*>(QStringLiteral("toolSetupAddCameraButton")) == nullptr,
            "Tool Setup still owns project-level Camera creation");

        ToolSetupPanelView editView = selectionView;
        editView.mountFrameMode = ToolSetupMountFrameMode::Edit;
        editView.mountFrameNameEditorVisible = true;
        editView.mountFrameNameEditorEnabled = true;
        editView.mountTransformEditorVisible = true;
        editView.mountTransformEditorEnabled = true;
        widget.setDocumentView(editView);
        application.processEvents();
        require(applyTask->isVisible() && applyTask->isEnabled() && cancelTask->isVisible(),
            "Tool Setup Apply/Cancel commands are unavailable during focused mount editing");
        require(commandBar->geometry().bottom() <= widget.rect().bottom(),
            "Tool Setup command bar is outside the visible panel viewport");
        require(isInsidePanel(applyTask) && isInsidePanel(cancelTask) &&
                isInsidePanel(saveProject) && isInsidePanel(done),
            "Tool Setup focused task commands are clipped by the panel viewport");
        require(saveProject->isVisible() && done->isVisible(),
            "Tool Setup Save/Done commands are hidden during focused editing");
        require(setupTabs->currentIndex() == 0 && setupTabs->tabBar()->isHidden(),
            "focused mount editing does not switch to a distraction-free Mounts page");

        widget.setDocumentView(selectionView);
        application.processEvents();
        require(setupTabs->tabBar()->isVisible(),
            "Tool Setup task tabs were not restored after mount editing");
        require(!applyTask->isEnabled() && !cancelTask->isEnabled(),
            "Tool Setup task commands did not return to selection state");
    }

    void verifyRobotRunCollisionResults(QApplication& application)
    {
        MotionControlWidget widget;
        widget.resize(420, 900);
        widget.show();
        application.processEvents();

        auto* monitoringButton = widget.findChild<QPushButton*>(
            QStringLiteral("robotRunCollisionMonitoringButton"));
        auto* detailsButton = widget.findChild<QPushButton*>(
            QStringLiteral("robotRunCollisionDetailsButton"));
        auto* runtimeStatus = widget.findChild<QLabel*>(
            QStringLiteral("robotRunCollisionRuntimeStatus"));
        auto* resultSummary = widget.findChild<QWidget*>(
            QStringLiteral("robotRunCollisionResultSummary"));
        require(monitoringButton != nullptr && detailsButton != nullptr &&
                runtimeStatus != nullptr && resultSummary != nullptr,
            "Robot Run collision result controls are incomplete");
        require(resultSummary->minimumWidth() == 0,
            "Robot Run collision summary cannot fit a narrow task panel");

        MotionControlWidget::CollisionDetectorItem detector;
        detector.id = QStringLiteral("detector-a");
        detector.label = QStringLiteral("Detector A");
        detector.enabled = false;
        detector.active = true;
        widget.setCollisionDetectors({ detector }, detector.id);
        require(detailsButton->isEnabled() &&
                runtimeStatus->text() == QStringLiteral("Runtime queries: Off"),
            "configured detector state is not distinguished from runtime queries");

        bool detailsRequested = false;
        QObject::connect(&widget, &MotionControlWidget::collisionDetailsRequested,
            &widget, [&detailsRequested]() { detailsRequested = true; });
        detailsButton->click();
        require(detailsRequested, "Robot Run result details command was not emitted");

        widget.setCollisionMonitoringChecked(true);
        require(runtimeStatus->text() == QStringLiteral("Runtime queries: On") &&
                monitoringButton->text() == QStringLiteral("Disable Runtime Detection"),
            "Robot Run runtime query state did not update");
        widget.setCollisionMonitoringAvailable(false);
        require(runtimeStatus->text() == QStringLiteral("Runtime queries: No detector") &&
                !detailsButton->isEnabled(),
            "Robot Run missing-detector state is not visible");
    }

    void verifyCameraPreviewLocalResize(QApplication& application)
    {
        QWidget viewport;
        viewport.resize(900, 700);
        viewport.show();
        CameraPreviewWidget preview(
            QStringLiteral("camera.instance.1"),
            QStringLiteral("A very long industrial inspection camera name"),
            &viewport);
        preview.move(20, 20);
        preview.show();
        application.processEvents();

        QLabel* handle = preview.findChild<QLabel*>(
            QStringLiteral("cameraPreviewResizeHandle"));
        require(handle != nullptr && handle->isVisible(),
            "Camera preview local resize handle is missing");
        const QSize viewportSize = viewport.size();
        const QSize previewSize = preview.size();

        QMouseEvent press(
            QEvent::MouseButtonPress,
            QPointF(8.0, 8.0),
            QPointF(8.0, 8.0),
            QPointF(100.0, 100.0),
            Qt::LeftButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(handle, &press);
        QMouseEvent move(
            QEvent::MouseMove,
            QPointF(88.0, 68.0),
            QPointF(88.0, 68.0),
            QPointF(180.0, 160.0),
            Qt::NoButton,
            Qt::LeftButton,
            Qt::NoModifier);
        QApplication::sendEvent(handle, &move);
        QMouseEvent release(
            QEvent::MouseButtonRelease,
            QPointF(88.0, 68.0),
            QPointF(88.0, 68.0),
            QPointF(180.0, 160.0),
            Qt::LeftButton,
            Qt::NoButton,
            Qt::NoModifier);
        QApplication::sendEvent(handle, &release);
        application.processEvents();

        require(viewport.size() == viewportSize,
            "Camera preview resize changed its parent viewport geometry");
        require(preview.width() > previewSize.width() && preview.height() > previewSize.height(),
            "Camera preview local resize did not enlarge the preview content area");

        preview.resetPreviewSize();
        application.processEvents();
        require(viewport.size() == viewportSize,
            "Camera preview reset changed its parent viewport geometry");
        require(preview.size() == previewSize,
            "Camera preview reset did not restore the default preview size");

        preview.setPositionLocked(true);
        application.processEvents();
        require(!handle->isVisible(),
            "Camera preview resize handle remains available while layout is locked");
        preview.setPositionLocked(false);
        application.processEvents();
        require(handle->isVisible(),
            "Camera preview resize handle was not restored after unlocking");

        preview.setStatus(CameraPreviewStatus::Error, QStringLiteral("Camera render failed"));
        application.processEvents();
        auto* messageOverlay = preview.findChild<QLabel*>(
            QStringLiteral("cameraPreviewMessageOverlay"));
        require(messageOverlay != nullptr && messageOverlay->isVisible() &&
                messageOverlay->width() > 0 && messageOverlay->height() > 0,
            "Camera preview error state is not visible inside the preview");

        bool hiddenSignalReceived = false;
        QObject::connect(&preview, &CameraPreviewWidget::previewVisibilityChanged,
            [&hiddenSignalReceived](bool visible) {
                if(!visible) {
                    hiddenSignalReceived = true;
                }
            });
        auto* closeButton = preview.findChild<QToolButton*>(
            QStringLiteral("cameraPreviewCloseButton"));
        require(closeButton != nullptr && closeButton->isVisible() &&
                preview.rect().contains(
                    QRect(closeButton->mapTo(&preview, QPoint(0, 0)), closeButton->size())),
            "Camera preview close command is missing or clipped");
        closeButton->click();
        application.processEvents();
        require(hiddenSignalReceived && preview.isHidden(),
            "Camera preview close did not enter the hidden session state");
        preview.show();
        application.processEvents();
        require(preview.isVisible(),
            "A hidden Camera preview cannot be shown again");
    }

    void verifyPlatformConfigurationDialog(QApplication& application)
    {
        using namespace robot_qt_viewer;

        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "platform dialog temporary directory failed");
        const std::filesystem::path root = std::filesystem::u8path(
            temporaryDirectory.path().toUtf8().constData());
        RobotQtViewerWorkbenchPackageRegistry catalog =
            defaultRobotQtViewerWorkbenchPackageRegistry();
        const QString projectAssemblyId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        RobotQtViewerPlatformProfile baseProfile;
        baseProfile.id = QStringLiteral("base-robot");
        baseProfile.displayName = QStringLiteral("General Robot Simulation Platform");
        for(const RobotQtViewerWorkbenchDesc& workbench : catalog.workbenches()) {
            baseProfile.workbenchIds.push_back(workbench.descriptor.id);
        }
        baseProfile.defaultWorkbenchId = projectAssemblyId;
        QString profileSaveError;
        require(RobotQtViewerPlatformProfileIo::saveProfile(
                    root / "profiles" / "base-robot.platform.json",
                    baseProfile,
                    &profileSaveError),
            "base Product Profile fixture could not be saved");
        RobotQtViewerPlatformProfile mobileProfile;
        mobileProfile.id = QStringLiteral("mobile-robot");
        mobileProfile.displayName = QStringLiteral("Mobile Robot Platform");
        mobileProfile.workbenchIds.push_back(projectAssemblyId);
        mobileProfile.defaultWorkbenchId = projectAssemblyId;
        require(RobotQtViewerPlatformProfileIo::saveProfile(
                    root / "profiles" / "mobile-robot.platform.json",
                    mobileProfile,
                    &profileSaveError),
            "alternate Product Profile fixture could not be saved");
        RobotQtViewerPlatformConfigurationDialog dialog(
            catalog,
            root / "profiles",
            QStringLiteral("base-robot"));
        dialog.show();
        application.processEvents();

        auto* saveProfile = dialog.findChild<QPushButton*>(
            QStringLiteral("saveCurrentProductProfileButton"));
        auto* activate = dialog.findChild<QPushButton*>(
            QStringLiteral("switchProductProfileButton"));
        auto* cancel = dialog.findChild<QPushButton*>(
            QStringLiteral("cancelProductProfileButton"));
        auto* workbenchTree = dialog.findChild<QTreeWidget*>(
            QStringLiteral("productProfileWorkbenchTree"));
        auto* runtimeIdentity = dialog.findChild<QLabel*>(
            QStringLiteral("platformRuntimeIdentityLabel"));
        auto* runningProfile = dialog.findChild<QLabel*>(
            QStringLiteral("runningProductProfileLabel"));
        auto* footer = dialog.findChild<QWidget*>(
            QStringLiteral("productProfileFooter"));
        require(saveProfile != nullptr && saveProfile->isVisible(),
            "platform manager Save Current Profile button is missing or hidden");
        require(activate != nullptr && activate->isVisible(),
            "platform manager Switch and Restart button is missing or hidden");
        require(cancel != nullptr && cancel->isVisible(),
            "platform manager Cancel button is missing or hidden");
        require(saveProfile->width() >= 130 && activate->width() >= 130 &&
                cancel->width() >= 130 && saveProfile->height() >= 36 &&
                activate->height() >= 36 && cancel->height() >= 36,
            "platform manager command buttons have no clickable geometry");
        require(activate->text() == QStringLiteral("OK"),
            "unchanged running Product Profile does not offer OK");
        dialog.resize(dialog.minimumSize());
        application.processEvents();
        const auto isInsideDialog = [&dialog](const QWidget* widget) {
            const QRect geometry(widget->mapTo(&dialog, QPoint(0, 0)), widget->size());
            return dialog.rect().contains(geometry);
        };
        require(isInsideDialog(saveProfile) && isInsideDialog(activate) &&
                isInsideDialog(cancel),
            "platform manager command buttons are clipped at minimum size");
        require(saveProfile->width() >= 130 && activate->width() >= 130 &&
                cancel->width() >= 130,
            "platform manager command buttons collapse at minimum size");
        const QRect availableGeometry = dialog.screen()->availableGeometry();
        const auto isInsideScreen = [&availableGeometry](const QWidget* widget) {
            return availableGeometry.contains(
                QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size()));
        };
        require(isInsideScreen(saveProfile) && isInsideScreen(activate) &&
                isInsideScreen(cancel),
            "platform manager command buttons are outside the screen available geometry");
        require(footer != nullptr && footer->isVisible() &&
                footer->height() >= footer->minimumHeight() &&
                isInsideDialog(footer),
            "platform manager fixed footer is missing or clipped");
        require(runtimeIdentity != nullptr && runtimeIdentity->isVisible() &&
                runtimeIdentity->text().contains(QCoreApplication::applicationFilePath()),
            "platform dialog does not identify the running executable");
        require(runningProfile != nullptr && runningProfile->isVisible() &&
                runningProfile->text().contains(QStringLiteral("base-robot")),
            "platform dialog does not identify the running Product Profile");
        require(workbenchTree != nullptr && workbenchTree->columnCount() == 2,
            "platform manager does not use the simplified Workbench editor");
        require(workbenchTree->alternatingRowColors(),
            "platform manager Workbench rows do not use alternating colors");
        require(workbenchTree->header()->sectionResizeMode(0) == QHeaderView::Interactive &&
                workbenchTree->header()->sectionResizeMode(1) == QHeaderView::Stretch,
            "platform manager Workbench columns do not support an interactive divider");
        const int originalWorkbenchColumnWidth = workbenchTree->columnWidth(0);
        workbenchTree->setColumnWidth(0, originalWorkbenchColumnWidth + 40);
        application.processEvents();
        require(workbenchTree->columnWidth(0) > originalWorkbenchColumnWidth,
            "platform manager Workbench column divider cannot be adjusted");
        require(workbenchTree->headerItem()->text(0) == QStringLiteral("Workbench"),
            "platform dialog Workbench header is missing");
        require(workbenchTree->topLevelItemCount() == catalog.workbenches().size(),
            "profile editor did not expose every catalog Workbench");

        const QList<QCheckBox*> workbenchChecks =
            workbenchTree->findChildren<QCheckBox*>(
                QStringLiteral("productProfileWorkbenchEnabledCheck"));
        const QList<QRadioButton*> defaultWorkbenchRadios =
            workbenchTree->findChildren<QRadioButton*>(
                QStringLiteral("productProfileDefaultWorkbenchRadio"));
        require(workbenchChecks.size() == catalog.workbenches().size(),
            "profile editor Workbench checkbox count is incorrect");
        require(defaultWorkbenchRadios.size() == catalog.workbenches().size(),
            "profile editor default Workbench radio count is incorrect");
        for(int index = 0; index < workbenchTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = workbenchTree->topLevelItem(index);
            QWidget* cell = workbenchTree->itemWidget(item, 0);
            require(cell != nullptr && cell->testAttribute(Qt::WA_TranslucentBackground),
                "Workbench cell hides the alternating row background");
            require(item->sizeHint(0).height() == item->sizeHint(1).height() &&
                    item->sizeHint(0).height() ==
                        workbenchTree->visualItemRect(item).height(),
                "Workbench columns do not share one row height");
        }
        QSet<QString> workbenchIds;
        QSet<QString> workbenchNames;
        QHash<QString, QCheckBox*> checksById;
        for(QCheckBox* check : workbenchChecks) {
            const QString workbenchId = check->property("workbenchId").toString();
            require(!workbenchId.isEmpty() && !workbenchIds.contains(workbenchId),
                "profile editor contains a missing or duplicate Workbench ID");
            require(!check->text().isEmpty() && !workbenchNames.contains(check->text()),
                "profile editor contains a missing or duplicate Workbench name");
            workbenchIds.insert(workbenchId);
            workbenchNames.insert(check->text());
            checksById.insert(workbenchId, check);
        }
        require(workbenchNames.contains(QStringLiteral("Project Assembly")) &&
                workbenchNames.contains(QStringLiteral("Tool Setup")),
            "Project Assembly and Tool Setup are not distinct Workbenches");
        const auto checkById = [workbenchTree](const QString& workbenchId) {
            for(int index = 0; index < workbenchTree->topLevelItemCount(); ++index) {
                QTreeWidgetItem* item = workbenchTree->topLevelItem(index);
                QWidget* cell = workbenchTree->itemWidget(item, 0);
                QCheckBox* check = cell == nullptr
                    ? nullptr
                    : cell->findChild<QCheckBox*>(
                        QStringLiteral("productProfileWorkbenchEnabledCheck"));
                if(check != nullptr &&
                    check->property("workbenchId").toString() == workbenchId) {
                    return check;
                }
            }
            return static_cast<QCheckBox*>(nullptr);
        };
        QCheckBox* projectAssemblyCheck = checkById(projectAssemblyId);
        require(projectAssemblyCheck != nullptr &&
                projectAssemblyCheck->isChecked(),
            "Profile Workbench is not projected as selected");
        int checkedDefaultCount = 0;
        for(QRadioButton* radio : defaultWorkbenchRadios) {
            const QString workbenchId = radio->property("workbenchId").toString();
            QCheckBox* check = checksById.value(workbenchId, nullptr);
            require(check != nullptr,
                "default Workbench radio does not belong to a Workbench");
            require(radio->isHidden() != check->isChecked(),
                "default Workbench radio visibility does not match Workbench enablement");
            if(radio->isChecked()) {
                ++checkedDefaultCount;
                require(check->isChecked(),
                    "default Workbench is not enabled");
            }
        }
        require(checkedDefaultCount == 1,
            "profile editor must select exactly one default Workbench");

        QToolButton* newProfile = nullptr;
        for(QToolButton* button : dialog.findChildren<QToolButton*>()) {
            if(button->toolTip() == QStringLiteral("New Product Profile")) {
                newProfile = button;
                break;
            }
        }
        require(newProfile != nullptr && newProfile->isVisible(),
            "platform manager New Product Profile command is missing");
        auto* deleteProfile =
            dialog.findChild<QToolButton*>(QStringLiteral("deleteProductProfileButton"));
        require(deleteProfile != nullptr && deleteProfile->isVisible() &&
                !deleteProfile->isEnabled(),
            "platform manager Delete command is missing or enabled for the active profile");
        newProfile->click();
        application.processEvents();
        require(deleteProfile->isEnabled(),
            "new non-active Product Profile cannot be deleted");
        require(saveProfile->isEnabled(), "new Product Profile cannot be saved");
        require(activate->text() == QStringLiteral("Switch and Restart"),
            "different Product Profile does not offer Switch and Restart");
        const QString collisionId =
            robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Collision);
        QCheckBox* collisionCheck = checkById(collisionId);
        require(collisionCheck != nullptr && !collisionCheck->isChecked(),
            "new Product Profile unexpectedly includes Collision");
        QTreeWidgetItem* collisionItem = nullptr;
        for(int index = 0; index < workbenchTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = workbenchTree->topLevelItem(index);
            if(item->data(0, Qt::UserRole).toString() == collisionId) {
                collisionItem = item;
                break;
            }
        }
        require(collisionItem != nullptr, "Collision Workbench row is missing");
        const int compactCollisionHeight = collisionItem->sizeHint(0).height();
        collisionCheck->setChecked(true);
        application.processEvents();
        require(collisionItem->sizeHint(0).height() > compactCollisionHeight &&
                collisionItem->sizeHint(0).height() == collisionItem->sizeHint(1).height(),
            "enabling a Workbench does not expand and align its row");
        collisionCheck->setChecked(false);
        application.processEvents();
        require(collisionItem->sizeHint(0).height() == compactCollisionHeight &&
                collisionItem->sizeHint(0).height() == collisionItem->sizeHint(1).height(),
            "disabling a Workbench does not restore a compact aligned row");
        saveProfile->click();
        application.processEvents();
        const std::filesystem::path savedProfilePath =
            root / "profiles" / "product-profile.platform.json";
        require(std::filesystem::exists(savedProfilePath),
            "Save Profile did not create a platform.json file");
        RobotQtViewerPlatformProfile savedProfile;
        QString loadError;
        require(RobotQtViewerPlatformProfileIo::loadProfile(
                    savedProfilePath, &savedProfile, &loadError),
            "saved Product Profile could not be loaded");
        require(savedProfile.id == QStringLiteral("product-profile") &&
                savedProfile.workbenchIds.contains(
                    robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse)),
            "saved Product Profile content is incorrect");
        const RobotQtViewerResolvedPlatformComposition savedComposition =
            RobotQtViewerPlatformProfileResolver::resolve(catalog, savedProfile);
        require(savedComposition.succeeded() &&
                !savedComposition.enabledWorkbenchIds.contains(collisionId),
            "Workbench shown as Excluded is still present in runtime composition");
        require(dialog.result() != QDialog::Accepted,
            "Save Profile unexpectedly activated or closed the dialog");

        activate->click();
        application.processEvents();
        require(dialog.result() == QDialog::Accepted,
            "Activate and Restart did not accept the dialog");
        require(dialog.configurationChanged(),
            "new Product Profile was not reported as an activation change");

        RobotQtViewerPlatformConfigurationDialog existingProfileDialog(
            catalog,
            root / "profiles",
            QStringLiteral("base-robot"));
        existingProfileDialog.show();
        application.processEvents();
        auto* profileCombo = existingProfileDialog.findChild<QComboBox*>(
            QStringLiteral("productProfileCombo"));
        auto* existingActivate = existingProfileDialog.findChild<QPushButton*>(
            QStringLiteral("switchProductProfileButton"));
        require(profileCombo != nullptr && existingActivate != nullptr,
            "existing Product Profile switch controls are missing");
        const int mobileIndex = profileCombo->findData(QStringLiteral("mobile-robot"));
        require(mobileIndex >= 0, "alternate Product Profile is not listed");
        profileCombo->setCurrentIndex(mobileIndex);
        application.processEvents();
        require(existingProfileDialog.selectedProfileId() == QStringLiteral("mobile-robot"),
            "selecting an existing Product Profile did not update the target ID");
        require(existingActivate->text() == QStringLiteral("Switch and Restart"),
            "existing Product Profile selection does not offer Switch and Restart");
        existingActivate->click();
        application.processEvents();
        require(existingProfileDialog.result() == QDialog::Accepted &&
                existingProfileDialog.configurationChanged() &&
                existingProfileDialog.selectedProfileId() == QStringLiteral("mobile-robot"),
            "existing Product Profile activation did not preserve the selected target");

        RobotQtViewerPlatformConfigurationDialog canceledDialog(
            catalog,
            root / "profiles",
            QStringLiteral("base-robot"));
        canceledDialog.show();
        application.processEvents();
        auto* canceledButton = canceledDialog.findChild<QPushButton*>(
            QStringLiteral("cancelProductProfileButton"));
        require(canceledButton != nullptr && canceledButton->isVisible(),
            "cancel dialog has no Cancel button");
        QToolButton* canceledNewProfile = nullptr;
        for(QToolButton* button : canceledDialog.findChildren<QToolButton*>()) {
            if(button->toolTip() == QStringLiteral("New Product Profile")) {
                canceledNewProfile = button;
                break;
            }
        }
        require(canceledNewProfile != nullptr, "cancel dialog has no New Profile command");
        canceledNewProfile->click();
        application.processEvents();
        canceledButton->click();
        application.processEvents();
        require(canceledDialog.result() == QDialog::Rejected,
            "platform dialog Cancel did not reject the dialog");
        require(!std::filesystem::exists(
                    root / "profiles" / "product-profile-2.platform.json"),
            "platform dialog Cancel saved a new Product Profile");
    }
}

int main(int argc, char** argv)
{
    try {
        QApplication application(argc, argv);
        requireThemeResources();
        verifyTheme(application, robot_qt_viewer::ThemeKind::Modern);
        verifyTheme(application, robot_qt_viewer::ThemeKind::Dark);
        verifyTheme(application, robot_qt_viewer::ThemeKind::Light);
        verifyToolSetupCommandBar(application);
        verifyRobotRunCollisionResults(application);
        verifyCameraPreviewLocalResize(application);
        verifyPlatformConfigurationDialog(application);
        std::cout << "RobotQtViewer theme state smoke passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "RobotQtViewer theme state smoke failed: " << error.what() << '\n';
        return 1;
    }
}
