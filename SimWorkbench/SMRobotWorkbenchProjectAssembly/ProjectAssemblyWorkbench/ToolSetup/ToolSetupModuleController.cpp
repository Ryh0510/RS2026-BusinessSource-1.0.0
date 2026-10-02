#include "ToolSetupModuleController.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPreviewState.h"
#include "RobotQtWidgetUtils.h"
#include "ToolAttachmentCommandController.h"
#include "ToolSetupAppServices.h"
#include "ToolSetupTaskControllers.h"
#include "ToolSetupViewModelBuilder.h"
#include "ToolSetupWidget.h"

#include <SimulationProject/ProjectDocument.h>
#include <SimulationProject/ProjectAttachmentCommands.h>
#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectSession.h>

#include <CustomLog/CustomLog.h>

#include <Eigen/Geometry>

#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <RobotQtViewerFileDialog.h>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <vector>

namespace
{
    class EntitySelectionDialog final : public QDialog
    {
    public:
        EntitySelectionDialog(
            const QString& title,
            const QString& fieldLabel,
            const QVector<QPair<QString, QString>>& items,
            const QString& selectedId,
            QWidget* parent)
            : QDialog(parent)
        {
            setWindowTitle(title);
            setMinimumWidth(520);
            auto* root = new QVBoxLayout(this);
            auto* form = new QFormLayout();
            m_combo = new QComboBox(this);
            robot_qt_viewer::configureInspectorEntityCombo(m_combo, 24);
            for(const auto& item : items) {
                m_combo->addItem(item.first, item.second);
                m_combo->setItemData(m_combo->count() - 1, item.first, Qt::ToolTipRole);
            }
            const int selectedIndex = m_combo->findData(selectedId);
            if(selectedIndex >= 0) {
                m_combo->setCurrentIndex(selectedIndex);
            }
            form->addRow(fieldLabel, m_combo);
            root->addLayout(form);

            auto* buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
            robot_qt_viewer::configureDialogButtonBox(buttons);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
            root->addWidget(buttons);
        }

        QString selectedId() const
        {
            return m_combo != nullptr ? m_combo->currentData().toString() : QString();
        }

    private:
        QComboBox* m_combo = nullptr;
    };

    const simulation_project::RobotMountDesc* findRobotMountDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& mountId)
    {
        const auto it = std::find_if(
            document.robotMounts.begin(),
            document.robotMounts.end(),
            [&](const simulation_project::RobotMountDesc& mount) {
                return mount.id == mountId;
            });
        return it == document.robotMounts.end() ? nullptr : &(*it);
    }

    const simulation_project::MountedAttachmentDesc* findMountedAttachmentDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& attachmentId)
    {
        const auto it = std::find_if(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& attachment) {
                return attachment.id == attachmentId;
            });
        return it == document.mountedAttachments.end() ? nullptr : &(*it);
    }

    const simulation_project::RobotDesc* findRobotDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& robotId)
    {
        const auto it = std::find_if(
            document.robots.begin(),
            document.robots.end(),
            [&](const simulation_project::RobotDesc& robot) {
                return robot.id == robotId;
            });
        return it == document.robots.end() ? nullptr : &(*it);
    }

    const simulation_project::SceneObjectDesc* findSceneObjectDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& objectId)
    {
        const auto it = std::find_if(
            document.objects.begin(),
            document.objects.end(),
            [&](const simulation_project::SceneObjectDesc& object) {
                return object.id == objectId;
            });
        return it == document.objects.end() ? nullptr : &(*it);
    }

    const simulation_project::AttachmentAssetDesc* findAttachmentAssetDesc(
        const simulation_project::ProjectDocument& document,
        const std::string& assetId)
    {
        const auto it = std::find_if(
            document.attachmentAssets.begin(),
            document.attachmentAssets.end(),
            [&](const simulation_project::AttachmentAssetDesc& asset) {
                return asset.id == assetId;
            });
        return it == document.attachmentAssets.end() ? nullptr : &(*it);
    }

    const simulation_project::MountedAttachmentDesc* findMountedAttachmentByAssetId(
        const simulation_project::ProjectDocument& document,
        const std::string& assetId)
    {
        const auto it = std::find_if(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& attachment) {
                return attachment.assetId == assetId;
            });
        return it == document.mountedAttachments.end() ? nullptr : &(*it);
    }

    const simulation_project::MountedAttachmentDesc* findMountedAttachmentByMountId(
        const simulation_project::ProjectDocument& document,
        const std::string& mountId)
    {
        const auto it = std::find_if(
            document.mountedAttachments.begin(),
            document.mountedAttachments.end(),
            [&](const simulation_project::MountedAttachmentDesc& attachment) {
                return attachment.mountFrameId == mountId;
            });
        return it == document.mountedAttachments.end() ? nullptr : &(*it);
    }

    const simulation_project::ObjectFrameDesc* findObjectFrameDesc(
        const simulation_project::SceneObjectDesc& object,
        const std::string& frameId)
    {
        const auto it = std::find_if(
            object.objectFrames.begin(),
            object.objectFrames.end(),
            [&](const simulation_project::ObjectFrameDesc& frame) {
                return frame.id == frameId;
            });
        return it == object.objectFrames.end() ? nullptr : &(*it);
    }

    bool nearlyEqual(double lhs, double rhs)
    {
        return std::abs(lhs - rhs) <= 1.0e-9;
    }

    bool sameTransform(
        const simulation_project::TransformDesc& lhs,
        const simulation_project::TransformDesc& rhs)
    {
        return nearlyEqual(lhs.x, rhs.x) &&
            nearlyEqual(lhs.y, rhs.y) &&
            nearlyEqual(lhs.z, rhs.z) &&
            nearlyEqual(lhs.roll, rhs.roll) &&
            nearlyEqual(lhs.pitch, rhs.pitch) &&
            nearlyEqual(lhs.yaw, rhs.yaw);
    }

    bool sameRobotMount(
        const simulation_project::RobotMountDesc& lhs,
        const simulation_project::RobotMountDesc& rhs)
    {
        return lhs.id == rhs.id &&
            lhs.name == rhs.name &&
            lhs.robotId == rhs.robotId &&
            lhs.linkName == rhs.linkName &&
            sameTransform(lhs.linkToMount, rhs.linkToMount);
    }

    std::string findHiddenSceneObjectIdBySourcePath(
        const simulation_project::ProjectDocument& document,
        const std::string& sourcePath)
    {
        if(sourcePath.empty()) {
            return std::string();
        }
        const auto it = std::find_if(
            document.objects.begin(),
            document.objects.end(),
            [&](const simulation_project::SceneObjectDesc& object) {
                return !object.visible && object.sourcePath == sourcePath;
            });
        return it == document.objects.end() ? std::string() : it->id;
    }

    Eigen::Isometry3d makeTransform(const simulation_project::TransformDesc& desc)
    {
        Eigen::Isometry3d transform = Eigen::Isometry3d::Identity();
        transform.translation() = Eigen::Vector3d(desc.x, desc.y, desc.z);
        transform.linear() =
            Eigen::AngleAxisd(desc.yaw, Eigen::Vector3d::UnitZ()).toRotationMatrix() *
            Eigen::AngleAxisd(desc.pitch, Eigen::Vector3d::UnitY()).toRotationMatrix() *
            Eigen::AngleAxisd(desc.roll, Eigen::Vector3d::UnitX()).toRotationMatrix();
        return transform;
    }

    simulation_project::TransformDesc makeTransformDesc(const Eigen::Isometry3d& transform)
    {
        simulation_project::TransformDesc desc;
        desc.x = transform.translation().x();
        desc.y = transform.translation().y();
        desc.z = transform.translation().z();
        const Eigen::Vector3d euler = transform.linear().eulerAngles(2, 1, 0);
        desc.yaw = euler[0];
        desc.pitch = euler[1];
        desc.roll = euler[2];
        return desc;
    }

    simulation_project::TransformDesc inverseTransform(
        const simulation_project::TransformDesc& transform)
    {
        return makeTransformDesc(makeTransform(transform).inverse());
    }

    QString formatMatrixValue(double value)
    {
        return QString::number(value, 'f', 3);
    }

    QString formatTransformMatrix(const simulation_project::TransformDesc& transform)
    {
        const Eigen::Matrix4d matrix = makeTransform(transform).matrix();
        QStringList rows;
        for(int row = 0; row < 4; ++row) {
            QStringList values;
            for(int column = 0; column < 4; ++column) {
                values.push_back(formatMatrixValue(matrix(row, column)));
            }
            rows.push_back(QStringLiteral("[ %1 ]").arg(values.join(QStringLiteral("  "))));
        }
        return rows.join(QLatin1Char('\n'));
    }

    ToolSetupBindingView makeBindingView(
        const simulation_project::ProjectDocument& document,
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        ToolSetupBindingView view;
        const simulation_project::RobotMountDesc* mount =
            findRobotMountDesc(document, mountId.toStdString());
        const simulation_project::SceneObjectDesc* object =
            findSceneObjectDesc(document, objectId.toStdString());
        view.visible = mount != nullptr;
        if(mount != nullptr) {
            view.mountLinkName = QString::fromStdString(mount->linkName);
            view.mountFrameName = QString::fromStdString(mount->name.empty() ? mount->id : mount->name);
            view.mountTransformText = formatTransformMatrix(mount->linkToMount);
        }
        if(object != nullptr) {
            view.hasObjectBinding = true;
            const std::string objectName = object->name.empty() ? object->id : object->name;
            const std::string mountName =
                mount != nullptr ? (mount->name.empty() ? mount->id : mount->name) : std::string();
            view.objectName = QString::fromStdString(objectName);
            view.bindingName = QString::fromStdString(
                objectName.empty()
                    ? (mountName.empty() ? std::string("Attachment") : mountName + " Attachment")
                    : objectName);
            if(frameId.isEmpty()) {
                view.objectFrameName = QStringLiteral("Object origin");
                view.objectFrameInverseTransformText =
                    formatTransformMatrix(simulation_project::TransformDesc());
            } else {
                const simulation_project::ObjectFrameDesc* frame =
                    findObjectFrameDesc(*object, frameId.toStdString());
                if(frame != nullptr) {
                    view.objectFrameName = QString::fromStdString(frame->name.empty() ? frame->id : frame->name);
                    view.objectFrameInverseTransformText = formatTransformMatrix(inverseTransform(frame->objectToFrame));
                }
            }
        } else {
            view.bindingName = QStringLiteral("Pending binding");
            view.hasObjectBinding = false;
            view.objectFrameName = QStringLiteral("Object Frame");
            view.objectName = QStringLiteral("Object");
            view.objectFrameInverseTransformText =
                formatTransformMatrix(simulation_project::TransformDesc());
        }
        return view;
    }

    std::string qStringToUtf8(const QString& text)
    {
        const QByteArray bytes = text.toUtf8();
        return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
    }

    std::string trimUnderscores(std::string value)
    {
        while(!value.empty() && value.front() == '_') {
            value.erase(value.begin());
        }
        while(!value.empty() && value.back() == '_') {
            value.pop_back();
        }
        return value;
    }

    std::string makeAsciiSlug(
        const std::string& displayName,
        const std::string& fallbackPrefix,
        std::size_t maxLength)
    {
        std::string slug;
        slug.reserve(displayName.size());
        bool lastWasSeparator = false;
        for(const unsigned char ch : displayName) {
            if(ch < 128 && std::isalnum(ch)) {
                slug.push_back(static_cast<char>(std::tolower(ch)));
                lastWasSeparator = false;
            } else if(ch < 128 && (ch == '_' || ch == '-' || ch == '.' || std::isspace(ch))) {
                if(!slug.empty() && !lastWasSeparator) {
                    slug.push_back('_');
                    lastWasSeparator = true;
                }
            }
        }

        slug = trimUnderscores(slug);
        if(slug.empty()) {
            slug = fallbackPrefix;
        }
        if(slug.size() > maxLength) {
            slug.resize(maxLength);
            slug = trimUnderscores(slug);
        }
        return slug.empty() ? fallbackPrefix : slug;
    }

    std::string makeToolAssetDisplayName(
        const std::string& sourceName,
        const std::string& fallbackName)
    {
        return sourceName.empty() ? fallbackName : sourceName;
    }

    std::string makeAttachmentDisplayName(
        const std::string& assetName,
        const std::string& mountName)
    {
        if(!assetName.empty()) {
            return assetName;
        }
        return mountName.empty() ? std::string("Attachment") : mountName + " Attachment";
    }

    QString conciseUiText(const std::string& value, int maxCharacters)
    {
        QString text = QString::fromStdString(value);
        if(maxCharacters <= 8 || text.size() <= maxCharacters) {
            return text;
        }

        const int headCount = (maxCharacters - 3) / 2;
        const int tailCount = maxCharacters - 3 - headCount;
        return text.left(headCount) + "..." + text.right(tailCount);
    }

    simulation_project::MountedAttachmentDesc makeMountedAttachmentMirror(
        const simulation_project::MountedAttachmentDesc& attachment)
    {
        simulation_project::MountedAttachmentDesc value;
        value.id = attachment.id;
        value.name = attachment.name;
        value.mountFrameId = attachment.mountFrameId;
        value.assetId = attachment.assetId;
        value.sourceObjectId = attachment.sourceObjectId;
        value.sourceObjectFrameId = attachment.sourceObjectFrameId;
        value.mountToAssetMount = attachment.mountToAssetMount;
        value.visible = attachment.visible;
        value.enabled = attachment.enabled;
        return value;
    }

    ToolSetupComboItem makeComboItem(const QString& text, const QString& id)
    {
        ToolSetupComboItem item;
        item.text = text;
        item.id = id;
        return item;
    }

}

namespace robot_qt_viewer
{
    ToolSetupModuleController::ToolSetupModuleController(
        ToolSetupWidget& widget,
        RobotQtViewerDocumentContext& context,
        ToolSetupAppServices& appServices,
        QObject* parent)
        : QObject(parent)
        , m_widget(widget)
        , m_context(context)
        , m_appServices(appServices)
    {
        m_attachmentDefinitionTask =
            new ToolSetupAttachmentDefinitionTaskController(*this, this);
        m_installedDeviceTask = new ToolSetupInstalledDeviceTaskController(*this, this);
        m_mountFrameTask = new ToolSetupMountFrameTaskController(*this, this);
        m_objectBindingTask = new ToolSetupObjectBindingTaskController(*this, this);
        m_taskSession = new ToolSetupTaskSessionCoordinator(*this, this);
        connect(&m_widget, &ToolSetupWidget::mountSelectionChanged,
            this, &ToolSetupModuleController::handleMountSelectionChanged);
        connect(&m_widget, &ToolSetupWidget::attachmentSelectionChanged,
            this, &ToolSetupModuleController::handleAttachmentSelectionChanged);
        connect(&m_widget, &ToolSetupWidget::addRobotMountRequested,
            this, &ToolSetupModuleController::createRobotMountForSelectedLink);
        connect(&m_widget, &ToolSetupWidget::deleteRobotMountRequested,
            this, &ToolSetupModuleController::deleteCurrentRobotMount);
        connect(&m_widget, &ToolSetupWidget::configureAttachmentRequested,
            this, &ToolSetupModuleController::configureCurrentToolAttachment);
        connect(&m_widget, &ToolSetupWidget::importToolAssetRequested,
            this, &ToolSetupModuleController::importToolAsset);
        connect(&m_widget, &ToolSetupWidget::attachToolAssetRequested,
            this, &ToolSetupModuleController::attachExistingToolAsset);
        connect(&m_widget, &ToolSetupWidget::assetSelectionChanged,
            this, &ToolSetupModuleController::handleAssetSelectionChanged);
        connect(&m_widget, &ToolSetupWidget::editToolAssetRequested,
            this, &ToolSetupModuleController::editCurrentToolAsset);
        connect(&m_widget, &ToolSetupWidget::duplicateAssetRequested,
            this, &ToolSetupModuleController::duplicateCurrentAsset);
        connect(&m_widget, &ToolSetupWidget::rebindAttachmentRequested,
            this, &ToolSetupModuleController::rebindCurrentAttachment);
        connect(&m_widget, &ToolSetupWidget::objectBindingSelectionChanged,
            this, &ToolSetupModuleController::handleObjectBindingSelectionChanged);
        connect(&m_widget, &ToolSetupWidget::objectBindingApplyRequested,
            this, &ToolSetupModuleController::applyObjectBinding);
        connect(&m_widget, &ToolSetupWidget::taskDirtyChanged,
            this, [this](bool dirty) {
                setTaskDirty(dirty);
            });
        connect(&m_widget, &ToolSetupWidget::taskApplyRequested,
            this, &ToolSetupModuleController::applyTaskChanges);
        connect(&m_widget, &ToolSetupWidget::taskCancelRequested,
            this, &ToolSetupModuleController::cancelTaskChanges);
        connect(&m_widget, &ToolSetupWidget::taskExitRequested,
            this, &ToolSetupModuleController::requestTaskExit);
        connect(&m_widget, &ToolSetupWidget::mountTransformPreviewChanged,
            this, [this](const simulation_project::TransformDesc& transform) {
                const QString mountId = m_widget.currentMountId();
                if(mountId.isEmpty()) {
                    return;
                }
                RobotQtViewerViewportPreviewPayload preview;
                preview.previewRobotMountTransform = true;
                preview.previewRobotMountId = mountId;
                preview.robotMountTransform = transform;
                preview.setActivePreviewRobotMount = true;
                preview.activePreviewRobotMountId = mountId;
                mutateViewportPreview(preview, QStringLiteral("toolSetupMountTransformPreview"));
            });
        connect(&m_widget, &ToolSetupWidget::attachmentOffsetPreviewChanged,
            this, [this](const simulation_project::TransformDesc& transform) {
                const QString attachmentId = m_widget.currentAttachmentId();
                if(attachmentId.isEmpty()) {
                    return;
                }
                RobotQtViewerViewportPreviewPayload preview;
                preview.previewMountedAttachmentTransform = true;
                preview.previewMountedAttachmentId = attachmentId;
                preview.mountedAttachmentTransform = transform;
                mutateViewportPreview(preview, QStringLiteral("toolSetupAttachmentOffsetPreview"));
            });
        connect(&m_widget, &ToolSetupWidget::attachmentAssetPreviewChanged,
            this, [this](const simulation_project::AttachmentAssetDesc& asset) {
                if(asset.id.empty()) {
                    return;
                }
                RobotQtViewerViewportPreviewPayload preview;
                preview.previewAttachmentAsset = true;
                preview.attachmentAsset = asset;
                mutateViewportPreview(preview, QStringLiteral("toolSetupAttachmentAssetPreview"));
            });
        connect(&m_widget, &ToolSetupWidget::frameVisibilityChanged,
            this, &ToolSetupModuleController::updateToolFrameVisibility);
    }

    void ToolSetupModuleController::refresh(
        const QString& selectedRobotId,
        const QString& selectedLinkName,
        const QString& preferredMountId,
        const QString& preferredAttachmentId,
        const QString& preferredAssetId)
    {
        m_updating = true;
        const QString previousMountId = m_widget.currentMountId();
        const QString previousAttachmentId = m_widget.currentAttachmentId();
        const QString previousAssetId = m_widget.currentAssetId();
        ToolSetupPanelView view = buildToolSetupPanelView(
            m_context.document(),
            selectedRobotId,
            selectedLinkName,
            preferredMountId,
            previousMountId,
            preferredAttachmentId,
            previousAttachmentId,
            preferredAssetId,
            previousAssetId);
        if(m_mountFrameTask->snapshotIsNew() && m_mountFrameTask->hasSnapshot()) {
            const simulation_project::RobotMountDesc& draft = m_mountFrameTask->snapshot();
            const QString draftMountId = QString::fromStdString(draft.id);
            view.mountItems.push_back(makeComboItem(draftMountId, draftMountId));
            view.selectedMountId = draftMountId;
            view.mountDetails = QStringLiteral("New mount frame draft");
            view.mountFrameName = draftMountId;
            view.selectedMountLinkName = QString::fromStdString(draft.linkName);
            view.mountTransform = draft.linkToMount;
            view.mountItemsEnabled = false;
            view.deleteMountEnabled = false;
        }
        view.mountFrameMode = m_objectBindingTask->active()
            ? ToolSetupMountFrameMode::Selection
            : m_mountFrameTask->mode();
        if(view.mountFrameMode == ToolSetupMountFrameMode::Create ||
            view.mountFrameMode == ToolSetupMountFrameMode::Edit) {
            view.mountFrameNameEditorVisible = true;
            view.mountFrameNameEditorEnabled = true;
            view.mountTransformEditorVisible = true;
            view.mountTransformEditorEnabled = true;
            if(view.mountTransformEditorTitle.isEmpty()) {
                view.mountTransformEditorTitle = QStringLiteral("Frame Transform");
            }
        }
        view.robotMountFramePinnedEnabled = !view.selectedMountId.isEmpty();
        view.robotMountFramePinned =
            view.robotMountFramePinnedEnabled &&
            m_mountFrameTask->isPinned(view.selectedMountId);
        m_widget.setDocumentView(view);
        m_updating = false;
        if(!m_taskSession->dirty() && !m_objectBindingTask->active() &&
            !m_mountFrameTask->snapshotIsNew()) {
            captureMountEditSnapshot(m_widget.currentMountId());
        }
        syncPinnedRobotMountFrames();
        emit viewModelRefreshed();
    }

    void ToolSetupModuleController::handleEvent(
        const RobotQtViewerEvent& event,
        const QString& selectedRobotId,
        const QString& selectedLinkName)
    {
        if(m_objectBindingTask->active()) {
            if(event.kind == RobotQtViewerEventKind::ViewportReloaded ||
                event.kind == RobotQtViewerEventKind::SelectionChanged ||
                event.kind == RobotQtViewerEventKind::ProjectDocumentChanged) {
                m_objectBindingTask->refreshEditor(
                    m_objectBindingTask->mountId(),
                    m_objectBindingTask->objectId(),
                    m_objectBindingTask->frameId());
            }
            return;
        }

        switch(event.kind) {
        case RobotQtViewerEventKind::SelectionChanged:
            refresh(
                selectedRobotId,
                selectedLinkName,
                event.selection.mountId,
                event.selection.attachmentId,
                event.selection.assetId);
            break;
        case RobotQtViewerEventKind::AttachmentChanged:
            refresh(
                selectedRobotId,
                selectedLinkName,
                event.attachment.mountId,
                event.attachment.attachmentId,
                event.attachment.assetId);
            break;
        case RobotQtViewerEventKind::ProjectDocumentChanged:
        case RobotQtViewerEventKind::ViewportReloaded:
            refresh(selectedRobotId, selectedLinkName);
            break;
        default:
            break;
        }
    }

    void ToolSetupModuleController::handleMountSelectionChanged(int index)
    {
        m_mountFrameTask->handleSelectionChanged(index);
    }

    void ToolSetupModuleController::handleAttachmentSelectionChanged(int index)
    {
        m_installedDeviceTask->handleSelectionChanged(index);
    }

    void ToolSetupModuleController::handleAssetSelectionChanged(int index)
    {
        m_attachmentDefinitionTask->handleSelectionChanged(index);
    }

    void ToolSetupModuleController::focusRobotMountTask(
        const QString& robotId,
        const QString& linkName,
        const QString& preferredMountId)
    {
        m_mountFrameTask->focusTask(robotId, linkName, preferredMountId);
    }

    void ToolSetupModuleController::importToolAsset()
    {
        m_attachmentDefinitionTask->importAsset();
    }

    void ToolSetupModuleController::attachExistingToolAsset()
    {
        m_installedDeviceTask->attachExistingAsset();
    }

    void ToolSetupModuleController::duplicateCurrentAsset()
    {
        m_attachmentDefinitionTask->duplicateCurrent();
    }

    void ToolSetupModuleController::rebindCurrentAttachment()
    {
        m_installedDeviceTask->rebindCurrent();
    }

    void ToolSetupModuleController::createRobotMountForSelectedLink()
    {
        m_mountFrameTask->createForSelectedLink();
    }

    void ToolSetupModuleController::deleteCurrentRobotMount()
    {
        m_mountFrameTask->deleteCurrent();
    }

    bool ToolSetupModuleController::hasPendingTaskChanges() const
    {
        return m_taskSession->dirty();
    }

    bool ToolSetupModuleController::resolvePendingTaskChanges(QWidget* parentWidget, bool restoreEditorTarget)
    {
        return m_taskSession->resolvePendingChanges(parentWidget, restoreEditorTarget);
    }

    void ToolSetupModuleController::releaseProjectState() noexcept
    {
        m_taskSession->releaseProjectState();
    }

    bool ToolSetupModuleController::applyPendingTaskChanges()
    {
        return m_taskSession->applyPendingChanges();
    }

    void ToolSetupModuleController::discardPendingTaskChanges(const QString& message, bool restoreEditorTarget)
    {
        m_taskSession->discardPendingChanges(message, restoreEditorTarget);
    }

    void ToolSetupModuleController::configureCurrentToolAttachment()
    {
        m_installedDeviceTask->configureCurrent();
    }

    void ToolSetupModuleController::applyTaskChanges(
        bool hasMountTransform,
        const simulation_project::TransformDesc& mountTransform,
        bool hasAttachmentInstance,
        const simulation_project::MountedAttachmentDesc& attachmentInstance,
        bool hasAttachmentOffset,
        const simulation_project::TransformDesc& attachmentOffset,
        bool hasToolAsset,
        const simulation_project::AttachmentAssetDesc& toolAsset)
    {
        const QString attachmentId = m_widget.currentAttachmentId();
        QString selectedMountId = m_widget.currentMountId();
        const QString selectedMountLinkName = m_widget.currentMountLinkName();
        const simulation_project::RobotMountDesc* selectedMount =
            findRobotMountDesc(m_context.document(), selectedMountId.toStdString());
        const simulation_project::MountedAttachmentDesc* attachment =
            findMountedAttachmentDesc(m_context.document(), attachmentId.toStdString());
        if(attachment == nullptr) {
            if(hasAttachmentInstance || hasAttachmentOffset) {
                emit statusMessageRequested("Select an attachment first.", 3000);
                return;
            }
            if(!hasMountTransform && !hasToolAsset) {
                emit statusMessageRequested("Select an attachment or attachment asset first.", 3000);
                return;
            }

            bool projectChanged = false;
            bool mountFrameChanged = false;
            QString appliedMountRobotId;
            QString appliedMountLinkName;
            if(hasMountTransform) {
                if(selectedMount == nullptr) {
                    emit statusMessageRequested("Select a mount frame first.", 3000);
                    return;
                }
                simulation_project::RobotMountDesc updatedMount = *selectedMount;
                const QString oldMountId = QString::fromStdString(selectedMount->id);
                const QString mountName = m_widget.currentMountFrameName();
                if(mountName.isEmpty()) {
                    emit statusMessageRequested("Frame key/name cannot be empty.", 4000);
                    return;
                }
                updatedMount.id = qStringToUtf8(mountName);
                updatedMount.name = updatedMount.id;
                if(!selectedMountLinkName.isEmpty()) {
                    updatedMount.linkName = selectedMountLinkName.toStdString();
                }
                updatedMount.linkToMount = mountTransform;
                selectedMountId = QString::fromStdString(updatedMount.id);
                const bool creatingMount = m_mountFrameTask->snapshotIsNew();
                if(creatingMount || !sameRobotMount(*selectedMount, updatedMount)) {
                    const std::string previousMountId = selectedMount->id;
                    const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
                        QStringLiteral("applyToolSetupMountTask"),
                        ProjectDirtyPolicy::UserEdit,
                        [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                            if(creatingMount
                                   ? !service.addRobotMount(updatedMount, &error)
                                   : !service.updateRobotMountFrame(previousMountId, updatedMount, &error)) {
                                return false;
                            }
                            changed = true;
                            return true;
                        });
                    if(!mutationResult.success) {
                        emit statusMessageRequested(
                            QString("Mount frame update failed: %1").arg(mutationResult.message),
                            5000);
                        return;
                    }
                    if(oldMountId != selectedMountId) {
                        m_mountFrameTask->replacePinnedId(oldMountId, selectedMountId);
                        syncPinnedRobotMountFrames();
                    }
                    RobotQtViewerViewportPreviewPayload preview;
                    if(oldMountId != selectedMountId) {
                        preview.removePreviewRobotMount = true;
                        preview.removePreviewRobotMountId = oldMountId;
                    }
                    preview.upsertPreviewRobotMount = true;
                    preview.robotMount = updatedMount;
                    preview.previewRobotMountTransform = true;
                    preview.previewRobotMountId = selectedMountId;
                    preview.robotMountTransform = updatedMount.linkToMount;
                    preview.setActivePreviewRobotMount = true;
                    preview.activePreviewRobotMountId = selectedMountId;
                    mutateViewportPreview(preview, QStringLiteral("applyTaskChanges"));
                    projectChanged = true;
                    mountFrameChanged = true;
                }
                appliedMountRobotId = QString::fromStdString(updatedMount.robotId);
                appliedMountLinkName = QString::fromStdString(updatedMount.linkName);
            }

            const QString selectedAssetId = m_widget.currentAssetId();
            const std::string assetId = selectedAssetId.toStdString();
            if(hasToolAsset) {
                if(assetId.empty()) {
                    emit statusMessageRequested("Select an attachment asset first.", 3000);
                    return;
                }

                ToolAttachmentCommandResult commandResult;
                const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
                    QStringLiteral("applyToolSetupAssetTask"),
                    ProjectDirtyPolicy::UserEdit,
                    [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                        commandResult =
                            ToolAttachmentCommandController::applyAttachmentAssetUpdate(service.document(), assetId, toolAsset);
                        if(!commandResult.success) {
                            error = commandResult.message.toStdString();
                            return false;
                        }
                        changed = commandResult.projectChanged;
                        return true;
                    });
                if(!mutationResult.success) {
                    emit statusMessageRequested(mutationResult.message, 5000);
                    return;
                }
                projectChanged = projectChanged || mutationResult.changed;
            }
            const QString appliedAssetId = QString::fromStdString(assetId);
            if(hasMountTransform && !appliedMountRobotId.isEmpty()) {
                m_appServices.setRobotMountContext(appliedMountRobotId, appliedMountLinkName, selectedMountId);
                m_context.selectionModel().selectRobotMount(
                    appliedMountRobotId,
                    appliedMountLinkName,
                    selectedMountId,
                    QStringLiteral("applyToolSetupMountTask"));
            } else if(!appliedAssetId.isEmpty()) {
                m_context.selectionModel().selectToolAsset(appliedAssetId, QStringLiteral("applyToolSetupAssetTask"));
            }
            m_mountFrameTask->setMode(ToolSetupMountFrameMode::Selection);
            refresh(
                m_appServices.selectedRobotId(),
                m_appServices.selectedLinkName(),
                selectedMountId,
                QString(),
                appliedAssetId);
            RobotQtViewerAttachmentPayload payload;
            payload.mountId = selectedMountId;
            payload.assetId = appliedAssetId;
            m_context.documentController().publishAttachmentChanged(payload, QStringLiteral("applyToolSetupAssetTask"));
            m_context.documentController().publishDocumentChanged(QStringLiteral("applyToolSetupAssetTask"), false);
            clearMountEditSnapshot();
            captureMountEditSnapshot(selectedMountId);
            setTaskDirty(false);
            if(hasMountTransform && !appliedMountRobotId.isEmpty()) {
                clearMountFrameViewportFocus();
                emit robotContextSelected(appliedMountRobotId);
                emit mountFrameFocusRequested(appliedMountRobotId, appliedMountLinkName, selectedMountId);
                emit selectionDependentViewsRefreshRequested();
            }
            if(hasMountTransform) {
                clearMountFrameViewportFocus();
                emit taskExitRequested();
            }
            emit statusMessageRequested("Mount setup changes applied.", 3000);
            return;
        }

        const std::string attachmentIdValue = attachment->id;
        const std::string mountId = attachment->mountFrameId;
        const std::string assetId = attachment->assetId;
        std::string appliedMountId = mountId;
        std::string appliedAssetId = assetId;
        bool projectChanged = false;

        if(hasMountTransform) {
            if(selectedMount == nullptr) {
                emit statusMessageRequested("Select a mount frame first.", 3000);
                return;
            }
            simulation_project::RobotMountDesc updatedMount = *selectedMount;
            const QString oldMountId = QString::fromStdString(selectedMount->id);
            const QString mountName = m_widget.currentMountFrameName();
            if(mountName.isEmpty()) {
                emit statusMessageRequested("Frame key/name cannot be empty.", 4000);
                return;
            }
            updatedMount.id = qStringToUtf8(mountName);
            updatedMount.name = updatedMount.id;
            if(!selectedMountLinkName.isEmpty()) {
                updatedMount.linkName = selectedMountLinkName.toStdString();
            }
            updatedMount.linkToMount = mountTransform;
            appliedMountId = updatedMount.id;
            selectedMountId = QString::fromStdString(updatedMount.id);
            if(!sameRobotMount(*selectedMount, updatedMount)) {
                const std::string previousMountId = selectedMount->id;
                const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
                    QStringLiteral("applyToolSetupTask"),
                    ProjectDirtyPolicy::UserEdit,
                    [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                        if(!service.updateRobotMountFrame(previousMountId, updatedMount, &error)) {
                            return false;
                        }
                        changed = true;
                        return true;
                    });
                if(!mutationResult.success) {
                    emit statusMessageRequested(
                        QString("Mount frame update failed: %1").arg(mutationResult.message),
                        5000);
                    return;
                }
                RobotQtViewerViewportPreviewPayload preview;
                if(oldMountId != selectedMountId) {
                    preview.removePreviewRobotMount = true;
                    preview.removePreviewRobotMountId = oldMountId;
                }
                preview.upsertPreviewRobotMount = true;
                preview.robotMount = updatedMount;
                preview.previewRobotMountTransform = true;
                preview.previewRobotMountId = selectedMountId;
                preview.robotMountTransform = updatedMount.linkToMount;
                preview.setActivePreviewRobotMount = true;
                preview.activePreviewRobotMountId = selectedMountId;
                mutateViewportPreview(preview, QStringLiteral("applyTaskChanges"));
                projectChanged = true;
            }
        }

        if(hasAttachmentInstance || hasAttachmentOffset) {
            simulation_project::MountedAttachmentDesc updatedAttachment =
                hasAttachmentInstance
                    ? makeMountedAttachmentMirror(attachmentInstance)
                    : makeMountedAttachmentMirror(*attachment);
            updatedAttachment.id = attachment->id;
            if(updatedAttachment.mountFrameId.empty()) {
                updatedAttachment.mountFrameId = attachment->mountFrameId;
            }
            if(updatedAttachment.assetId.empty()) {
                updatedAttachment.assetId = attachment->assetId;
            }
            if(hasMountTransform) {
                updatedAttachment.mountFrameId = appliedMountId;
            }
            if(hasAttachmentOffset) {
                updatedAttachment.mountToAssetMount = attachmentOffset;
            }
            appliedMountId = updatedAttachment.mountFrameId;
            appliedAssetId = updatedAttachment.assetId;
            ToolAttachmentCommandResult commandResult;
            const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
                QStringLiteral("applyToolSetupTask"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                    commandResult =
                        ToolAttachmentCommandController::applyMountedAttachmentUpdate(service.document(), updatedAttachment);
                    if(!commandResult.success) {
                        error = commandResult.message.toStdString();
                        return false;
                    }
                    changed = commandResult.projectChanged;
                    return true;
                });
            if(!mutationResult.success) {
                emit statusMessageRequested(mutationResult.message, 5000);
                return;
            }
            projectChanged = projectChanged || mutationResult.changed;
        }

        if(hasToolAsset) {
            ToolAttachmentCommandResult commandResult;
            const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
                QStringLiteral("applyToolSetupTask"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                    commandResult =
                        ToolAttachmentCommandController::applyAttachmentAssetUpdate(service.document(), appliedAssetId, toolAsset);
                    if(!commandResult.success) {
                        error = commandResult.message.toStdString();
                        return false;
                    }
                    changed = commandResult.projectChanged;
                    return true;
                });
            if(!mutationResult.success) {
                emit statusMessageRequested(mutationResult.message, 5000);
                return;
            }
            projectChanged = projectChanged || mutationResult.changed;
            if(!hasAttachmentInstance) {
                appliedAssetId = toolAsset.id;
            }
        }
        selectToolAttachmentById(attachmentIdValue);
        RobotQtViewerAttachmentPayload payload;
        payload.mountId = QString::fromStdString(appliedMountId);
        payload.attachmentId = QString::fromStdString(attachmentIdValue);
        payload.assetId = QString::fromStdString(appliedAssetId);
        m_context.documentController().publishAttachmentChanged(payload, QStringLiteral("applyToolSetupTask"));
        m_context.documentController().publishDocumentChanged(QStringLiteral("applyToolSetupTask"), false);
        m_mountFrameTask->setMode(ToolSetupMountFrameMode::Selection);
        clearMountEditSnapshot();
        captureMountEditSnapshot(QString::fromStdString(appliedMountId));
        setTaskDirty(false);
        if(const simulation_project::RobotMountDesc* appliedMount =
            findRobotMountDesc(m_context.document(), appliedMountId)) {
            if(hasMountTransform) {
                const QString appliedRobotId = QString::fromStdString(appliedMount->robotId);
                const QString appliedLinkName = QString::fromStdString(appliedMount->linkName);
                const QString appliedMountFrameId = QString::fromStdString(appliedMount->id);
                clearMountFrameViewportFocus();
                emit robotContextSelected(appliedRobotId);
                emit mountFrameFocusRequested(appliedRobotId, appliedLinkName, appliedMountFrameId);
                emit selectionDependentViewsRefreshRequested();
                emit taskExitRequested();
            } else {
                enterMountFrameViewportFocus(
                    QString::fromStdString(appliedMount->robotId),
                    QString::fromStdString(appliedMount->linkName));
            }
        }
        emit statusMessageRequested("Mount setup changes applied.", 3000);
    }

    void ToolSetupModuleController::cancelTaskChanges()
    {
        m_taskSession->cancelTask();
    }

    void ToolSetupModuleController::requestTaskExit()
    {
        m_taskSession->requestTaskExit();
    }

    void ToolSetupModuleController::setTaskDirty(bool dirty, const QString& message)
    {
        m_taskSession->setDirty(dirty, message);
    }

    void ToolSetupModuleController::captureMountEditSnapshot(const QString& mountId, bool newMount)
    {
        m_mountFrameTask->captureSnapshot(mountId, newMount);
    }

    void ToolSetupModuleController::clearMountEditSnapshot()
    {
        m_mountFrameTask->clearSnapshot();
    }

    void ToolSetupModuleController::enterMountFrameViewportFocus(
        const QString& robotId,
        const QString& linkName)
    {
        m_mountFrameTask->enterViewportFocus(robotId, linkName);
    }

    void ToolSetupModuleController::clearMountFrameViewportFocus()
    {
        m_mountFrameTask->clearViewportFocus();
    }

    void ToolSetupModuleController::clearObjectBindingPreviewViewportState()
    {
        m_context.viewportPreviewState().clearTaskPreview(
            QStringLiteral("clearObjectBindingPreviewViewportState"));
    }

    void ToolSetupModuleController::mutateViewportPreview(
        const RobotQtViewerViewportPreviewPayload& preview,
        const QString& sourceId)
    {
        RobotQtViewerViewportPreviewPayload mergedPreview = preview;
        if(!mergedPreview.setPinnedRobotMountFrames) {
            mergedPreview.setPinnedRobotMountFrames = true;
            mergedPreview.pinnedRobotMountFrameIds = pinnedRobotMountFrameIds();
        }
        m_context.viewportPreviewState().mutate(mergedPreview, sourceId);
    }

    void ToolSetupModuleController::selectToolAttachmentById(const std::string& attachmentId)
    {
        m_installedDeviceTask->selectById(attachmentId);
    }

    void ToolSetupModuleController::editCurrentToolAsset()
    {
        m_attachmentDefinitionTask->editCurrent();
    }

    bool ToolSetupModuleController::editToolAssetById(const std::string& assetId)
    {
        return m_attachmentDefinitionTask->editById(assetId);
    }

    void ToolSetupModuleController::createToolAssetFromSceneObject(const QString& objectId)
    {
        m_attachmentDefinitionTask->createFromSceneObject(objectId);
    }

    void ToolSetupModuleController::focusObjectBindingTask(
        const QString& preferredMountId,
        const QString& preferredObjectId,
        const QString& preferredFrameId)
    {
        m_objectBindingTask->focusTask(
            preferredMountId,
            preferredObjectId,
            preferredFrameId);
    }

    void ToolSetupModuleController::handleObjectBindingSelectionChanged(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        m_objectBindingTask->handleSelectionChanged(mountId, objectId, frameId);
    }

    bool ToolSetupModuleController::previewObjectBinding(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId,
        QString* attachmentId,
        QString* assetId)
    {
        return m_objectBindingTask->preview(
            mountId,
            objectId,
            frameId,
            attachmentId,
            assetId);
    }

    void ToolSetupModuleController::refreshObjectBindingEditor(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        m_objectBindingTask->refreshEditor(mountId, objectId, frameId);
    }

    void ToolSetupModuleController::applyObjectBinding(
        const QString& mountId,
        const QString& objectId,
        const QString& frameId)
    {
        m_objectBindingTask->apply(mountId, objectId, frameId);
    }

    void ToolSetupModuleController::unbindMountedAttachment(const QString& attachmentId)
    {
        m_installedDeviceTask->unbind(attachmentId);
    }

    void ToolSetupModuleController::updateToolFrameVisibility()
    {
        m_mountFrameTask->updateFrameVisibility();
    }

    QString ToolSetupModuleController::currentMountId() const
    {
        return m_widget.currentMountId();
    }

    QString ToolSetupModuleController::currentAttachmentId() const
    {
        return m_widget.currentAttachmentId();
    }

    void ToolSetupModuleController::syncPinnedRobotMountFrames()
    {
        m_mountFrameTask->syncPinnedFrames();
    }

    QStringList ToolSetupModuleController::pinnedRobotMountFrameIds() const
    {
        return m_mountFrameTask->pinnedFrameIds();
    }

}
