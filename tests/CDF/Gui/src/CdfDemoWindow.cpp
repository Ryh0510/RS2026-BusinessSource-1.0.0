#include "CdfDemoWindow.h"

#include "CdfDemoController.h"

#include "RobotViewport.h"

#include <SimulationProject/ProjectDocument.h>

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace cdf_gui
{
    namespace
    {
        constexpr const char* kRobotId = "cdf_three_dof_robot_3r";
        constexpr const char* kObstacleObjectId = "cdf_obstacle_scene";
        constexpr const char* kDetectorId = "cdf_robot_obstacle_collision";
        constexpr double kSliderScale = 1000.0;
        constexpr double kJointLower[3] = { -3.141592653589793, -2.617994, -2.617994 };
        constexpr double kJointUpper[3] = { 3.141592653589793, 2.617994, 2.617994 };
        constexpr const char* kJointNames[3] = { "joint1", "joint2", "joint3" };

        int toSliderValue(double radians)
        {
            return static_cast<int>(std::lround(radians * kSliderScale));
        }

        double fromSliderValue(int value)
        {
            return static_cast<double>(value) / kSliderScale;
        }

        QString formatJoints(const QVector<double>& values)
        {
            return QStringLiteral("[%1, %2, %3]")
                .arg(values.size() > 0 ? values[0] : 0.0, 0, 'f', 3)
                .arg(values.size() > 1 ? values[1] : 0.0, 0, 'f', 3)
                .arg(values.size() > 2 ? values[2] : 0.0, 0, 'f', 3);
        }

        void appendInitialJoint(
            simulation_project::RobotDesc& robot,
            const char* jointName,
            const QVector<double>& values,
            int index)
        {
            simulation_project::JointValueDesc joint;
            joint.jointName = jointName;
            joint.value = index < values.size() ? values[index] : 0.0;
            robot.initialJoints.push_back(joint);
        }

        simulation_project::CollisionDetectorTargetDesc robotTarget()
        {
            simulation_project::CollisionDetectorTargetDesc target;
            target.robotId = kRobotId;
            return target;
        }

        simulation_project::CollisionDetectorTargetDesc objectTarget()
        {
            simulation_project::CollisionDetectorTargetDesc target;
            target.objectId = kObstacleObjectId;
            return target;
        }

        simulation_project::CollisionPairGeneratorDesc robotObjectGenerator()
        {
            simulation_project::CollisionPairGeneratorDesc generator;
            generator.type = "RobotObject";
            generator.robotId = kRobotId;
            generator.objectId = kObstacleObjectId;
            return generator;
        }
    }

    CdfDemoWindow::CdfDemoWindow(QWidget* parent)
        : QMainWindow(parent)
        , m_controller(new CdfDemoController(this))
        , m_viewport(new RobotViewport(this))
        , m_title(new QLabel(this))
        , m_summary(new QLabel(this))
        , m_clearance(new QLabel(this))
        , m_gradient(new QLabel(this))
        , m_seed(new QLabel(this))
        , m_repair(new QLabel(this))
        , m_status(new QLabel(this))
        , m_currentConfig(new QLabel(this))
        , m_startConfig(new QLabel(this))
        , m_goalConfig(new QLabel(this))
        , m_targetClearance(new QDoubleSpinBox(this))
        , m_animationTimer(new QTimer(this))
    {
        setWindowTitle(QStringLiteral("CDF 3R Robot Collision-Free Planning Demos"));
        resize(1180, 720);

        QWidget* central = new QWidget(this);
        QHBoxLayout* rootLayout = new QHBoxLayout(central);
        rootLayout->setContentsMargins(12, 12, 12, 12);
        rootLayout->setSpacing(12);

        QFrame* sidePanel = new QFrame(central);
        sidePanel->setFrameShape(QFrame::StyledPanel);
        sidePanel->setMinimumWidth(330);
        QVBoxLayout* sideLayout = new QVBoxLayout(sidePanel);
        sideLayout->setContentsMargins(14, 14, 14, 14);
        sideLayout->setSpacing(10);

        m_title->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600;"));
        m_summary->setWordWrap(true);
        m_clearance->setWordWrap(true);
        m_gradient->setWordWrap(true);
        m_seed->setWordWrap(true);
        m_repair->setWordWrap(true);
        m_status->setWordWrap(true);
        m_currentConfig->setWordWrap(true);
        m_startConfig->setWordWrap(true);
        m_goalConfig->setWordWrap(true);

        QHBoxLayout* caseButtons = new QHBoxLayout();
        QPushButton* distanceButton = makeToolButton(
            QStringLiteral("Field"),
            QStyle::SP_FileDialogDetailedView,
            QStringLiteral("Show the online CDF distance and finite-difference gradient case."));
        QPushButton* seedButton = makeToolButton(
            QStringLiteral("Seed"),
            QStyle::SP_MediaPlay,
            QStringLiteral("Show the sampled-tree seed path case used as the OMPL stand-in."));
        QPushButton* repairButton = makeToolButton(
            QStringLiteral("Repair"),
            QStyle::SP_BrowserReload,
            QStringLiteral("Show CDF path repair from a colliding seed."));
        caseButtons->addWidget(distanceButton);
        caseButtons->addWidget(seedButton);
        caseButtons->addWidget(repairButton);

        QGridLayout* jointLayout = new QGridLayout();
        jointLayout->setColumnStretch(1, 1);
        for (int i = 0; i < 3; ++i)
        {
            QLabel* jointLabel = new QLabel(QString::fromLatin1(kJointNames[i]), this);
            QSlider* slider = new QSlider(Qt::Horizontal, this);
            slider->setRange(toSliderValue(kJointLower[i]), toSliderValue(kJointUpper[i]));
            slider->setSingleStep(5);
            slider->setPageStep(50);

            QDoubleSpinBox* spinBox = new QDoubleSpinBox(this);
            spinBox->setRange(kJointLower[i], kJointUpper[i]);
            spinBox->setSingleStep(0.01);
            spinBox->setDecimals(3);
            spinBox->setSuffix(QStringLiteral(" rad"));

            m_jointSliders[static_cast<std::size_t>(i)] = slider;
            m_jointSpinBoxes[static_cast<std::size_t>(i)] = spinBox;

            jointLayout->addWidget(jointLabel, i, 0);
            jointLayout->addWidget(slider, i, 1);
            jointLayout->addWidget(spinBox, i, 2);

            connect(slider, &QSlider::valueChanged, this, [this, i](int value) {
                setCurrentJointValue(i, fromSliderValue(value));
            });
            connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, i](double value) {
                setCurrentJointValue(i, value);
            });
        }

        QHBoxLayout* endpointButtons = new QHBoxLayout();
        QPushButton* saveStartButton = makeToolButton(
            QStringLiteral("Save Start"),
            QStyle::SP_DialogSaveButton,
            QStringLiteral("Save the current joint configuration as the start."));
        QPushButton* saveGoalButton = makeToolButton(
            QStringLiteral("Save Goal"),
            QStyle::SP_DialogSaveButton,
            QStringLiteral("Save the current joint configuration as the goal."));
        endpointButtons->addWidget(saveStartButton);
        endpointButtons->addWidget(saveGoalButton);

        QPushButton* planButton = makeToolButton(
            QStringLiteral("Plan Trajectory"),
            QStyle::SP_DialogApplyButton,
            QStringLiteral("Run CDF collision-free planning from the saved start to the saved goal."));

        m_targetClearance->setRange(0.0, 0.20);
        m_targetClearance->setSingleStep(0.005);
        m_targetClearance->setDecimals(3);

        QFormLayout* form = new QFormLayout();
        form->addRow(QStringLiteral("Target clearance"), m_targetClearance);
        form->addRow(QStringLiteral("Path"), m_summary);
        form->addRow(QStringLiteral("Clearance"), m_clearance);
        form->addRow(QStringLiteral("Gradient"), m_gradient);
        form->addRow(QStringLiteral("Seed"), m_seed);
        form->addRow(QStringLiteral("Repair"), m_repair);
        form->addRow(QStringLiteral("Current q"), m_currentConfig);
        form->addRow(QStringLiteral("Start q"), m_startConfig);
        form->addRow(QStringLiteral("Goal q"), m_goalConfig);

        sideLayout->addWidget(m_title);
        sideLayout->addLayout(caseButtons);
        sideLayout->addSpacing(4);
        sideLayout->addLayout(jointLayout);
        sideLayout->addLayout(endpointButtons);
        sideLayout->addWidget(planButton);
        sideLayout->addLayout(form);
        sideLayout->addStretch();
        sideLayout->addWidget(m_status);

        rootLayout->addWidget(sidePanel);
        rootLayout->addWidget(m_viewport, 1);
        setCentralWidget(central);

        m_animationTimer->setInterval(55);
        connect(m_animationTimer, &QTimer::timeout, this, &CdfDemoWindow::advanceAnimation);
        connect(distanceButton, &QPushButton::clicked, m_controller, &CdfDemoController::showDistanceFieldCase);
        connect(seedButton, &QPushButton::clicked, m_controller, &CdfDemoController::showOmplSeedCase);
        connect(repairButton, &QPushButton::clicked, m_controller, &CdfDemoController::showRepairCase);
        connect(saveStartButton, &QPushButton::clicked, this, &CdfDemoWindow::saveCurrentAsStart);
        connect(saveGoalButton, &QPushButton::clicked, this, &CdfDemoWindow::saveCurrentAsGoal);
        connect(planButton, &QPushButton::clicked, this, &CdfDemoWindow::planFromSavedEndpoints);
        connect(
            m_targetClearance,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            m_controller,
            &CdfDemoController::setTargetClearance);
        connect(m_controller, &CdfDemoController::viewModelChanged, this, &CdfDemoWindow::applyViewModel);
        connect(
            m_controller,
            &CdfDemoController::statusMessageRequested,
            statusBar(),
            &QStatusBar::showMessage);

        applyViewModel(m_controller->currentViewModel());
        m_animationTimer->start();
    }

    void CdfDemoWindow::applyViewModel(const CdfDemoViewModel& model)
    {
        m_model = model;
        m_animationFrame = 0;
        m_manualPoseActive = false;
        m_savedStartJoints = model.startJoints;
        m_savedGoalJoints = model.goalJoints;
        syncJointEditors(model.robotJointPath.isEmpty() ? model.startJoints : model.robotJointPath.front());
        updateEndpointLabels();

        m_title->setText(model.title);
        m_summary->setText(model.summary);
        m_clearance->setText(model.clearanceText);
        m_gradient->setText(model.gradientText);
        m_seed->setText(model.seedText);
        m_repair->setText(model.repairText);
        m_status->setText(model.statusText);
        m_status->setStyleSheet(model.collisionFree
            ? QStringLiteral("color: rgb(31, 132, 106);")
            : QStringLiteral("color: rgb(178, 68, 61);"));

        const QSignalBlocker blocker(m_targetClearance);
        m_targetClearance->setValue(model.targetClearance);

        const simulation_project::ProjectDocument document = makeRobotProjectDocument(model);
        const bool loaded = m_viewport->loadProjectDocument(document, std::filesystem::path(CDF_PROJECT_SOURCE_PATH));
        if (!loaded)
            statusBar()->showMessage(m_viewport->lastError(), 6000);
        applyRobotFrame(0);
        m_viewport->setCollisionGeometryVisible(true);
        m_viewport->resetCamera();
    }

    void CdfDemoWindow::advanceAnimation()
    {
        if (m_manualPoseActive || m_model.robotJointPath.isEmpty())
            return;
        m_animationFrame = (m_animationFrame + 1) % m_model.robotJointPath.size();
        applyRobotFrame(m_animationFrame);
    }

    simulation_project::ProjectDocument CdfDemoWindow::makeRobotProjectDocument(const CdfDemoViewModel& model) const
    {
        simulation_project::ProjectDocument document;
        document.view.camera.target = simulation_project::Vec3Desc{ 0.20, 0.0, 0.08 };
        document.view.camera.distance = 1.25;
        document.view.camera.yaw = 45.0;
        document.view.camera.pitch = -35.0;
        document.view.showGrid = true;
        document.view.showAxis = true;

        const QVector<double> initialJoints =
            model.robotJointPath.isEmpty() ? model.startJoints : model.robotJointPath.front();

        simulation_project::RobotDesc robot;
        robot.id = kRobotId;
        robot.name = "three_dof_robot_3r";
        robot.sourceType = "urdf";
        robot.sourcePath = CDF_3R_URDF_PATH;
        robot.visible = true;
        robot.collisionEnabled = true;
        appendInitialJoint(robot, "joint1", initialJoints, 0);
        appendInitialJoint(robot, "joint2", initialJoints, 1);
        appendInitialJoint(robot, "joint3", initialJoints, 2);
        document.robots.push_back(robot);

        simulation_project::SceneObjectDesc obstacles;
        obstacles.id = kObstacleObjectId;
        obstacles.name = "ThreeDOF_Robot_ObstacleScene";
        obstacles.objectType = "fixture";
        obstacles.sourcePath = CDF_OBSTACLE_SCENE_MESH_PATH;
        obstacles.visible = true;
        obstacles.collisionEnabled = true;
        obstacles.transform.x = -0.05;
        obstacles.transform.z = 0.06;
        obstacles.transform.roll = 1.5707963267948966;
        obstacles.visualScale = 1.0;
        obstacles.collisionScale = 1.0;
        document.objects.push_back(obstacles);

        simulation_project::CollisionDetectorDesc detector;
        detector.id = kDetectorId;
        detector.name = "CDF robot vs obstacle scene";
        detector.type = "RobotObject";
        detector.enabled = true;
        detector.contacts = true;
        detector.nearestPoints = true;
        detector.distance = true;
        detector.maxContacts = 32;
        detector.targets.push_back(robotTarget());
        detector.targets.push_back(objectTarget());
        detector.pairGenerators.push_back(robotObjectGenerator());
        document.collision.detectors.push_back(detector);

        return document;
    }

    void CdfDemoWindow::applyRobotFrame(int frameIndex)
    {
        if (m_model.robotJointPath.isEmpty())
            return;
        const int index = std::max(0, std::min(frameIndex, m_model.robotJointPath.size() - 1));
        const QVector<double>& q = m_model.robotJointPath[index];
        if (q.size() > 0)
            m_viewport->setRobotJointValue(kRobotId, QStringLiteral("joint1"), q[0]);
        if (q.size() > 1)
            m_viewport->setRobotJointValue(kRobotId, QStringLiteral("joint2"), q[1]);
        if (q.size() > 2)
            m_viewport->setRobotJointValue(kRobotId, QStringLiteral("joint3"), q[2]);
        syncJointEditors(q);
    }

    void CdfDemoWindow::setCurrentJointValue(int jointIndex, double value)
    {
        if (jointIndex < 0 || jointIndex >= 3)
            return;
        if (m_currentJoints.size() < 3)
            m_currentJoints = QVector<double>{ 0.0, 0.0, 0.0 };

        const double clamped = std::max(kJointLower[jointIndex], std::min(value, kJointUpper[jointIndex]));
        m_currentJoints[jointIndex] = clamped;

        {
            const QSignalBlocker sliderBlocker(m_jointSliders[static_cast<std::size_t>(jointIndex)]);
            const QSignalBlocker spinBlocker(m_jointSpinBoxes[static_cast<std::size_t>(jointIndex)]);
            m_jointSliders[static_cast<std::size_t>(jointIndex)]->setValue(toSliderValue(clamped));
            m_jointSpinBoxes[static_cast<std::size_t>(jointIndex)]->setValue(clamped);
        }

        m_manualPoseActive = true;
        applyCurrentJointsToViewport();
        updateEndpointLabels();
    }

    void CdfDemoWindow::syncJointEditors(const QVector<double>& joints)
    {
        if (joints.size() >= 3)
            m_currentJoints = QVector<double>{ joints[0], joints[1], joints[2] };
        else
            m_currentJoints = QVector<double>{ 0.0, 0.0, 0.0 };

        for (int i = 0; i < 3; ++i)
        {
            const double value = std::max(kJointLower[i], std::min(m_currentJoints[i], kJointUpper[i]));
            m_currentJoints[i] = value;
            const QSignalBlocker sliderBlocker(m_jointSliders[static_cast<std::size_t>(i)]);
            const QSignalBlocker spinBlocker(m_jointSpinBoxes[static_cast<std::size_t>(i)]);
            m_jointSliders[static_cast<std::size_t>(i)]->setValue(toSliderValue(value));
            m_jointSpinBoxes[static_cast<std::size_t>(i)]->setValue(value);
        }

        updateEndpointLabels();
    }

    void CdfDemoWindow::applyCurrentJointsToViewport()
    {
        if (m_currentJoints.size() < 3)
            return;
        m_viewport->setRobotJointValue(kRobotId, QStringLiteral("joint1"), m_currentJoints[0]);
        m_viewport->setRobotJointValue(kRobotId, QStringLiteral("joint2"), m_currentJoints[1]);
        m_viewport->setRobotJointValue(kRobotId, QStringLiteral("joint3"), m_currentJoints[2]);
    }

    void CdfDemoWindow::saveCurrentAsStart()
    {
        if (m_currentJoints.size() < 3)
            return;
        m_savedStartJoints = m_currentJoints;
        updateEndpointLabels();
        statusBar()->showMessage(QStringLiteral("Saved current joint configuration as start."), 2500);
    }

    void CdfDemoWindow::saveCurrentAsGoal()
    {
        if (m_currentJoints.size() < 3)
            return;
        m_savedGoalJoints = m_currentJoints;
        updateEndpointLabels();
        statusBar()->showMessage(QStringLiteral("Saved current joint configuration as goal."), 2500);
    }

    void CdfDemoWindow::planFromSavedEndpoints()
    {
        if (m_savedStartJoints.size() < 3 || m_savedGoalJoints.size() < 3)
            return;
        m_manualPoseActive = false;
        m_controller->planFromEndpoints(m_savedStartJoints, m_savedGoalJoints);
    }

    void CdfDemoWindow::updateEndpointLabels()
    {
        m_currentConfig->setText(formatJoints(m_currentJoints));
        m_startConfig->setText(formatJoints(m_savedStartJoints));
        m_goalConfig->setText(formatJoints(m_savedGoalJoints));
    }

    QPushButton* CdfDemoWindow::makeToolButton(
        const QString& text,
        QStyle::StandardPixmap icon,
        const QString& tooltip)
    {
        QPushButton* button = new QPushButton(style()->standardIcon(icon), text, this);
        button->setToolTip(tooltip);
        button->setMinimumHeight(32);
        return button;
    }
}
