#pragma once
#include <QDialog>
#include <SimulationProject/ProjectDocument.h>
#include <Eigen/Geometry>
#include <array>
#include <functional>
#include <map>
#include <string>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

Eigen::Isometry3d tcpTransformMatrix(const simulation_project::TransformDesc& pose);
simulation_project::TransformDesc tcpTransformDesc(const Eigen::Isometry3d& pose);
// FANUC: mm + WPR degrees (Rz(R) Ry(P) Rx(W)); ABB: mm + [qw,qx,qy,qz].
// Generic: mm + RPY degrees (Rz(yaw) Ry(pitch) Rx(roll)).
bool tcpPoseFromValues(const std::string& convention, const std::array<double, 7>& values,
    Eigen::Isometry3d& pose);
std::array<double, 7> tcpPoseToValues(const std::string& convention, const Eigen::Isometry3d& pose);

class TcpDefinitionDialog final : public QDialog {
public:
    TcpDefinitionDialog(const simulation_project::TransformDesc& assetToTcp,
        const std::map<std::string, Eigen::Isometry3d>& referenceToAsset,
        const QString& convention, const QString& reference, QWidget* parent = nullptr);
    simulation_project::TransformDesc assetToTcp() const;
    QString convention() const;
    QString referenceLink() const;
    void setUserFrameApplyHandler(std::function<bool(const simulation_project::TransformDesc&)> handler);
private:
    std::function<bool(const simulation_project::TransformDesc&)> m_applyUserFrame;
    QLabel* m_hint = nullptr;
    bool readFields();
    void displayFields();
    void changeDefinition();
    QComboBox* m_convention = nullptr;
    QComboBox* m_reference = nullptr;
    std::array<QDoubleSpinBox*, 7> m_fields{};
    std::array<QLabel*, 7> m_labels{};
    std::map<std::string, Eigen::Isometry3d> m_referenceToAsset;
    Eigen::Isometry3d m_assetToTcp;
    QString m_displayConvention;
    QString m_displayReference;
};
