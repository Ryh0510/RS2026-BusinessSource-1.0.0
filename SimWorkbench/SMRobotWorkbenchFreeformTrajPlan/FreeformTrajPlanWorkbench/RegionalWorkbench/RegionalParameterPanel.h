#pragma once
#include "SlicingWorkbench/FreeformTrajPlanWorkbenchExport.h"
#include <SlicingTrajPlan/BladeRegionPlanner.h>
#include <QWidget>
#include <array>
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QLabel;
namespace smrobot::workbench::freeform {
class FREEFORM_TRAJ_PLAN_WORKBENCH_API RegionalParameterPanel final : public QWidget {
    Q_OBJECT
public:
    explicit RegionalParameterPanel(QWidget* parent=nullptr);
    blade_region::Parameters parameters() const;
    blade_region::Region selectedRegion() const;
    void setState(bool identified,bool references,bool paths,bool connected,bool busy);
    void setSummary(const QString& text);
    bool colorsEnabled() const;
    bool tcpPreview() const;
signals:
    void parametersEdited();
    void colorsChanged();
    void actionRequested(int action);
private:
    QComboBox* m_axis=nullptr;
    QComboBox* m_pathView=nullptr;
    QComboBox* m_region=nullptr;
    QComboBox* m_method=nullptr;
    std::array<blade_region::Method,5> m_methods{{blade_region::Method::Slicing,
        blade_region::Method::ReferenceOffsets,blade_region::Method::ReferenceOffsets,
        blade_region::Method::Slicing,blade_region::Method::Slicing}};
    std::array<QDoubleSpinBox*,12> m_values{};
    std::array<QPushButton*,6> m_actions{};
    QPushButton* m_colors=nullptr;
    QLabel* m_summary=nullptr;
};
}
