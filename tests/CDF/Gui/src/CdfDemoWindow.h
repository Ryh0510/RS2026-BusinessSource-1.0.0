#pragma once

#include "CdfDemoViewModel.h"

#include <QMainWindow>
#include <QStyle>
#include <QVector>

#include <array>

class QLabel;
class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QTimer;
class RobotViewport;

namespace simulation_project
{
    struct ProjectDocument;
}

namespace cdf_gui
{
    class CdfDemoController;

    class CdfDemoWindow final : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit CdfDemoWindow(QWidget* parent = nullptr);

    private slots:
        void applyViewModel(const cdf_gui::CdfDemoViewModel& model);
        void advanceAnimation();

    private:
        QPushButton* makeToolButton(const QString& text, QStyle::StandardPixmap icon, const QString& tooltip);
        simulation_project::ProjectDocument makeRobotProjectDocument(const cdf_gui::CdfDemoViewModel& model) const;
        void applyRobotFrame(int frameIndex);
        void setCurrentJointValue(int jointIndex, double value);
        void syncJointEditors(const QVector<double>& joints);
        void applyCurrentJointsToViewport();
        void saveCurrentAsStart();
        void saveCurrentAsGoal();
        void planFromSavedEndpoints();
        void updateEndpointLabels();

        CdfDemoController* m_controller = nullptr;
        RobotViewport* m_viewport = nullptr;
        QLabel* m_title = nullptr;
        QLabel* m_summary = nullptr;
        QLabel* m_clearance = nullptr;
        QLabel* m_gradient = nullptr;
        QLabel* m_seed = nullptr;
        QLabel* m_repair = nullptr;
        QLabel* m_status = nullptr;
        QLabel* m_currentConfig = nullptr;
        QLabel* m_startConfig = nullptr;
        QLabel* m_goalConfig = nullptr;
        QDoubleSpinBox* m_targetClearance = nullptr;
        std::array<QSlider*, 3> m_jointSliders{};
        std::array<QDoubleSpinBox*, 3> m_jointSpinBoxes{};
        QTimer* m_animationTimer = nullptr;
        CdfDemoViewModel m_model;
        QVector<double> m_currentJoints;
        QVector<double> m_savedStartJoints;
        QVector<double> m_savedGoalJoints;
        int m_animationFrame = 0;
        bool m_manualPoseActive = false;
    };
}
