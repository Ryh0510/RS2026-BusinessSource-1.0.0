#include "ToolTransformEditorWidget.h"

#include "RobotQtWidgetUtils.h"
#include "TcpDefinitionDialog.h"
#include <QComboBox>

#include <QDoubleSpinBox>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    double toDegrees(double radians)
    {
        return radians * 180.0 / kPi;
    }

    double toRadians(double degrees)
    {
        return degrees * kPi / 180.0;
    }

    std::array<double, 16> transformMatrixValues(const simulation_project::TransformDesc& transform)
    {
        const double cr = std::cos(transform.roll);
        const double sr = std::sin(transform.roll);
        const double cp = std::cos(transform.pitch);
        const double sp = std::sin(transform.pitch);
        const double cy = std::cos(transform.yaw);
        const double sy = std::sin(transform.yaw);

        return {
            cy * cp,
            cy * sp * sr - sy * cr,
            cy * sp * cr + sy * sr,
            transform.x,
            sy * cp,
            sy * sp * sr + cy * cr,
            sy * sp * cr - cy * sr,
            transform.y,
            -sp,
            cp * sr,
            cp * cr,
            transform.z,
            0.0,
            0.0,
            0.0,
            1.0
        };
    }

    QDoubleSpinBox* makeDistanceSpin(QWidget* parent)
    {
        auto* spin = new QDoubleSpinBox(parent);
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(4);
        spin->setSingleStep(0.01);
        spin->setSuffix(" m");
        spin->setKeyboardTracking(false);
        robot_qt_viewer::makeHorizontallyCompressible(spin);
        return spin;
    }

    QDoubleSpinBox* makeAngleSpin(QWidget* parent)
    {
        auto* spin = new QDoubleSpinBox(parent);
        spin->setRange(-360.0, 360.0);
        spin->setDecimals(3);
        spin->setSingleStep(1.0);
        spin->setSuffix(" deg");
        spin->setKeyboardTracking(false);
        robot_qt_viewer::makeHorizontallyCompressible(spin);
        return spin;
    }

    void addTransformRows(
        QGridLayout* layout,
        QWidget* parent,
        QDoubleSpinBox* xSpin,
        QDoubleSpinBox* ySpin,
        QDoubleSpinBox* zSpin,
        QDoubleSpinBox* rollSpin,
        QDoubleSpinBox* pitchSpin,
        QDoubleSpinBox* yawSpin)
    {
        layout->addWidget(new QLabel("X", parent), 0, 0);
        layout->addWidget(xSpin, 0, 1);
        layout->addWidget(new QLabel("Y", parent), 1, 0);
        layout->addWidget(ySpin, 1, 1);
        layout->addWidget(new QLabel("Z", parent), 2, 0);
        layout->addWidget(zSpin, 2, 1);
        layout->addWidget(new QLabel("Theta", parent), 3, 0);
        layout->addWidget(rollSpin, 3, 1);
        layout->addWidget(new QLabel("Phi", parent), 4, 0);
        layout->addWidget(pitchSpin, 4, 1);
        layout->addWidget(new QLabel("Psi", parent), 5, 0);
        layout->addWidget(yawSpin, 5, 1);
        layout->setColumnStretch(1, 1);
    }
}

ToolTransformEditorWidget::ToolTransformEditorWidget(const QString& title, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_titleLabel = robot_qt_viewer::makePanelTitle(title, this);
    layout->addWidget(m_titleLabel);

    m_matrixGroup = new QGroupBox(QStringLiteral("Matrix 4x4"), this);
    auto* matrixGrid = new QGridLayout(m_matrixGroup);
    matrixGrid->setContentsMargins(8, 24, 8, 8);
    matrixGrid->setHorizontalSpacing(10);
    matrixGrid->setVerticalSpacing(4);
    for(int row = 0; row < 4; ++row) {
        for(int column = 0; column < 4; ++column) {
            QLabel* label = new QLabel(QStringLiteral("0.0000"), m_matrixGroup);
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            QFont font(QStringLiteral("Consolas"));
            font.setStyleHint(QFont::Monospace);
            label->setFont(font);
            label->setMinimumWidth(68);
            label->setProperty("matrixCell", true);
            m_matrixLabels[static_cast<std::size_t>(row * 4 + column)] = label;
            matrixGrid->addWidget(label, row, column);
        }
    }
    layout->addWidget(m_matrixGroup);

    m_convention = new QComboBox(this);
    m_convention->setObjectName("tcpConvention");
    m_convention->addItem("Generic - XYZ / RPY (mm, deg)", "generic");
    m_convention->addItem("FANUC - XYZ / WPR (mm, deg)", "fanuc");
    m_convention->addItem("ABB - XYZ / quaternion (w, x, y, z)", "abb");
    m_convention->hide();
    layout->addWidget(m_convention);

    auto* parameterGroup = new QGroupBox(QStringLiteral("Local Transform"), this);
    auto* grid = new QGridLayout(parameterGroup);
    grid->setContentsMargins(8, 24, 2, 8);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(7);
    m_xSpin = makeDistanceSpin(this);
    m_ySpin = makeDistanceSpin(this);
    m_zSpin = makeDistanceSpin(this);
    m_rollSpin = makeAngleSpin(this);
    m_pitchSpin = makeAngleSpin(this);
    m_yawSpin = makeAngleSpin(this);
    addTransformRows(grid, parameterGroup, m_xSpin, m_ySpin, m_zSpin, m_rollSpin, m_pitchSpin, m_yawSpin);
    for(int i = 0; i < 3; ++i) m_rotationLabels[i] = qobject_cast<QLabel*>(grid->itemAtPosition(i + 3, 0)->widget());
    m_rotationLabels[3] = new QLabel("q4 (z)", parameterGroup);
    m_qzSpin = makeAngleSpin(this);
    m_qzSpin->setObjectName("tcpQuaternionZ");
    grid->addWidget(m_rotationLabels[3], 6, 0);
    grid->addWidget(m_qzSpin, 6, 1);
    m_rotationLabels[3]->hide(); m_qzSpin->hide();
    layout->addWidget(parameterGroup);

    const QList<QDoubleSpinBox*> spins = {
        m_xSpin,
        m_ySpin,
        m_zSpin,
        m_rollSpin,
        m_pitchSpin,
        m_yawSpin,
        m_qzSpin
    };
    for(QDoubleSpinBox* spin : spins) {
        connect(
            spin,
            static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            this,
            [this](double) {
                emitTransformChanged();
            });
    }
    connect(m_convention, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int) {
        simulation_project::TransformDesc pose;
        if(!readTcpTransform(pose)) {
            QSignalBlocker blocker(m_convention);
            m_convention->setCurrentIndex(m_convention->findData(m_displayConvention));
            return;
        }
        m_displayConvention = m_convention->currentData().toString();
        setTransform(pose);
    });
}

void ToolTransformEditorWidget::setTcpConventionEnabled(bool enabled)
{
    const auto pose = transform();
    m_displayConvention = enabled ? m_convention->currentData().toString() : QString();
    m_convention->setVisible(enabled);
    setTransform(pose);
}

bool ToolTransformEditorWidget::readTcpTransform(simulation_project::TransformDesc& pose) const
{
    Eigen::Isometry3d matrix;
    const bool valid = ::tcpPoseFromValues(m_displayConvention.toStdString(),
        {m_xSpin->value(), m_ySpin->value(), m_zSpin->value(), m_rollSpin->value(),
         m_pitchSpin->value(), m_yawSpin->value(), m_qzSpin->value()}, matrix);
    m_convention->setToolTip(valid ? QString() : QStringLiteral("Invalid TCP: quaternion must be nonzero."));
    if(valid) pose = ::tcpTransformDesc(matrix);
    return valid;
}

void ToolTransformEditorWidget::setTitle(const QString& title)
{
    if(m_titleLabel != nullptr) {
        m_titleLabel->setText(title);
    }
}

void ToolTransformEditorWidget::setMatrixVisible(bool visible)
{
    if(m_matrixGroup != nullptr) {
        m_matrixGroup->setVisible(visible);
    }
}

void ToolTransformEditorWidget::setTransform(const simulation_project::TransformDesc& transform)
{
    m_lastValidTransform = transform;
    const QList<QDoubleSpinBox*> spins = {
        m_xSpin,
        m_ySpin,
        m_zSpin,
        m_rollSpin,
        m_pitchSpin,
        m_yawSpin,
        m_qzSpin
    };
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    blockers.reserve(static_cast<std::size_t>(spins.size()));
    for(QDoubleSpinBox* spin : spins) {
        if(spin != nullptr) {
            blockers.push_back(std::make_unique<QSignalBlocker>(spin));
        }
    }

    if(!m_displayConvention.isEmpty()) {
        const bool abb = m_displayConvention == "abb";
        const bool fanuc = m_displayConvention == "fanuc";
        for(auto* spin : {m_xSpin, m_ySpin, m_zSpin}) {
            spin->setRange(-1e8, 1e8); spin->setDecimals(8); spin->setSuffix(" mm");
        }
        for(auto* spin : {m_rollSpin, m_pitchSpin, m_yawSpin, m_qzSpin}) {
            spin->setRange(abb ? -1.0 : -360.0, abb ? 1.0 : 360.0);
            spin->setDecimals(8); spin->setSuffix(abb ? "" : " deg");
            spin->setSingleStep(abb ? .01 : 1.0);
        }
        m_rotationLabels[0]->setText(abb ? "q1 (w)" : fanuc ? "W" : "Roll");
        m_rotationLabels[1]->setText(abb ? "q2 (x)" : fanuc ? "P" : "Pitch");
        m_rotationLabels[2]->setText(abb ? "q3 (y)" : fanuc ? "R" : "Yaw");
        m_rotationLabels[3]->setVisible(abb); m_qzSpin->setVisible(abb);
        const auto values = ::tcpPoseToValues(m_displayConvention.toStdString(), ::tcpTransformMatrix(transform));
        const std::array<QDoubleSpinBox*, 7> fields = {m_xSpin,m_ySpin,m_zSpin,m_rollSpin,m_pitchSpin,m_yawSpin,m_qzSpin};
        for(std::size_t i=0;i<fields.size();++i) fields[i]->setValue(values[i]);
        updateMatrix(transform);
        return;
    }
    for(auto* spin : {m_xSpin, m_ySpin, m_zSpin}) { spin->setSuffix(" m"); spin->setDecimals(4); }
    m_rotationLabels[3]->hide(); m_qzSpin->hide();
    m_xSpin->setValue(transform.x);
    m_ySpin->setValue(transform.y);
    m_zSpin->setValue(transform.z);
    m_rollSpin->setValue(toDegrees(transform.roll));
    m_pitchSpin->setValue(toDegrees(transform.pitch));
    m_yawSpin->setValue(toDegrees(transform.yaw));
    updateMatrix(transform);
}

simulation_project::TransformDesc ToolTransformEditorWidget::transform() const
{
    if(!m_displayConvention.isEmpty()) {
        auto pose = m_lastValidTransform;
        readTcpTransform(pose);
        return pose;
    }
    simulation_project::TransformDesc value;
    value.x = m_xSpin->value();
    value.y = m_ySpin->value();
    value.z = m_zSpin->value();
    value.roll = toRadians(m_rollSpin->value());
    value.pitch = toRadians(m_pitchSpin->value());
    value.yaw = toRadians(m_yawSpin->value());
    return value;
}

void ToolTransformEditorWidget::emitTransformChanged()
{
    if(!m_displayConvention.isEmpty()) {
        simulation_project::TransformDesc pose;
        if(!readTcpTransform(pose)) return;
    }
    const simulation_project::TransformDesc value = transform();
    m_lastValidTransform = value;
    updateMatrix(value);
    emit transformChanged(value);
}

void ToolTransformEditorWidget::updateMatrix(const simulation_project::TransformDesc& transform)
{
    const std::array<double, 16> values = transformMatrixValues(transform);
    for(std::size_t i = 0; i < values.size(); ++i) {
        QLabel* label = m_matrixLabels[i];
        if(label != nullptr) {
            label->setText(QString::number(values[i], 'f', 4));
        }
    }
}
