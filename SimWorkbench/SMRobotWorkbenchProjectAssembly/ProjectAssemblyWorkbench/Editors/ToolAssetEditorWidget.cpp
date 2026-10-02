#include "ToolAssetEditorWidget.h"

#include "RobotQtWidgetUtils.h"
#include "ToolTransformEditorWidget.h"

#include <Eigen/Geometry>

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <vector>

namespace
{
    QDoubleSpinBox* makeScaleSpin(QWidget* parent)
    {
        auto* spin = new QDoubleSpinBox(parent);
        spin->setRange(0.0001, 100000.0);
        spin->setDecimals(6);
        spin->setSingleStep(0.1);
        spin->setMinimumHeight(28);
        return spin;
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

    simulation_project::TransformDesc assetTcpTransform(
        const simulation_project::AttachmentAssetDesc& asset)
    {
        for(const simulation_project::AttachmentFunctionalFrameDesc& frame : asset.functionalFrames) {
            if(frame.primary || frame.frameType == "tcp") {
                return frame.assetMountToFrame;
            }
        }
        return simulation_project::TransformDesc();
    }

    simulation_project::TransformDesc makeModelToFlangeTransform(
        const simulation_project::AttachmentAssetDesc& asset)
    {
        return makeTransformDesc(makeTransform(asset.assetMountToVisual).inverse());
    }

    simulation_project::TransformDesc makeModelToTcpTransform(
        const simulation_project::AttachmentAssetDesc& asset)
    {
        const Eigen::Isometry3d mountToVisual = makeTransform(asset.assetMountToVisual);
        const Eigen::Isometry3d mountToTcp = makeTransform(assetTcpTransform(asset));
        return makeTransformDesc(mountToVisual.inverse() * mountToTcp);
    }
}

class ToolFrameDiagramWidget : public QWidget
{
public:
    explicit ToolFrameDiagramWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(170);
        setMaximumHeight(170);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setAssetName(const QString& assetName)
    {
        m_assetName = assetName;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF area = rect().adjusted(1.0, 1.0, -1.0, -1.0);
        const QColor borderColor(82, 96, 112);
        const QColor nodeBorderColor(82, 156, 238);
        const QColor nodeFillColor(22, 34, 48);
        const QColor textColor(226, 234, 244);
        const QColor mutedColor(130, 144, 160);

        painter.fillRect(area, palette().base());
        painter.setPen(QPen(borderColor, 1.0));
        painter.setBrush(palette().base());
        painter.drawRoundedRect(area, 6.0, 6.0);

        const qreal margin = 16.0;
        const qreal nodeWidth = qMin<qreal>(150.0, qMax<qreal>(104.0, (area.width() - margin * 2.0 - 44.0) / 3.0));
        const qreal nodeHeight = 42.0;
        const qreal y = area.top() + 44.0;
        const qreal gap = qMax<qreal>(22.0, (area.width() - margin * 2.0 - nodeWidth * 3.0) / 2.0);
        const QRectF visualRect(area.left() + margin, y, nodeWidth, nodeHeight);
        const QRectF flangeRect(visualRect.right() + gap, y, nodeWidth, nodeHeight);
        const QRectF tcpRect(flangeRect.right() + gap, y, nodeWidth, nodeHeight);

        drawNode(painter, visualRect, m_assetName.isEmpty() ? QStringLiteral("Model / Visual") : m_assetName, false, nodeFillColor, nodeBorderColor, textColor);
        drawNode(painter, flangeRect, QStringLiteral("Tool flange"), true, nodeFillColor, nodeBorderColor, textColor);
        drawNode(painter, tcpRect, QStringLiteral("TCP"), true, nodeFillColor, nodeBorderColor, textColor);
        drawArrow(painter, visualRect, flangeRect, mutedColor);
        drawArrow(painter, flangeRect, tcpRect, mutedColor);
    }

private:
    void drawNode(
        QPainter& painter,
        const QRectF& rect,
        const QString& text,
        bool frameNode,
        const QColor& fillColor,
        const QColor& borderColor,
        const QColor& textColor) const
    {
        painter.setPen(QPen(borderColor, 1.6));
        painter.setBrush(fillColor);
        if(frameNode) {
            painter.drawEllipse(rect);
        } else {
            painter.drawRoundedRect(rect, 8.0, 8.0);
        }

        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(8);
        painter.setFont(font);
        painter.setPen(textColor);
        const QFontMetrics metrics(font);
        painter.drawText(
            rect.adjusted(8.0, 4.0, -8.0, -4.0),
            Qt::AlignCenter,
            metrics.elidedText(text, Qt::ElideMiddle, static_cast<int>(rect.width() - 16.0)));
    }

    void drawArrow(QPainter& painter, const QRectF& from, const QRectF& to, const QColor& color) const
    {
        const QPointF start(from.right(), from.center().y());
        const QPointF end(to.left(), to.center().y());
        painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(start, end);

        const qreal arrowSize = 6.0;
        painter.drawLine(end, QPointF(end.x() - arrowSize, end.y() - arrowSize));
        painter.drawLine(end, QPointF(end.x() - arrowSize, end.y() + arrowSize));
    }

    QString m_assetName;
};

ToolAssetEditorWidget::ToolAssetEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("attachmentAssetEditorTabs"));
    auto* definitionPage = new QWidget(m_tabs);
    auto* definitionLayout = new QVBoxLayout(definitionPage);
    definitionLayout->setContentsMargins(8, 8, 8, 8);
    definitionLayout->setSpacing(10);
    auto* appearancePage = new QWidget(m_tabs);
    auto* appearanceLayout = new QVBoxLayout(appearancePage);
    appearanceLayout->setContentsMargins(8, 8, 8, 8);
    appearanceLayout->setSpacing(10);
    auto* diagnosticsPage = new QWidget(m_tabs);
    auto* diagnosticsLayout = new QVBoxLayout(diagnosticsPage);
    diagnosticsLayout->setContentsMargins(8, 8, 8, 8);
    diagnosticsLayout->setSpacing(10);

    m_idEdit = new QLineEdit(this);
    m_idEdit->setReadOnly(true);
    m_idEdit->setToolTip("Stable internal id. It is used by project references and is not renamed here.");
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setToolTip("Display name shown in the Tool panel and Scene Explorer.");
    m_typeEdit = new QLineEdit(this);
    m_visualPathEdit = new QLineEdit(this);
    m_visualScaleSpin = makeScaleSpin(this);
    m_visualPathEdit->setMinimumHeight(28);
    m_nameEdit->setMinimumHeight(28);
    m_typeEdit->setMinimumHeight(28);
    m_idEdit->setMinimumHeight(28);

    auto* definitionForm = new QFormLayout();
    definitionForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    definitionForm->setVerticalSpacing(8);
    definitionForm->addRow("Display name", m_nameEdit);
    definitionLayout->addLayout(definitionForm);

    auto* appearanceForm = new QFormLayout();
    appearanceForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    appearanceForm->setVerticalSpacing(8);
    appearanceForm->addRow("Visual path", m_visualPathEdit);
    appearanceForm->addRow("Visual scale", m_visualScaleSpin);
    appearanceLayout->addLayout(appearanceForm);

    auto* diagnosticsForm = new QFormLayout();
    diagnosticsForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    diagnosticsForm->setVerticalSpacing(8);
    diagnosticsForm->addRow("Internal id", m_idEdit);
    diagnosticsForm->addRow("Type", m_typeEdit);
    diagnosticsLayout->addLayout(diagnosticsForm);

    m_sharedAssetHint = new QLabel(
        "Library asset settings are shared by every tool attachment that references this asset. Use attachment offset for per-robot instance changes.",
        diagnosticsPage);
    m_sharedAssetHint->setWordWrap(true);
    diagnosticsLayout->addWidget(m_sharedAssetHint);

    m_frameDiagram = new ToolFrameDiagramWidget(diagnosticsPage);
    diagnosticsLayout->addWidget(m_frameDiagram);

    m_visualTransformEditor = new ToolTransformEditorWidget("Model/world -> Tool flange {F}", appearancePage);
    m_tcpTransformEditor = new ToolTransformEditorWidget("Model/world -> TCP {TCP}", definitionPage);
    m_visualTransformEditor->setMatrixVisible(false);
    m_tcpTransformEditor->setMatrixVisible(false);
    appearanceLayout->addWidget(m_visualTransformEditor);
    definitionLayout->addWidget(m_tcpTransformEditor);

    m_cameraGroup = new QGroupBox("Camera Intrinsics", definitionPage);
    auto* cameraLayout = new QFormLayout(m_cameraGroup);
    m_cameraWidthSpin = new QSpinBox(m_cameraGroup);
    m_cameraWidthSpin->setRange(1, 16384);
    m_cameraHeightSpin = new QSpinBox(m_cameraGroup);
    m_cameraHeightSpin->setRange(1, 16384);
    m_cameraFovYSpin = new QDoubleSpinBox(m_cameraGroup);
    m_cameraFovYSpin->setRange(1.0, 179.0);
    m_cameraFovYSpin->setDecimals(3);
    m_cameraNearSpin = new QDoubleSpinBox(m_cameraGroup);
    m_cameraNearSpin->setRange(0.0001, 10000.0);
    m_cameraNearSpin->setDecimals(4);
    m_cameraFarSpin = new QDoubleSpinBox(m_cameraGroup);
    m_cameraFarSpin->setRange(0.001, 1000000.0);
    m_cameraFarSpin->setDecimals(3);
    cameraLayout->addRow("Width", m_cameraWidthSpin);
    cameraLayout->addRow("Height", m_cameraHeightSpin);
    cameraLayout->addRow("Vertical FOV", m_cameraFovYSpin);
    cameraLayout->addRow("Near plane", m_cameraNearSpin);
    cameraLayout->addRow("Far plane", m_cameraFarSpin);
    m_cameraGroup->hide();
    definitionLayout->insertWidget(1, m_cameraGroup);

    definitionLayout->addStretch(1);
    appearanceLayout->addStretch(1);
    diagnosticsLayout->addStretch(1);
    m_tabs->addTab(definitionPage, QStringLiteral("Definition"));
    m_tabs->addTab(appearancePage, QStringLiteral("Appearance"));
    m_tabs->addTab(diagnosticsPage, QStringLiteral("Diagnostics"));
    layout->addWidget(m_tabs);

    m_applyButton = new QPushButton("Apply Tool Asset", this);
    robot_qt_viewer::configureActionButton(
        m_applyButton,
        robot_qt_viewer::UiActionRole::Primary);
    m_applyButton->setVisible(false);
    layout->addWidget(m_applyButton);

    connect(m_nameEdit, &QLineEdit::editingFinished, this, &ToolAssetEditorWidget::emitAssetChanged);
    connect(m_typeEdit, &QLineEdit::editingFinished, this, &ToolAssetEditorWidget::emitAssetChanged);
    connect(m_visualPathEdit, &QLineEdit::editingFinished, this, &ToolAssetEditorWidget::emitAssetChanged);
    connect(
        m_visualScaleSpin,
        static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this,
        [this](double) {
            emitAssetChanged();
        });
    connect(
        m_visualTransformEditor,
        &ToolTransformEditorWidget::transformChanged,
        this,
        &ToolAssetEditorWidget::emitVisualTransformChanged);
    connect(
        m_tcpTransformEditor,
        &ToolTransformEditorWidget::transformChanged,
        this,
        &ToolAssetEditorWidget::emitTcpTransformChanged);
    const auto cameraChanged = [this]() { emitAssetChanged(); };
    connect(m_cameraWidthSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [cameraChanged](int) { cameraChanged(); });
    connect(m_cameraHeightSpin, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
        [cameraChanged](int) { cameraChanged(); });
    connect(m_cameraFovYSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
        [cameraChanged](double) { cameraChanged(); });
    connect(m_cameraNearSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
        [cameraChanged](double) { cameraChanged(); });
    connect(m_cameraFarSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
        [cameraChanged](double) { cameraChanged(); });
    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        storeUiToAsset();
        if(m_applyButton != nullptr) {
            m_applyButton->setEnabled(false);
        }
        emit applyRequested(m_asset);
    });
}

void ToolAssetEditorWidget::setAsset(const simulation_project::AttachmentAssetDesc& asset)
{
    m_asset = asset;
    loadAssetToUi();
}

simulation_project::AttachmentAssetDesc ToolAssetEditorWidget::asset() const
{
    return collectUiAsset();
}

void ToolAssetEditorWidget::setApplyButtonVisible(bool visible)
{
    if(m_applyButton != nullptr) {
        m_applyButton->setVisible(visible);
        m_applyButton->setEnabled(false);
    }
}

void ToolAssetEditorWidget::loadAssetToUi()
{
    m_updating = true;

    const QList<QObject*> blockers = {
        m_idEdit,
        m_nameEdit,
        m_typeEdit,
        m_visualPathEdit,
        m_visualScaleSpin,
        m_visualTransformEditor,
        m_tcpTransformEditor,
        m_cameraWidthSpin,
        m_cameraHeightSpin,
        m_cameraFovYSpin,
        m_cameraNearSpin,
        m_cameraFarSpin
    };
    std::vector<std::unique_ptr<QSignalBlocker>> signalBlockers;
    signalBlockers.reserve(static_cast<std::size_t>(blockers.size()));
    for(QObject* object : blockers) {
        if(object != nullptr) {
            signalBlockers.push_back(std::make_unique<QSignalBlocker>(object));
        }
    }

    m_idEdit->setText(QString::fromStdString(m_asset.id));
    m_nameEdit->setText(QString::fromStdString(m_asset.name));
    m_typeEdit->setText(QString::fromStdString(m_asset.assetType));
    m_visualPathEdit->setText(QString::fromStdString(m_asset.visualPath));
    const bool cameraAsset = m_asset.assetKind == "sensor" && m_asset.assetType == "camera";
    m_visualScaleSpin->setValue(m_asset.visualScale);
    m_cameraGroup->setVisible(cameraAsset);
    m_frameDiagram->setVisible(!cameraAsset);
    m_sharedAssetHint->setText(cameraAsset
        ? QStringLiteral("Camera definition settings are shared by every installed instance. Use the mounted instance offset to adjust one installation.")
        : QStringLiteral("Tool definition settings are shared by every installed instance. Use the mounted instance offset to adjust one installation."));
    m_nameEdit->setToolTip(cameraAsset
        ? QStringLiteral("Camera definition name shown in Sensors and installed-device lists.")
        : QStringLiteral("Tool definition name shown in the Tool panel and Scene Explorer."));
    if(m_applyButton != nullptr) {
        m_applyButton->setText(cameraAsset
            ? QStringLiteral("Apply Camera Definition")
            : QStringLiteral("Apply Tool Definition"));
    }
    m_visualTransformEditor->setTitle(cameraAsset
        ? QStringLiteral("Camera mount -> Visual")
        : QStringLiteral("Model/world -> Tool flange {F}"));
    m_tcpTransformEditor->setTitle(cameraAsset
        ? QStringLiteral("Camera mount -> Optical frame")
        : QStringLiteral("Model/world -> TCP {TCP}"));
    m_cameraWidthSpin->setValue(m_asset.sensorIntrinsics.width);
    m_cameraHeightSpin->setValue(m_asset.sensorIntrinsics.height);
    m_cameraFovYSpin->setValue(m_asset.sensorIntrinsics.fovY);
    m_cameraNearSpin->setValue(m_asset.sensorIntrinsics.nearPlane);
    m_cameraFarSpin->setValue(m_asset.sensorIntrinsics.farPlane);
    m_visualTransformEditor->setTransform(cameraAsset
        ? m_asset.assetMountToVisual
        : makeModelToFlangeTransform(m_asset));
    m_tcpTransformEditor->setTransform(cameraAsset
        ? assetTcpTransform(m_asset)
        : makeModelToTcpTransform(m_asset));
    if(m_frameDiagram != nullptr) {
        const QString assetName = QString::fromStdString(m_asset.name.empty() ? m_asset.id : m_asset.name);
        m_frameDiagram->setAssetName(assetName);
    }
    if(m_applyButton != nullptr) {
        m_applyButton->setEnabled(false);
    }

    m_updating = false;
}

simulation_project::AttachmentAssetDesc ToolAssetEditorWidget::collectUiAsset() const
{
    simulation_project::AttachmentAssetDesc value = m_asset;
    value.name = m_nameEdit->text().trimmed().toStdString();
    value.assetKind = value.assetKind.empty() ? "tool" : value.assetKind;
    value.assetType = m_typeEdit->text().trimmed().toStdString();
    value.visualPath = m_visualPathEdit->text().trimmed().toStdString();
    value.visualScale = m_visualScaleSpin->value();

    const bool cameraAsset = value.assetKind == "sensor" && value.assetType == "camera";
    simulation_project::TransformDesc functionalFrameTransform;
    if(cameraAsset) {
        value.assetMountToVisual = m_visualTransformEditor->transform();
        functionalFrameTransform = m_tcpTransformEditor->transform();
    } else {
        const Eigen::Isometry3d modelToFlange = makeTransform(m_visualTransformEditor->transform());
        const Eigen::Isometry3d modelToTcp = makeTransform(m_tcpTransformEditor->transform());
        const Eigen::Isometry3d flangeToModel = modelToFlange.inverse();
        const Eigen::Isometry3d flangeToTcp = flangeToModel * modelToTcp;
        value.assetMountToVisual = makeTransformDesc(flangeToModel);
        functionalFrameTransform = makeTransformDesc(flangeToTcp);
    }

    simulation_project::AttachmentFunctionalFrameDesc tcpFrame;
    tcpFrame.id = value.id + (cameraAsset ? ".optical" : ".tcp");
    tcpFrame.name = cameraAsset ? "Optical" : "TCP";
    tcpFrame.frameType = cameraAsset ? "optical" : "tcp";
    tcpFrame.assetMountToFrame = functionalFrameTransform;
    tcpFrame.primary = true;
    value.functionalFrames.erase(
        std::remove_if(
            value.functionalFrames.begin(),
            value.functionalFrames.end(),
            [&](const simulation_project::AttachmentFunctionalFrameDesc& frame) {
                return frame.primary || frame.frameType == "tcp" ||
                    frame.frameType == "optical";
            }),
        value.functionalFrames.end());
    value.functionalFrames.push_back(tcpFrame);
    if(cameraAsset) {
        value.hasSensorIntrinsics = true;
        value.sensorIntrinsics.model = "pinhole";
        value.sensorIntrinsics.width = m_cameraWidthSpin->value();
        value.sensorIntrinsics.height = m_cameraHeightSpin->value();
        value.sensorIntrinsics.fovY = m_cameraFovYSpin->value();
        value.sensorIntrinsics.nearPlane = m_cameraNearSpin->value();
        value.sensorIntrinsics.farPlane = m_cameraFarSpin->value();
    }
    return value;
}

void ToolAssetEditorWidget::storeUiToAsset()
{
    m_asset = collectUiAsset();
}

void ToolAssetEditorWidget::emitAssetChanged()
{
    if(m_updating) {
        return;
    }
    storeUiToAsset();
    if(m_applyButton != nullptr && m_applyButton->isVisible()) {
        m_applyButton->setEnabled(true);
    }
    emit assetChanged(m_asset);
}

void ToolAssetEditorWidget::emitVisualTransformChanged()
{
    if(m_updating) {
        return;
    }
    storeUiToAsset();
    if(m_applyButton != nullptr && m_applyButton->isVisible()) {
        m_applyButton->setEnabled(true);
    }
    emit visualTransformChanged(m_asset);
}

void ToolAssetEditorWidget::emitTcpTransformChanged()
{
    if(m_updating) {
        return;
    }
    storeUiToAsset();
    if(m_applyButton != nullptr && m_applyButton->isVisible()) {
        m_applyButton->setEnabled(true);
    }
    emit tcpTransformChanged(m_asset);
}
