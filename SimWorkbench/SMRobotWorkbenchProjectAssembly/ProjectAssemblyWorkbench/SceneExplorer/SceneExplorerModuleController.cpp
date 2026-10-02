#include "SceneExplorerModuleController.h"

#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPorts.h"
#include "RobotQtViewerViewportServices.h"
#include "RobotQtViewerViewportPreviewState.h"
#include "RobotQtWidgetUtils.h"
#include "SceneExplorerViewModelBuilder.h"
#include "SceneSelectionController.h"
#include "SceneExplorerTaskWidget.h"
#include "SceneExplorerWidget.h"
#include "SceneEntityWorkflowController.h"
#include "ToolTransformEditorWidget.h"

#include <SimulationProject/ProjectDocumentService.h>
#include <SimulationProject/ProjectSession.h>

#include <QAction>
#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <cmath>
#include <algorithm>
#include <cstddef>

namespace robot_qt_viewer
{
    namespace
    {
        class ProjectCameraDefinitionDialog final : public QDialog
        {
        public:
            ProjectCameraDefinitionDialog(
                const simulation_project::ProjectDocument& document,
                QWidget* parent)
                : QDialog(parent)
            {
                setWindowTitle(QStringLiteral("Add Camera Definition"));
                setMinimumSize(540, 560);

                auto* root = new QVBoxLayout(this);
                auto* definitionForm = new QFormLayout();
                m_nameEdit = new QLineEdit(QStringLiteral("Camera"), this);
                m_kindCombo = new QComboBox(this);
                m_kindCombo->addItem(QStringLiteral("Virtual Camera (no geometry)"), false);
                m_kindCombo->addItem(QStringLiteral("Camera With Geometry"), true);
                configureInspectorCombo(m_kindCombo);
                m_geometryCombo = new QComboBox(this);
                for(const simulation_project::SceneObjectDesc& object : document.objects) {
                    if(object.sourcePath.empty()) {
                        continue;
                    }
                    const QString name = QString::fromStdString(
                        object.name.empty() ? object.id : object.name);
                    m_geometryCombo->addItem(name, QString::fromStdString(object.id));
                    m_geometryCombo->setItemData(
                        m_geometryCombo->count() - 1,
                        QString::fromStdString(object.sourcePath),
                        Qt::ToolTipRole);
                }
                if(m_geometryCombo->count() == 0) {
                    m_geometryCombo->addItem(QStringLiteral("No project geometry available"));
                    m_geometryCombo->setToolTip(QStringLiteral(
                        "Import an object first, or create a virtual Camera without geometry."));
                }
                configureInspectorEntityCombo(m_geometryCombo);
                m_geometryCombo->setEnabled(false);
                configureInspectorForm(definitionForm);
                definitionForm->addRow(QStringLiteral("Camera name"), m_nameEdit);
                definitionForm->addRow(QStringLiteral("Definition type"), m_kindCombo);
                definitionForm->addRow(QStringLiteral("Geometry source"), m_geometryCombo);
                root->addLayout(definitionForm);

                auto* tabs = new QTabWidget(this);
                auto* imagingPage = new QWidget(tabs);
                auto* imagingForm = new QFormLayout(imagingPage);
                m_widthSpin = new QSpinBox(imagingPage);
                m_widthSpin->setRange(1, 16384);
                m_widthSpin->setValue(640);
                m_heightSpin = new QSpinBox(imagingPage);
                m_heightSpin->setRange(1, 16384);
                m_heightSpin->setValue(480);
                m_fovSpin = new QDoubleSpinBox(imagingPage);
                m_fovSpin->setRange(1.0, 179.0);
                m_fovSpin->setDecimals(3);
                m_fovSpin->setValue(60.0);
                m_nearSpin = new QDoubleSpinBox(imagingPage);
                m_nearSpin->setRange(0.0001, 10000.0);
                m_nearSpin->setDecimals(4);
                m_nearSpin->setValue(0.01);
                m_farSpin = new QDoubleSpinBox(imagingPage);
                m_farSpin->setRange(0.001, 1000000.0);
                m_farSpin->setDecimals(3);
                m_farSpin->setValue(10.0);
                imagingForm->addRow(QStringLiteral("Width"), m_widthSpin);
                imagingForm->addRow(QStringLiteral("Height"), m_heightSpin);
                imagingForm->addRow(QStringLiteral("Vertical FOV"), m_fovSpin);
                imagingForm->addRow(QStringLiteral("Near plane"), m_nearSpin);
                imagingForm->addRow(QStringLiteral("Far plane"), m_farSpin);
                tabs->addTab(imagingPage, QStringLiteral("Imaging"));

                auto* opticalPage = new QWidget(tabs);
                auto* opticalLayout = new QVBoxLayout(opticalPage);
                opticalLayout->addWidget(new QLabel(
                    QStringLiteral("Optical frame convention: +Z forward, +Y image-down."),
                    opticalPage));
                m_opticalEditor = new ToolTransformEditorWidget(
                    QStringLiteral("Camera mount -> Optical frame"), opticalPage);
                m_opticalEditor->setMatrixVisible(false);
                opticalLayout->addWidget(m_opticalEditor);
                opticalLayout->addStretch(1);
                tabs->addTab(opticalPage, QStringLiteral("Optical Frame"));
                root->addWidget(tabs, 1);

                m_buttons = new QDialogButtonBox(
                    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
                configureDialogButtonBox(m_buttons);
                connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
                connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
                root->addWidget(m_buttons);

                const auto updateAcceptance = [this]() {
                    const bool geometryRequired = m_kindCombo->currentData().toBool();
                    const bool geometryAvailable =
                        !m_geometryCombo->currentData().toString().isEmpty();
                    m_geometryCombo->setEnabled(geometryRequired && geometryAvailable);
                    QPushButton* okButton = m_buttons->button(QDialogButtonBox::Ok);
                    if(okButton != nullptr) {
                        okButton->setEnabled(
                            !m_nameEdit->text().trimmed().isEmpty() &&
                            m_farSpin->value() > m_nearSpin->value() &&
                            (!geometryRequired || geometryAvailable));
                    }
                };
                connect(m_kindCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    this, [updateAcceptance](int) { updateAcceptance(); });
                connect(m_nameEdit, &QLineEdit::textChanged,
                    this, [updateAcceptance](const QString&) { updateAcceptance(); });
                connect(m_nearSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    this, [updateAcceptance](double) { updateAcceptance(); });
                connect(m_farSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                    this, [updateAcceptance](double) { updateAcceptance(); });
                updateAcceptance();
            }

            QString cameraName() const { return m_nameEdit->text().trimmed(); }
            bool usesGeometry() const { return m_kindCombo->currentData().toBool(); }
            QString geometryObjectId() const { return m_geometryCombo->currentData().toString(); }
            int imageWidth() const { return m_widthSpin->value(); }
            int imageHeight() const { return m_heightSpin->value(); }
            double fovY() const { return m_fovSpin->value(); }
            double nearPlane() const { return m_nearSpin->value(); }
            double farPlane() const { return m_farSpin->value(); }
            simulation_project::TransformDesc opticalTransform() const
            {
                return m_opticalEditor->transform();
            }

        private:
            QLineEdit* m_nameEdit = nullptr;
            QComboBox* m_kindCombo = nullptr;
            QComboBox* m_geometryCombo = nullptr;
            QSpinBox* m_widthSpin = nullptr;
            QSpinBox* m_heightSpin = nullptr;
            QDoubleSpinBox* m_fovSpin = nullptr;
            QDoubleSpinBox* m_nearSpin = nullptr;
            QDoubleSpinBox* m_farSpin = nullptr;
            ToolTransformEditorWidget* m_opticalEditor = nullptr;
            QDialogButtonBox* m_buttons = nullptr;
        };

        bool transformForNode(
            const simulation_project::ProjectDocument& document,
            const SceneExplorerNodeRef& node,
            simulation_project::TransformDesc& transform)
        {
            if(node.kind == SceneExplorerNodeKind::Robot) {
                for(const simulation_project::RobotDesc& robot : document.robots) {
                    if(robot.id == node.id.toStdString()) {
                        transform = robot.baseTransform;
                        return true;
                    }
                }
                return false;
            }

            if(node.kind == SceneExplorerNodeKind::Object) {
                for(const simulation_project::SceneObjectDesc& object : document.objects) {
                    if(object.id == node.id.toStdString()) {
                        transform = object.transform;
                        return true;
                    }
                }
            }
            if(node.kind == SceneExplorerNodeKind::ObjectFrame) {
                for(const simulation_project::SceneObjectDesc& object : document.objects) {
                    if(object.id != node.id.toStdString()) {
                        continue;
                    }
                    for(const simulation_project::ObjectFrameDesc& frame : object.objectFrames) {
                        if(frame.id == node.linkName.toStdString()) {
                            transform = frame.objectToFrame;
                            return true;
                        }
                    }
                    return false;
                }
            }
            if(node.kind == SceneExplorerNodeKind::PointCloud) {
                for(const simulation_project::PointCloudDesc& pointCloud : document.pointClouds) {
                    if(pointCloud.id == node.id.toStdString()) {
                        transform = pointCloud.transform;
                        return true;
                    }
                }
            }
            return false;
        }

        std::string qStringToUtf8(const QString& text)
        {
            const QByteArray bytes = text.toUtf8();
            return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
        }

        const simulation_project::ObjectFrameDesc* findObjectFrameDesc(
            const simulation_project::ProjectDocument& document,
            const QString& objectId,
            const QString& frameId)
        {
            const std::string objectIdValue = qStringToUtf8(objectId);
            const std::string frameIdValue = qStringToUtf8(frameId);
            for(const simulation_project::SceneObjectDesc& object : document.objects) {
                if(object.id != objectIdValue) {
                    continue;
                }
                for(const simulation_project::ObjectFrameDesc& frame : object.objectFrames) {
                    if(frame.id == frameIdValue) {
                        return &frame;
                    }
                }
                return nullptr;
            }
            return nullptr;
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
    }

    SceneExplorerModuleController::SceneExplorerModuleController(
        SceneExplorerWidget& widget,
        RobotQtViewerDocumentContext& context,
        QObject* parent)
        : QObject(parent)
        , m_widget(widget)
        , m_context(context)
    {
        connect(&m_widget, &SceneExplorerWidget::nodeActivated,
            this, &SceneExplorerModuleController::handleNodeActivated);
        connect(&m_widget, &SceneExplorerWidget::treeContextMenuRequested,
            this, &SceneExplorerModuleController::showContextMenu);
    }

    void SceneExplorerModuleController::setRobotRuntime(
        const QString& robotId,
        const QString& robotName,
        const QStringList& links,
        const QStringList& joints,
        const QStringList& movableJoints,
        const QStringList& movableJointTypes)
    {
        for(SceneExplorerRobotRuntimeView& robot : m_robots) {
            if(robot.id == robotId) {
                robot.name = robotName;
                robot.links = links;
                robot.joints = joints;
                robot.movableJoints = movableJoints;
                robot.movableJointTypes = movableJointTypes;
                m_context.documentController().publishRobotRuntimeChanged(QStringLiteral("sceneExplorerRuntime"));
                return;
            }
        }

        SceneExplorerRobotRuntimeView robot;
        robot.id = robotId;
        robot.name = robotName;
        robot.links = links;
        robot.joints = joints;
        robot.movableJoints = movableJoints;
        robot.movableJointTypes = movableJointTypes;
        m_robots.push_back(robot);
        m_context.documentController().publishRobotRuntimeChanged(QStringLiteral("sceneExplorerRuntime"));
    }

    void SceneExplorerModuleController::setSceneObjectRuntime(const QString& objectId, const QString& objectName)
    {
        for(SceneExplorerObjectRuntimeView& object : m_objects) {
            if(object.id == objectId) {
                object.name = objectName;
                m_context.documentController().publishRobotRuntimeChanged(QStringLiteral("sceneExplorerRuntime"));
                return;
            }
        }

        SceneExplorerObjectRuntimeView object;
        object.id = objectId;
        object.name = objectName;
        m_objects.push_back(object);
        m_context.documentController().publishRobotRuntimeChanged(QStringLiteral("sceneExplorerRuntime"));
    }

    void SceneExplorerModuleController::clearRuntime()
    {
        m_robots.clear();
        m_objects.clear();
        m_context.documentController().publishRobotRuntimeChanged(QStringLiteral("sceneExplorerRuntimeClear"));
    }

    void SceneExplorerModuleController::refreshViewModel()
    {
        SceneExplorerViewModel viewModel = buildSceneExplorerViewModel(
            m_context.document(),
            m_robots,
            m_objects,
            m_widget.currentNode(),
            SceneExplorerViewOptions{
                m_interactionMode,
                sceneExplorerTreeProjectionForWorkbench(m_workbenchDescriptor)
            });
        if(m_objectFrameEditSnapshotIsNew && m_hasPendingTransformPreview) {
            const simulation_project::ProjectDocumentService service(m_context.document());
            const simulation_project::SceneObjectDesc* object =
                service.findSceneObject(m_activeObjectFrameObjectId.toStdString());
            viewModel.transformEditorVisible = true;
            viewModel.transformEditorEnabled = true;
            viewModel.transformEditorDirty = true;
            viewModel.transformEditorTitle = QStringLiteral("Frame Transform");
            viewModel.transformTarget = m_pendingTransformTarget;
            viewModel.transform = m_pendingTransform;
            viewModel.objectFrameEditorVisible = true;
            viewModel.objectFrameMode = SceneExplorerObjectFrameMode::Create;
            viewModel.objectFrameObjectName = object == nullptr
                ? m_activeObjectFrameObjectId
                : QString::fromStdString(object->name.empty() ? object->id : object->name);
            viewModel.objectFrameName = m_pendingTransformTarget.name;
            viewModel.objectFrameVisible = m_objectFrameEditSnapshot.visible;
            viewModel.objectFrameVisibilityControlVisible = true;
            viewModel.objectFrameVisibilityTarget = m_pendingTransformTarget;
            viewModel.objectFrameDiagram.visible = true;
            viewModel.objectFrameDiagram.objectName = viewModel.objectFrameObjectName;
            viewModel.objectFrameDiagram.objectFrameName = viewModel.objectFrameName;
        }
        if(viewModel.objectFrameEditorVisible &&
            viewModel.transformTarget.kind == SceneExplorerNodeKind::ObjectFrame &&
            viewModel.transformTarget.id == m_activeObjectFrameObjectId &&
            viewModel.transformTarget.linkName == m_activeObjectFrameId) {
            viewModel.objectFrameMode = m_objectFrameMode;
        }
        if(viewModel.transformEditorVisible && hasPendingTransformPreviewFor(viewModel.transformTarget)) {
            viewModel.transform = m_pendingTransform;
            viewModel.transformEditorDirty = true;
            if(viewModel.transformTarget.kind == SceneExplorerNodeKind::ObjectFrame &&
                !m_pendingTransformTarget.name.isEmpty()) {
                viewModel.objectFrameName = m_pendingTransformTarget.name;
            }
        }
        m_widget.setDocumentView(viewModel);
        if(m_taskWidget != nullptr) {
            m_taskWidget->setDocumentView(viewModel);
        }
    }

    void SceneExplorerModuleController::setSummaryText(const QString& text)
    {
        m_widget.setSummaryText(text);
    }

    void SceneExplorerModuleController::setTaskWidget(SceneExplorerTaskWidget* taskWidget)
    {
        m_taskWidget = taskWidget;
        if(m_taskWidget == nullptr) {
            return;
        }
        connect(m_taskWidget, &SceneExplorerTaskWidget::transformPreviewChanged,
            this, &SceneExplorerModuleController::handleTransformPreviewChanged);
        connect(m_taskWidget, &SceneExplorerTaskWidget::transformApplyRequested,
            this, &SceneExplorerModuleController::applyTransformChange);
        connect(m_taskWidget, &SceneExplorerTaskWidget::transformCancelRequested,
            this, &SceneExplorerModuleController::cancelTransformChange);
        connect(m_taskWidget, &SceneExplorerTaskWidget::objectFrameVisibilityChanged,
            this, &SceneExplorerModuleController::handleObjectFrameVisibilityChanged);
        refreshViewModel();
    }

    void SceneExplorerModuleController::setSceneEntityWorkflow(SceneEntityWorkflowController* workflow)
    {
        m_sceneEntityWorkflow = workflow;
    }

    void SceneExplorerModuleController::setViewportServices(RobotQtViewerViewportServices* viewportServices)
    {
        m_viewportServices = viewportServices;
    }

    void SceneExplorerModuleController::setAssemblyViewport(
        IRobotQtViewerAssemblyViewportPort* viewport)
    {
        m_assemblyViewport = viewport;
    }

    void SceneExplorerModuleController::setViewportInteractionMode(RobotQtViewerViewportInteractionMode mode)
    {
        if(m_interactionMode == mode) {
            return;
        }
        m_interactionMode = mode;
        publishTaskStateChanged(QStringLiteral("sceneExplorerInteractionMode"));
    }

    void SceneExplorerModuleController::setWorkbenchDescriptor(
        const RobotQtViewerWorkbenchDescriptor& descriptor)
    {
        m_workbenchDescriptor = descriptor;
        setViewportInteractionMode(descriptor.defaultViewportMode);
        refreshViewModel();
    }

    void SceneExplorerModuleController::handleEvent(const RobotQtViewerEvent& event)
    {
        switch(event.kind) {
        case RobotQtViewerEventKind::ProjectDocumentChanged:
        case RobotQtViewerEventKind::ViewportReloaded:
        case RobotQtViewerEventKind::RobotRuntimeChanged:
        case RobotQtViewerEventKind::AttachmentChanged:
        case RobotQtViewerEventKind::TaskStateChanged:
            refreshViewModel();
            break;
        default:
            break;
        }
    }

    SceneExplorerNodeRef SceneExplorerModuleController::currentNode() const
    {
        return m_widget.currentNode();
    }

    bool SceneExplorerModuleController::selectNode(const SceneExplorerNodeRef& node)
    {
        return m_widget.selectNode(node);
    }

    QHash<QString, QStringList> SceneExplorerModuleController::robotLinksByRobotId() const
    {
        QHash<QString, QStringList> linksByRobot;
        for(const SceneExplorerRobotRuntimeView& robot : m_robots) {
            linksByRobot.insert(robot.id, robot.links);
        }
        return linksByRobot;
    }

    SceneSelectionIntent SceneExplorerModuleController::selectionIntentForNode(
        const SceneExplorerNodeRef& node) const
    {
        return SceneSelectionController::intentFromNode(m_context.document(), node);
    }

    SceneRobotSelectionContext SceneExplorerModuleController::robotSelectionContext(
        const QString& robotId,
        const QString& preferredLinkName,
        const QString& preferredMountId) const
    {
        return SceneSelectionController::robotSelectionContext(
            m_context.document(),
            robotId,
            preferredLinkName,
            preferredMountId);
    }

    void SceneExplorerModuleController::focusTransformTask(const SceneExplorerNodeRef& node)
    {
        if(node.kind != SceneExplorerNodeKind::Robot &&
            node.kind != SceneExplorerNodeKind::Object &&
            node.kind != SceneExplorerNodeKind::ObjectFrame &&
            node.kind != SceneExplorerNodeKind::PointCloud) {
            emit statusMessageRequested(
                QStringLiteral("Select a robot, scene object, object frame, or point cloud transform."),
                3000);
            return;
        }

        if(node.kind == SceneExplorerNodeKind::Object ||
            node.kind == SceneExplorerNodeKind::ObjectFrame) {
            m_interactionMode = RobotQtViewerViewportInteractionMode::EditTransformPreview;
            if(node.kind == SceneExplorerNodeKind::ObjectFrame) {
                m_objectFrameMode = SceneExplorerObjectFrameMode::Edit;
                captureObjectFrameEditSnapshot(node.id, node.linkName);
                m_context.selectionModel().selectObjectFrame(
                    node.id,
                    node.linkName,
                    QStringLiteral("sceneExplorerObjectFrameEditTask"));
                RobotQtViewerViewportPreviewPayload preview;
                preview.focusObjectFrameObject = true;
                preview.focusObjectFrameObjectId = node.id;
                mutateViewportPreview(preview, QStringLiteral("sceneExplorerTransformTask"));
            }
        }
        publishTaskStateChanged(QStringLiteral("sceneExplorerTransformTask"));
        emit statusMessageRequested(QStringLiteral("Transform editor is ready."), 2500);
    }

    void SceneExplorerModuleController::createObjectFrameForObject(const QString& objectId)
    {
        if(objectId.isEmpty()) {
            emit statusMessageRequested(QStringLiteral("Select an object before adding an object frame."), 3000);
            return;
        }

        const std::string objectIdUtf8 = objectId.toStdString();
        simulation_project::ProjectDocumentService service(m_context.document());
        if(service.findSceneObject(objectIdUtf8) == nullptr) {
            emit statusMessageRequested(QStringLiteral("Object not found."), 5000);
            return;
        }

        const QString baseId = objectId + QStringLiteral("_frame");
        QString frameId = baseId;
        int suffix = 1;
        while(service.findObjectFrame(objectIdUtf8, frameId.toStdString()) != nullptr) {
            frameId = QString("%1_%2").arg(baseId).arg(suffix++);
        }
        simulation_project::ObjectFrameDesc frame;
        frame.id = frameId.toStdString();
        frame.name = frame.id;

        m_objectFrameMode = SceneExplorerObjectFrameMode::Create;
        m_objectFrameDraftSourceObjectId = objectId;
        m_activeObjectFrameObjectId = objectId;
        m_activeObjectFrameId = frameId;
        m_hasObjectFrameEditSnapshot = true;
        m_objectFrameEditSnapshotIsNew = true;
        m_objectFrameEditSnapshot = frame;
        m_interactionMode = RobotQtViewerViewportInteractionMode::EditTransformPreview;
        SceneExplorerNodeRef node;
        node.kind = SceneExplorerNodeKind::ObjectFrame;
        node.id = objectId;
        node.name = frameId;
        node.linkName = frameId;
        m_hasPendingTransformPreview = true;
        m_pendingTransformTarget = node;
        m_pendingTransform = frame.objectToFrame;
        RobotQtViewerViewportPreviewPayload preview;
        preview.upsertPreviewObjectFrame = true;
        preview.objectFrameObjectId = objectId;
        preview.objectFrame = frame;
        preview.previewObjectFrameTransform = true;
        preview.previewObjectFrameObjectId = objectId;
        preview.previewObjectFrameId = frameId;
        preview.objectFrameTransform = frame.objectToFrame;
        preview.focusObjectFrameObject = true;
        preview.focusObjectFrameObjectId = objectId;
        mutateViewportPreview(preview, QStringLiteral("createObjectFrame"));
        publishTaskStateChanged(QStringLiteral("createObjectFrame"));
        refreshViewModel();
        emit statusMessageRequested(QString("Object frame draft created: %1").arg(frameId), 3000);
    }

    void SceneExplorerModuleController::createCameraDefinition(QWidget* parentWidget)
    {
        ProjectCameraDefinitionDialog dialog(m_context.document(), parentWidget);
        if(dialog.exec() != QDialog::Accepted) {
            return;
        }

        simulation_project::AttachmentAssetDesc camera;
        const QString geometryObjectId = dialog.geometryObjectId();
        const ProjectMutationResult result = m_context.documentController().mutateProject(
            QStringLiteral("createCameraDefinition"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                camera.id = service.makeUniqueId("camera");
                camera.name = qStringToUtf8(dialog.cameraName());
                camera.assetKind = "sensor";
                camera.assetType = "camera";
                camera.visible = true;
                camera.hasSensorIntrinsics = true;
                camera.sensorIntrinsics.model = "pinhole";
                camera.sensorIntrinsics.width = dialog.imageWidth();
                camera.sensorIntrinsics.height = dialog.imageHeight();
                camera.sensorIntrinsics.fovY = dialog.fovY();
                camera.sensorIntrinsics.nearPlane = dialog.nearPlane();
                camera.sensorIntrinsics.farPlane = dialog.farPlane();

                if(dialog.usesGeometry()) {
                    const simulation_project::SceneObjectDesc* geometry =
                        service.findSceneObject(qStringToUtf8(geometryObjectId));
                    if(geometry == nullptr || geometry->sourcePath.empty()) {
                        error = "Selected camera geometry is no longer available.";
                        return false;
                    }
                    camera.visualPath = geometry->sourcePath;
                    camera.visualScale = geometry->visualScale;
                }

                simulation_project::AttachmentFunctionalFrameDesc opticalFrame;
                opticalFrame.id = camera.id + ".optical";
                opticalFrame.name = "Optical";
                opticalFrame.frameType = "optical";
                opticalFrame.assetMountToFrame = dialog.opticalTransform();
                opticalFrame.primary = true;
                camera.functionalFrames.push_back(opticalFrame);
                if(!service.addAttachmentAsset(camera, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!result.success) {
            QMessageBox::warning(
                parentWidget,
                QStringLiteral("Add Camera"),
                QStringLiteral("Camera creation failed: %1").arg(result.message));
            return;
        }

        m_context.documentController().publishDocumentChanged(
            QStringLiteral("createCameraDefinition"), false);
        refreshViewModel();
        emit statusMessageRequested(
            QStringLiteral("Created camera definition: %1").arg(dialog.cameraName()), 4000);
    }

    bool SceneExplorerModuleController::resolvePendingTransformPreviewIfTargetChanges(
        const SceneExplorerNodeRef& nextNode,
        QWidget* parentWidget)
    {
        if(!m_hasPendingTransformPreview || sameTransformTarget(m_pendingTransformTarget, nextNode)) {
            return true;
        }
        const bool resolved = resolvePendingTransformPreview(parentWidget);
        if(!resolved) {
            reassertActiveObjectFrameTask(QStringLiteral("sceneExplorerKeepObjectFrameTask"));
        }
        return resolved;
    }

    bool SceneExplorerModuleController::linkFrameVisible(
        const QString& robotId,
        const QString& linkName) const
    {
        return !robotId.isEmpty() &&
            !linkName.isEmpty() &&
            m_visibleLinkFrameKeys.contains(linkFrameKey(robotId, linkName));
    }

    bool SceneExplorerModuleController::toggleLinkFrameVisible(
        const QString& robotId,
        const QString& linkName)
    {
        if(robotId.isEmpty() || linkName.isEmpty()) {
            return false;
        }

        const QString key = linkFrameKey(robotId, linkName);
        if(m_visibleLinkFrameKeys.contains(key)) {
            m_visibleLinkFrameKeys.remove(key);
            return false;
        }

        m_visibleLinkFrameKeys.insert(key);
        return true;
    }

    bool SceneExplorerModuleController::nodeSelectableForCurrentMode(const SceneExplorerNodeRef& node) const
    {
        return sceneExplorerNodeSelectableForMode(node.kind, m_interactionMode);
    }

    void SceneExplorerModuleController::handleNodeActivated(const SceneExplorerNodeRef& node, int column)
    {
        if(!nodeSelectableForCurrentMode(node)) {
            emit statusMessageRequested(QStringLiteral("This item is not selectable in the current task."), 2500);
            refreshViewModel();
            return;
        }
        emit nodeActivated(node, column);
        refreshViewModel();
    }

    void SceneExplorerModuleController::handleTransformPreviewChanged(
        const SceneExplorerNodeRef& target,
        const simulation_project::TransformDesc& transform)
    {
        if(target.id.isEmpty()) {
            return;
        }
        if(m_hasPendingTransformPreview && !sameTransformTarget(m_pendingTransformTarget, target)) {
            discardPendingTransformPreview();
        }
        m_hasPendingTransformPreview = true;
        m_pendingTransformTarget = target;
        m_pendingTransform = transform;
        RobotQtViewerViewportPreviewPayload preview;
        if(target.kind == SceneExplorerNodeKind::Robot) {
            preview.previewRobotBaseTransform = true;
            preview.robotBaseRobotId = target.id;
            preview.robotBaseTransform = transform;
        } else if(target.kind == SceneExplorerNodeKind::ObjectFrame) {
            preview.previewObjectFrameTransform = true;
            preview.previewObjectFrameObjectId = target.id;
            preview.previewObjectFrameId = target.linkName;
            preview.objectFrameTransform = transform;
        } else if(target.kind == SceneExplorerNodeKind::Object ||
            target.kind == SceneExplorerNodeKind::PointCloud) {
            preview.previewSceneObjectTransform = true;
            preview.sceneObjectId = target.id;
            preview.sceneObjectTransform = transform;
        } else {
            return;
        }
        mutateViewportPreview(preview, QStringLiteral("sceneExplorerTransformPreview"));
    }

    void SceneExplorerModuleController::applyTransformChange(
        const SceneExplorerNodeRef& target,
        const simulation_project::TransformDesc& transform)
    {
        if(target.id.isEmpty()) {
            emit statusMessageRequested(QStringLiteral("Scene transform workflow is not available."), 4000);
            return;
        }
        if(target.kind != SceneExplorerNodeKind::ObjectFrame && m_sceneEntityWorkflow == nullptr) {
            emit statusMessageRequested(QStringLiteral("Scene transform workflow is not available."), 4000);
            return;
        }

        SceneEntityMutationResult result;
        if(target.kind == SceneExplorerNodeKind::Robot) {
            result = m_sceneEntityWorkflow->setRobotBaseTransform(target.id, transform);
        } else if(target.kind == SceneExplorerNodeKind::Object) {
            result = m_sceneEntityWorkflow->setSceneObjectTransform(target.id, transform);
        } else if(target.kind == SceneExplorerNodeKind::PointCloud) {
            result = m_sceneEntityWorkflow->setPointCloudTransform(target.id, transform);
        } else if(target.kind == SceneExplorerNodeKind::ObjectFrame) {
            if(target.name.trimmed().isEmpty()) {
                emit statusMessageRequested(QStringLiteral("Frame name cannot be empty."), 4000);
                return;
            }
            simulation_project::ObjectFrameDesc updated;
            const std::string objectId = target.id.toStdString();
            const std::string frameId = target.linkName.toStdString();
            const std::string frameName = qStringToUtf8(target.name.trimmed());
            const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
                QStringLiteral("applyObjectFrameTransform"),
                ProjectDirtyPolicy::UserEdit,
                [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                    const simulation_project::ObjectFrameDesc* existing =
                        service.findObjectFrame(objectId, frameId);
                    const bool creating = m_objectFrameEditSnapshotIsNew;
                    if(!creating && existing == nullptr) {
                        error = "Object frame not found.";
                        return false;
                    }
                    updated = creating ? m_objectFrameEditSnapshot : *existing;
                    updated.name = frameName;
                    updated.objectToFrame = transform;
                    if(!creating && updated.name == existing->name &&
                        sameTransform(updated.objectToFrame, existing->objectToFrame)) {
                        changed = false;
                        return true;
                    }
                    if(creating
                           ? !service.addObjectFrame(objectId, updated, &error)
                           : !service.updateObjectFrame(objectId, frameId, updated, &error)) {
                        return false;
                    }
                    changed = true;
                    return true;
                });
            if(!mutationResult.success) {
                emit statusMessageRequested(
                    QString("Object frame update failed: %1").arg(mutationResult.message),
                    5000);
                return;
            }
            const bool created = m_objectFrameEditSnapshotIsNew;
            m_interactionMode = RobotQtViewerViewportInteractionMode::Browse;
            RobotQtViewerViewportPreviewPayload preview;
            preview.clearObjectFrameObjectFocus = true;
            preview.upsertPreviewObjectFrame = true;
            preview.objectFrameObjectId = target.id;
            preview.objectFrame = updated;
            result.success = true;
            if(mutationResult.changed) {
                m_context.selectionModel().selectObjectFrame(
                    target.id,
                    target.linkName,
                    QStringLiteral("applyObjectFrameTransform"));
                result.message = created
                    ? QString("Created object frame: %1").arg(QString::fromStdString(updated.name))
                    : QString("Updated object frame: %1").arg(QString::fromStdString(updated.name));
            } else {
                result.message = QString("Object frame unchanged: %1").arg(target.name.trimmed());
            }
            mutateViewportPreview(preview, QStringLiteral("applyObjectFrameTransform"));
            m_objectFrameMode = SceneExplorerObjectFrameMode::Selection;
            clearObjectFrameEditSnapshot();
        } else {
            emit statusMessageRequested(
                QStringLiteral("Select a robot, scene object, object frame, or point cloud transform."),
                3000);
            return;
        }

        if(!result.success) {
            emit statusMessageRequested(result.message, 5000);
            return;
        }

        if((target.kind == SceneExplorerNodeKind::Object ||
               target.kind == SceneExplorerNodeKind::PointCloud) &&
            (m_assemblyViewport != nullptr || m_viewportServices != nullptr)) {
            if(m_assemblyViewport != nullptr) {
                m_assemblyViewport->commitSceneObjectTransform(target.id, transform);
            } else {
                m_viewportServices->commitSceneObjectTransform(target.id, transform);
            }
        }

        if(target.kind == SceneExplorerNodeKind::Object) {
            m_interactionMode = RobotQtViewerViewportInteractionMode::Browse;
            publishTaskStateChanged(QStringLiteral("sceneExplorerApplyObjectTransform"));
        }
        if(hasPendingTransformPreviewFor(target)) {
            m_hasPendingTransformPreview = false;
            m_pendingTransformTarget = SceneExplorerNodeRef();
            m_pendingTransform = simulation_project::TransformDesc();
        }
        refreshViewModel();
        emit statusMessageRequested(result.message, 3000);
    }

    void SceneExplorerModuleController::cancelTransformChange(const SceneExplorerNodeRef& target)
    {
        if(target.kind == SceneExplorerNodeKind::ObjectFrame && m_objectFrameEditSnapshotIsNew) {
            restoreObjectFrameDraft(QStringLiteral("cancelObjectFrameDraft"));
            emit statusMessageRequested(QStringLiteral("Object frame creation canceled."), 2500);
            return;
        }

        simulation_project::TransformDesc transform;
        if(transformForNode(m_context.document(), target, transform)) {
            RobotQtViewerViewportPreviewPayload preview;
            if(target.kind == SceneExplorerNodeKind::Robot) {
                preview.previewRobotBaseTransform = true;
                preview.robotBaseRobotId = target.id;
                preview.robotBaseTransform = transform;
            } else if(target.kind == SceneExplorerNodeKind::ObjectFrame) {
                preview.previewObjectFrameTransform = true;
                preview.previewObjectFrameObjectId = target.id;
                preview.previewObjectFrameId = target.linkName;
                preview.objectFrameTransform = transform;
                preview.clearObjectFrameObjectFocus = true;
                if(const simulation_project::ObjectFrameDesc* frame =
                       findObjectFrameDesc(m_context.document(), target.id, target.linkName)) {
                    preview.upsertPreviewObjectFrame = true;
                    preview.objectFrameObjectId = target.id;
                    preview.objectFrame = *frame;
                }
            } else if(target.kind == SceneExplorerNodeKind::Object ||
                target.kind == SceneExplorerNodeKind::PointCloud) {
                preview.previewSceneObjectTransform = true;
                preview.sceneObjectId = target.id;
                preview.sceneObjectTransform = transform;
            } else {
                return;
            }
            mutateViewportPreview(preview, QStringLiteral("sceneExplorerCancelTransform"));
        }
        if(target.kind == SceneExplorerNodeKind::Object ||
            target.kind == SceneExplorerNodeKind::ObjectFrame) {
            m_interactionMode = RobotQtViewerViewportInteractionMode::Browse;
        }
        if(target.kind == SceneExplorerNodeKind::ObjectFrame) {
            m_objectFrameMode = SceneExplorerObjectFrameMode::Selection;
            clearObjectFrameEditSnapshot();
        }
        if(hasPendingTransformPreviewFor(target)) {
            m_hasPendingTransformPreview = false;
            m_pendingTransformTarget = SceneExplorerNodeRef();
            m_pendingTransform = simulation_project::TransformDesc();
        }
        publishTaskStateChanged(QStringLiteral("sceneExplorerCancelTransform"));
        emit statusMessageRequested(QStringLiteral("Transform edits canceled."), 2500);
    }

    void SceneExplorerModuleController::handleObjectFrameVisibilityChanged(
        const SceneExplorerNodeRef& target,
        bool visible)
    {
        if(target.kind != SceneExplorerNodeKind::ObjectFrame ||
            target.id.isEmpty() ||
            target.linkName.isEmpty()) {
            emit statusMessageRequested(QStringLiteral("Select an object frame first."), 3000);
            refreshViewModel();
            return;
        }

        if(m_objectFrameEditSnapshotIsNew &&
            target.id == m_activeObjectFrameObjectId &&
            target.linkName == m_activeObjectFrameId) {
            m_objectFrameEditSnapshot.visible = visible;
            RobotQtViewerViewportPreviewPayload preview;
            preview.upsertPreviewObjectFrame = true;
            preview.objectFrameObjectId = target.id;
            preview.objectFrame = m_objectFrameEditSnapshot;
            mutateViewportPreview(preview, QStringLiteral("previewObjectFrameVisibility"));
            refreshViewModel();
            return;
        }

        const std::string objectId = target.id.toStdString();
        const std::string frameId = target.linkName.toStdString();
        const ProjectMutationResult mutationResult = m_context.documentController().mutateProject(
            QStringLiteral("setObjectFrameVisibility"),
            ProjectDirtyPolicy::UserEdit,
            [&](simulation_project::ProjectDocumentService& service, bool& changed, std::string& error) {
                const simulation_project::ObjectFrameDesc* existing =
                    service.findObjectFrame(objectId, frameId);
                if(existing == nullptr) {
                    error = "Object frame not found: " + frameId;
                    return false;
                }
                if(existing->visible == visible) {
                    changed = false;
                    return true;
                }
                if(!service.setObjectFrameVisibility(objectId, frameId, visible, &error)) {
                    return false;
                }
                changed = true;
                return true;
            });
        if(!mutationResult.success) {
            emit statusMessageRequested(
                QString("Object frame visibility update failed: %1").arg(mutationResult.message),
                5000);
            refreshViewModel();
            return;
        }

        if(const simulation_project::ObjectFrameDesc* frame =
               findObjectFrameDesc(m_context.document(), target.id, target.linkName)) {
            RobotQtViewerViewportPreviewPayload preview;
            preview.upsertPreviewObjectFrame = true;
            preview.objectFrameObjectId = target.id;
            preview.objectFrame = *frame;
            mutateViewportPreview(preview, QStringLiteral("setObjectFrameVisibility"));
        }

        emit statusMessageRequested(
            visible
                ? QStringLiteral("Object frame display enabled.")
                : QStringLiteral("Object frame display disabled."),
            2500);
    }

    bool SceneExplorerModuleController::sameTransformTarget(
        const SceneExplorerNodeRef& lhs,
        const SceneExplorerNodeRef& rhs) const
    {
        return lhs.kind == rhs.kind && lhs.id == rhs.id && lhs.linkName == rhs.linkName;
    }

    bool SceneExplorerModuleController::hasPendingTransformPreviewFor(const SceneExplorerNodeRef& target) const
    {
        return m_hasPendingTransformPreview && sameTransformTarget(m_pendingTransformTarget, target);
    }

    bool SceneExplorerModuleController::hasPendingTransformPreview() const
    {
        return m_hasPendingTransformPreview;
    }

    bool SceneExplorerModuleController::resolvePendingTransformPreview(QWidget* parentWidget)
    {
        if(!m_hasPendingTransformPreview) {
            return true;
        }

        const QMessageBox::StandardButton button = QMessageBox::question(
            parentWidget,
            QStringLiteral("Unsaved Transform Changes"),
            QStringLiteral("Save transform changes before changing selection?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        if(button == QMessageBox::Cancel) {
            return false;
        }

        const SceneExplorerNodeRef target = m_pendingTransformTarget;
        const simulation_project::TransformDesc transform = m_pendingTransform;
        if(button == QMessageBox::Discard) {
            discardPendingTransformPreview();
            publishTaskStateChanged(QStringLiteral("sceneExplorerDiscardTransform"));
            emit statusMessageRequested(QStringLiteral("Transform edits discarded."), 2500);
            return true;
        }

        applyTransformChange(target, transform);
        return !m_hasPendingTransformPreview;
    }

    void SceneExplorerModuleController::discardPendingTransformPreview()
    {
        if(!m_hasPendingTransformPreview) {
            return;
        }

        const SceneExplorerNodeRef target = m_pendingTransformTarget;
        if(target.kind == SceneExplorerNodeKind::ObjectFrame && m_objectFrameEditSnapshotIsNew) {
            restoreObjectFrameDraft(QStringLiteral("discardObjectFrameDraft"));
            return;
        }

        m_hasPendingTransformPreview = false;
        m_pendingTransformTarget = SceneExplorerNodeRef();
        m_pendingTransform = simulation_project::TransformDesc();

        simulation_project::TransformDesc transform;
        if(transformForNode(m_context.document(), target, transform)) {
            RobotQtViewerViewportPreviewPayload preview;
            if(target.kind == SceneExplorerNodeKind::Robot) {
                preview.previewRobotBaseTransform = true;
                preview.robotBaseRobotId = target.id;
                preview.robotBaseTransform = transform;
            } else if(target.kind == SceneExplorerNodeKind::ObjectFrame) {
                preview.previewObjectFrameTransform = true;
                preview.previewObjectFrameObjectId = target.id;
                preview.previewObjectFrameId = target.linkName;
                preview.objectFrameTransform = transform;
                preview.clearObjectFrameObjectFocus = true;
                if(const simulation_project::ObjectFrameDesc* frame =
                       findObjectFrameDesc(m_context.document(), target.id, target.linkName)) {
                    preview.upsertPreviewObjectFrame = true;
                    preview.objectFrameObjectId = target.id;
                    preview.objectFrame = *frame;
                }
            } else if(target.kind == SceneExplorerNodeKind::Object ||
                target.kind == SceneExplorerNodeKind::PointCloud) {
                preview.previewSceneObjectTransform = true;
                preview.sceneObjectId = target.id;
                preview.sceneObjectTransform = transform;
            } else {
                return;
            }
            mutateViewportPreview(preview, QStringLiteral("sceneExplorerDiscardTransform"));
        }
        if(target.kind == SceneExplorerNodeKind::Object ||
            target.kind == SceneExplorerNodeKind::ObjectFrame) {
            m_interactionMode = RobotQtViewerViewportInteractionMode::Browse;
        }
    }

    void SceneExplorerModuleController::releaseProjectState() noexcept
    {
        m_hasPendingTransformPreview = false;
        m_pendingTransformTarget = SceneExplorerNodeRef();
        m_pendingTransform = simulation_project::TransformDesc();
        m_interactionMode = RobotQtViewerViewportInteractionMode::Browse;
        m_objectFrameMode = SceneExplorerObjectFrameMode::Selection;
        clearObjectFrameEditSnapshot();
        m_visibleLinkFrameKeys.clear();
    }

    void SceneExplorerModuleController::publishTaskStateChanged(const QString& sourceId)
    {
        RobotQtViewerEvent event;
        event.kind = RobotQtViewerEventKind::TaskStateChanged;
        event.sourceId = sourceId;
        m_context.eventHub().publish(event);
    }

    void SceneExplorerModuleController::mutateViewportPreview(
        const RobotQtViewerViewportPreviewPayload& preview,
        const QString& sourceId)
    {
        RobotQtViewerViewportPreviewPayload mergedPreview = preview;
        mergeActiveObjectFrameViewportState(mergedPreview);
        m_context.viewportPreviewState().mutate(mergedPreview, sourceId);
    }

    bool SceneExplorerModuleController::objectFrameTaskActive() const
    {
        return (m_objectFrameMode == SceneExplorerObjectFrameMode::Create ||
                   m_objectFrameMode == SceneExplorerObjectFrameMode::Edit) &&
            !m_activeObjectFrameObjectId.isEmpty() &&
            !m_activeObjectFrameId.isEmpty();
    }

    void SceneExplorerModuleController::mergeActiveObjectFrameViewportState(
        RobotQtViewerViewportPreviewPayload& preview) const
    {
        if(!objectFrameTaskActive() || preview.clearObjectFrameObjectFocus) {
            return;
        }

        if(!preview.focusObjectFrameObject) {
            preview.focusObjectFrameObject = true;
            preview.focusObjectFrameObjectId = m_activeObjectFrameObjectId;
        }
        if(!preview.previewObjectFrameTransform) {
            preview.previewObjectFrameTransform = true;
            preview.previewObjectFrameObjectId = m_activeObjectFrameObjectId;
            preview.previewObjectFrameId = m_activeObjectFrameId;
            preview.objectFrameTransform = m_hasPendingTransformPreview &&
                    m_pendingTransformTarget.kind == SceneExplorerNodeKind::ObjectFrame &&
                    m_pendingTransformTarget.id == m_activeObjectFrameObjectId &&
                    m_pendingTransformTarget.linkName == m_activeObjectFrameId
                ? m_pendingTransform
                : m_objectFrameEditSnapshot.objectToFrame;
        }
    }

    void SceneExplorerModuleController::reassertActiveObjectFrameTask(const QString& sourceId)
    {
        if(!objectFrameTaskActive()) {
            return;
        }

        SceneExplorerNodeRef node;
        node.kind = SceneExplorerNodeKind::ObjectFrame;
        node.id = m_activeObjectFrameObjectId;
        node.name = m_activeObjectFrameId;
        node.linkName = m_activeObjectFrameId;
        selectNode(node);
        m_context.selectionModel().selectObjectFrame(
            m_activeObjectFrameObjectId,
            m_activeObjectFrameId,
            sourceId);

        RobotQtViewerViewportPreviewPayload preview;
        mergeActiveObjectFrameViewportState(preview);
        m_context.viewportPreviewState().mutate(preview, sourceId);
    }

    void SceneExplorerModuleController::captureObjectFrameEditSnapshot(
        const QString& objectId,
        const QString& frameId,
        bool newFrame)
    {
        const simulation_project::SceneObjectDesc* object = nullptr;
        for(const simulation_project::SceneObjectDesc& candidate : m_context.document().objects) {
            if(QString::fromStdString(candidate.id) == objectId) {
                object = &candidate;
                break;
            }
        }
        if(object == nullptr) {
            clearObjectFrameEditSnapshot();
            return;
        }

        const std::string frameIdValue = frameId.toStdString();
        const auto frameIt = std::find_if(
            object->objectFrames.begin(),
            object->objectFrames.end(),
            [&](const simulation_project::ObjectFrameDesc& frame) {
                return frame.id == frameIdValue;
            });
        if(frameIt == object->objectFrames.end()) {
            clearObjectFrameEditSnapshot();
            return;
        }

        m_hasObjectFrameEditSnapshot = true;
        m_objectFrameEditSnapshotIsNew = newFrame;
        m_activeObjectFrameObjectId = objectId;
        m_activeObjectFrameId = frameId;
        m_objectFrameEditSnapshot = *frameIt;
    }

    void SceneExplorerModuleController::clearObjectFrameEditSnapshot()
    {
        m_hasObjectFrameEditSnapshot = false;
        m_objectFrameEditSnapshotIsNew = false;
        m_activeObjectFrameObjectId.clear();
        m_activeObjectFrameId.clear();
        m_objectFrameDraftSourceObjectId.clear();
        m_objectFrameEditSnapshot = simulation_project::ObjectFrameDesc();
    }

    bool SceneExplorerModuleController::restoreObjectFrameDraft(const QString& sourceId)
    {
        const QString sourceObjectId = m_objectFrameDraftSourceObjectId;
        RobotQtViewerViewportPreviewPayload preview;
        preview.clearObjectFrameObjectFocus = true;
        mutateViewportPreview(preview, sourceId);

        m_hasPendingTransformPreview = false;
        m_pendingTransformTarget = SceneExplorerNodeRef();
        m_pendingTransform = simulation_project::TransformDesc();
        m_interactionMode = RobotQtViewerViewportInteractionMode::Browse;
        m_objectFrameMode = SceneExplorerObjectFrameMode::Selection;
        clearObjectFrameEditSnapshot();
        if(!sourceObjectId.isEmpty()) {
            SceneExplorerNodeRef objectNode;
            objectNode.kind = SceneExplorerNodeKind::Object;
            objectNode.id = sourceObjectId;
            objectNode.name = sourceObjectId;
            selectNode(objectNode);
            m_context.selectionModel().selectSceneObject(sourceObjectId, sourceId);
        }
        publishTaskStateChanged(sourceId);
        refreshViewModel();
        return true;
    }

    void SceneExplorerModuleController::showContextMenu(const QPoint& pos)
    {
        QTreeWidget* tree = m_widget.treeWidget();
        if(tree == nullptr) {
            return;
        }

        QTreeWidgetItem* item = SceneTreeIntentController::treeItemAtOrCurrent(tree, pos);
        if(item == nullptr) {
            return;
        }

        tree->setCurrentItem(item);
        if(!resolvePendingTransformPreviewIfTargetChanges(m_widget.currentNode(), tree)) {
            refreshViewModel();
            return;
        }
        if(!nodeSelectableForCurrentMode(m_widget.currentNode())) {
            emit statusMessageRequested(QStringLiteral("This item is not selectable in the current task."), 2500);
            return;
        }
        SceneTreeIntentController::ContextMenuModel model =
            SceneTreeIntentController::contextMenuModelFromItem(
                item,
                !m_context.document().collision.selectionSets.empty(),
                sceneExplorerActionScopeForWorkbench(m_workbenchDescriptor));
        if(model.actions.empty()) {
            return;
        }

        for(SceneTreeIntentController::ContextMenuActionView& actionView : model.actions) {
            if(actionView.action.kind == SceneTreeIntentController::ContextMenuActionKind::ShowLinkFrame) {
                actionView.checkable = true;
                actionView.checked = linkFrameVisible(actionView.action.node.id, actionView.action.node.linkName);
            }
        }

        QMenu menu(tree);
        for(const SceneTreeIntentController::ContextMenuActionView& actionView : model.actions) {
            if(actionView.separatorBefore && !menu.actions().empty()) {
                menu.addSeparator();
            }
            QAction* action = menu.addAction(actionView.label);
            action->setEnabled(actionView.enabled);
            action->setCheckable(actionView.checkable);
            action->setChecked(actionView.checked);
            connect(action, &QAction::triggered, this, [this, actionView]() {
                emit contextMenuActionRequested(actionView.action);
            });
        }
        menu.exec(tree->viewport()->mapToGlobal(pos));
    }

    QString SceneExplorerModuleController::linkFrameKey(
        const QString& robotId,
        const QString& linkName) const
    {
        return robotId + QStringLiteral("\n") + linkName;
    }
}
