#include "TcpDefinitionDialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>

namespace { constexpr double pi = 3.14159265358979323846; }
Eigen::Isometry3d tcpTransformMatrix(const simulation_project::TransformDesc& pose)
{
    Eigen::Isometry3d result = Eigen::Isometry3d::Identity();
    result.translation() = Eigen::Vector3d(pose.x, pose.y, pose.z);
    result.linear() = (Eigen::AngleAxisd(pose.yaw, Eigen::Vector3d::UnitZ()) *
        Eigen::AngleAxisd(pose.pitch, Eigen::Vector3d::UnitY()) *
        Eigen::AngleAxisd(pose.roll, Eigen::Vector3d::UnitX())).toRotationMatrix();
    return result;
}
simulation_project::TransformDesc tcpTransformDesc(const Eigen::Isometry3d& pose)
{
    simulation_project::TransformDesc result;
    result.x = pose.translation().x(); result.y = pose.translation().y(); result.z = pose.translation().z();
    const Eigen::Vector3d angles = pose.linear().eulerAngles(2, 1, 0);
    result.yaw = angles[0]; result.pitch = angles[1]; result.roll = angles[2];
    return result;
}
bool tcpPoseFromValues(const std::string& convention, const std::array<double, 7>& values, Eigen::Isometry3d& pose)
{
    for(double value : values) if(!std::isfinite(value)) return false;
    if(convention != "fanuc" && convention != "abb" && convention != "generic") return false;
    pose = Eigen::Isometry3d::Identity();
    pose.translation() = Eigen::Vector3d(values[0], values[1], values[2]) * .001;
    if(convention == "abb") {
        Eigen::Quaterniond q(values[3], values[4], values[5], values[6]);
        if(q.norm() < 1e-12) return false;
        pose.linear() = q.normalized().toRotationMatrix();
    } else {
        pose.linear() = (Eigen::AngleAxisd(values[5] * pi / 180, Eigen::Vector3d::UnitZ()) *
            Eigen::AngleAxisd(values[4] * pi / 180, Eigen::Vector3d::UnitY()) *
            Eigen::AngleAxisd(values[3] * pi / 180, Eigen::Vector3d::UnitX())).toRotationMatrix();
    }
    return true;
}
std::array<double, 7> tcpPoseToValues(const std::string& convention, const Eigen::Isometry3d& pose)
{
    std::array<double, 7> values{};
    for(int i = 0; i < 3; ++i) values[i] = pose.translation()[i] * 1000;
    if(convention == "abb") {
        Eigen::Quaterniond q(pose.linear()); q.normalize();
        if(q.w() < 0) q.coeffs() *= -1;
        values[3] = q.w(); values[4] = q.x(); values[5] = q.y(); values[6] = q.z();
    } else {
        const Eigen::Vector3d angles = pose.linear().eulerAngles(2, 1, 0);
        values[3] = angles[2] * 180 / pi; values[4] = angles[1] * 180 / pi; values[5] = angles[0] * 180 / pi;
    }
    return values;
}
TcpDefinitionDialog::TcpDefinitionDialog(const simulation_project::TransformDesc& assetToTcp,
    const std::map<std::string, Eigen::Isometry3d>& referenceToAsset,
    const QString& conventionName, const QString& reference, QWidget* parent)
    : QDialog(parent), m_referenceToAsset(referenceToAsset), m_assetToTcp(tcpTransformMatrix(assetToTcp))
{
    setWindowTitle(QStringLiteral("\u5de5\u5177 TCP \u5b9a\u4e49"));
    setMinimumWidth(520);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    m_convention = new QComboBox(this);
    m_convention->setObjectName("tcpConvention");
    m_convention->addItem("FANUC - XYZ / W P R (mm, deg)", "fanuc");
    m_convention->addItem("ABB - XYZ / q1 q2 q3 q4 (mm, w x y z)", "abb");
    m_convention->addItem(QStringLiteral("\u901a\u7528 - XYZ / Roll Pitch Yaw (mm, deg)"), "generic");
    m_reference = new QComboBox(this);
    m_reference->setObjectName("tcpReference");
    for(const auto& entry : referenceToAsset) m_reference->addItem(QString::fromStdString(entry.first));
    form->addRow(QStringLiteral("\u5b9a\u4e49\u65b9\u5f0f"), m_convention);
    form->addRow(QStringLiteral("\u53c2\u8003\u8fde\u6746\u5750\u6807\u7cfb"), m_reference);
    for(int i = 0; i < 7; ++i) {
        m_fields[i] = new QDoubleSpinBox(this);
        m_fields[i]->setObjectName(QString("tcpValue%1").arg(i));
        m_fields[i]->setRange(-1e9, 1e9);
        m_fields[i]->setDecimals(8);
        m_labels[i] = new QLabel(this);
        form->addRow(m_labels[i], m_fields[i]);
    }
    layout->addLayout(form);
    auto* hint = new QLabel(QStringLiteral("TCP \u76f8\u5bf9\u6240\u9009\u53c2\u8003\u8fde\u6746\u5b9a\u4e49\uff0c\u81ea\u52a8\u6362\u7b97\u5b89\u88c5\u4f4d\u504f\u79fb\u3002\n\u5207\u6362\u5b9a\u4e49\u65b9\u5f0f\u4e0e\u53c2\u8003\u7cfb\u65f6\u4fdd\u6301 TCP \u4f4d\u59ff\u4e0d\u53d8\u3002\n\u4fdd\u5b58\u540e\u6b64\u5b89\u88c5\u5b9e\u4f8b\u5c06\u8bbe\u4e3a\u5de5\u5177\u5e76\u6fc0\u6d3b\u5176 TCP\u3002"), this);
    m_hint=hint;
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setObjectName("definitionButtons");
    layout->addWidget(buttons);
    const int modeIndex = m_convention->findData(conventionName);
    m_convention->setCurrentIndex(modeIndex < 0 ? 2 : modeIndex);
    const int referenceIndex = m_reference->findText(reference);
    m_reference->setCurrentIndex(referenceIndex < 0 ? 0 : referenceIndex);
    m_displayConvention = convention(); m_displayReference = referenceLink();
    displayFields();
    connect(m_convention, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { changeDefinition(); });
    connect(m_reference, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] { changeDefinition(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if(!readFields()) return;
        if(m_applyUserFrame) {
            if(m_applyUserFrame(this->assetToTcp())) m_hint->setText(QStringLiteral("\u5df2\u5e94\u7528\u3002\u7a97\u53e3\u4fdd\u6301\u6253\u5f00\uff0c\u53ef\u7ee7\u7eed\u8c03\u6574\uff1b\u4fdd\u5b58\u9879\u76ee\u540e\u4fdd\u7559\u7528\u6237\u5750\u6807\u7cfb\u3002"));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
bool TcpDefinitionDialog::readFields()
{
    std::array<double, 7> values{};
    for(int i = 0; i < 7; ++i) { m_fields[i]->interpretText(); values[i] = m_fields[i]->value(); }
    Eigen::Isometry3d referenceToTcp;
    const auto reference = m_referenceToAsset.find(m_displayReference.toStdString());
    if(reference == m_referenceToAsset.end() || !tcpPoseFromValues(m_displayConvention.toStdString(), values, referenceToTcp)) {
        QMessageBox::warning(this, "TCP", QStringLiteral("\u8bf7\u8f93\u5165\u6709\u6548 TCP\uff1bABB \u56db\u5143\u6570\u4e0d\u80fd\u5168\u4e3a 0\u3002"));
        return false;
    }
    m_assetToTcp = reference->second.inverse() * referenceToTcp;
    return true;
}
void TcpDefinitionDialog::displayFields()
{
    const auto reference = m_referenceToAsset.find(referenceLink().toStdString());
    if(reference == m_referenceToAsset.end()) return;
    const auto values = tcpPoseToValues(convention().toStdString(), reference->second * m_assetToTcp);
    const bool abb = convention() == "abb";
    const char* labels[7] = {"X (mm)", "Y (mm)", "Z (mm)", "Roll (deg)", "Pitch (deg)", "Yaw (deg)", ""};
    const char* fanuc[7] = {"X (mm)", "Y (mm)", "Z (mm)", "W (deg)", "P (deg)", "R (deg)", ""};
    const char* abbLabels[7] = {"X (mm)", "Y (mm)", "Z (mm)", "q1 (w)", "q2 (x)", "q3 (y)", "q4 (z)"};
    for(int i = 0; i < 7; ++i) {
        m_labels[i]->setText(abb ? abbLabels[i] : convention() == "fanuc" ? fanuc[i] : labels[i]);
        m_fields[i]->setValue(values[i]);
        m_labels[i]->setVisible(i < 6 || abb); m_fields[i]->setVisible(i < 6 || abb);
    }
}
void TcpDefinitionDialog::changeDefinition()
{
    if(!readFields()) {
        QSignalBlocker a(m_convention), b(m_reference);
        m_convention->setCurrentIndex(m_convention->findData(m_displayConvention));
        m_reference->setCurrentText(m_displayReference);
        return;
    }
    m_displayConvention = convention(); m_displayReference = referenceLink();
    displayFields();
}
simulation_project::TransformDesc TcpDefinitionDialog::assetToTcp() const { return tcpTransformDesc(m_assetToTcp); }
QString TcpDefinitionDialog::convention() const { return m_convention->currentData().toString(); }
QString TcpDefinitionDialog::referenceLink() const { return m_reference->currentText(); }

void TcpDefinitionDialog::setUserFrameApplyHandler(std::function<bool(const simulation_project::TransformDesc&)> handler)
{
    m_applyUserFrame=std::move(handler);
    auto* buttons=findChild<QDialogButtonBox*>("definitionButtons");
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("\u786e\u5b9a\u5e94\u7528"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("\u5173\u95ed"));
    m_hint->setText(QStringLiteral("\u7528\u6237\u5750\u6807\u7cfb\u76f8\u5bf9\u4e8e WORLD \u5b9a\u4e49\u3002\u786e\u5b9a\u5e94\u7528\u540e\u7a97\u53e3\u4fdd\u6301\u6253\u5f00\uff0c\u5de5\u4ef6\u548c\u8f68\u8ff9\u540c\u6b65\u79fb\u52a8\uff0c\u89c6\u89d2\u4fdd\u6301\u4e0d\u53d8\u3002\u4fdd\u5b58\u9879\u76ee\u540e\u4fdd\u7559\u5750\u6807\u4fe1\u606f\u3002"));
}
