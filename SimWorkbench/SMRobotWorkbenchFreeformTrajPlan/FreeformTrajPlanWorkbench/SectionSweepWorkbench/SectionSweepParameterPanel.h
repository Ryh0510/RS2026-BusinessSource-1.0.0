#pragma once

#include "SlicingWorkbench/FreeformTrajPlanWorkbenchExport.h"
#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;

namespace smrobot::workbench::freeform
{
    class FREEFORM_TRAJ_PLAN_WORKBENCH_API SectionSweepParameterPanel final : public QWidget
    {
        Q_OBJECT
    public:
        explicit SectionSweepParameterPanel(QWidget* parent=nullptr);
        section_sweep::Parameters parameters() const;
    signals:
        void parametersEdited();
        void generateRequested();
    private:
        QComboBox* m_scanAxis=nullptr;
        QComboBox* m_sideAxis=nullptr;
        QDoubleSpinBox* m_passSpacing=nullptr;
        QDoubleSpinBox* m_sampleSpacing=nullptr;
        QDoubleSpinBox* m_sprayDistance=nullptr;
        QDoubleSpinBox* m_spraySpeed=nullptr;
        QDoubleSpinBox* m_transferSpeed=nullptr;
        QDoubleSpinBox* m_extension=nullptr;
        QDoubleSpinBox* m_retract=nullptr;
        QDoubleSpinBox* m_maximumTilt=nullptr;
        QCheckBox* m_alternate=nullptr;
    };
}
