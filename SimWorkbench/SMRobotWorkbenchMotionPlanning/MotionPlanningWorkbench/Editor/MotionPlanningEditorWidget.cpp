#include "MotionPlanningEditorWidget.h"

#include "RobotQtWidgetUtils.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
    void configureTrajectoryTable(
        QTableWidget* table,
        const QStringList& headers,
        int minimumHeight)
    {
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        for(int column = 2; column < headers.size(); ++column) {
            table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Stretch);
        }
        table->setColumnWidth(0, 42);
        table->setColumnWidth(1, 56);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setMinimumHeight(minimumHeight);
    }

    QTableWidgetItem* makeReadOnlyItem(const QString& text)
    {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    }

    QDoubleSpinBox* makePoseSpinBox(
        QWidget* parent,
        double minimum,
        double maximum,
        double value,
        double step,
        const QString& suffix = QString())
    {
        auto* spinBox = new QDoubleSpinBox(parent);
        spinBox->setRange(minimum, maximum);
        spinBox->setDecimals(6);
        spinBox->setSingleStep(step);
        spinBox->setValue(value);
        spinBox->setSuffix(suffix);
        return spinBox;
    }

    void populateTrajectoryTable(
        QTableWidget* table,
        const QVector<MotionPlanningEditorWidget::TrajectoryPointRow>& points,
        const QString& emptyText)
    {
        if(table == nullptr) {
            return;
        }

        table->clearSpans();
        table->setRowCount(points.size());
        const int columnCount = table->columnCount();
        for(int row = 0; row < points.size(); ++row) {
            const MotionPlanningEditorWidget::TrajectoryPointRow& point = points[row];
            table->setItem(row, 0, makeReadOnlyItem(QString::number(point.index)));
            table->setItem(row, 1, makeReadOnlyItem(point.timeText));
            table->setItem(row, 2, makeReadOnlyItem(point.valueText));
            if(columnCount > 3) {
                table->setItem(row, 3, makeReadOnlyItem(point.orientationText));
            }
        }

        if(points.empty()) {
            table->setRowCount(1);
            table->setSpan(0, 0, 1, columnCount);
            table->setItem(0, 0, makeReadOnlyItem(emptyText));
        }
        table->resizeColumnToContents(0);
        table->resizeColumnToContents(1);
        if(columnCount > 3) {
            table->resizeColumnToContents(3);
        }
    }
}

MotionPlanningEditorWidget::MotionPlanningEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Motion Planning"), this);
    title->setProperty("panelTitle", true);
    layout->addWidget(title);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_robotValue = new QLabel(QStringLiteral("No robot selected"), this);
    robot_qt_viewer::makeHorizontallyCompressible(m_robotValue);
    form->addRow(QStringLiteral("Robot"), m_robotValue);

    m_startJoints = new QLineEdit(this);
    m_startJoints->setPlaceholderText(QStringLiteral("0, 0, 0"));
    form->addRow(QStringLiteral("Start joints"), m_startJoints);

    m_jointNames = new QLineEdit(this);
    m_jointNames->setPlaceholderText(QStringLiteral("joint_1, joint_2, joint_3"));
    form->addRow(QStringLiteral("Joint names"), m_jointNames);

    m_goalJoints = new QLineEdit(this);
    m_goalJoints->setPlaceholderText(QStringLiteral("0.5, -0.2, 0.8"));
    form->addRow(QStringLiteral("Goal joints"), m_goalJoints);

    m_duration = new QDoubleSpinBox(this);
    m_duration->setRange(0.01, 3600.0);
    m_duration->setValue(5.0);
    m_duration->setSuffix(QStringLiteral(" s"));
    form->addRow(QStringLiteral("Duration"), m_duration);

    m_sampleCount = new QSpinBox(this);
    m_sampleCount->setRange(2, 10000);
    m_sampleCount->setValue(50);
    form->addRow(QStringLiteral("Samples"), m_sampleCount);

    layout->addLayout(form);

    m_planButton = new QPushButton(QStringLiteral("Plan trajectory"), this);
    layout->addWidget(m_planButton);

    auto* controlPointTitle = new QLabel(QStringLiteral("Trajectory Control Points"), this);
    controlPointTitle->setProperty("panelTitle", true);
    layout->addWidget(controlPointTitle);

    m_importButton = new QPushButton(QStringLiteral("Import trajectory..."), this);
    layout->addWidget(m_importButton);

    auto* ikForm = new QFormLayout();
    ikForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_ikToolMode = new QComboBox(this);
    m_ikToolMode->addItem(QStringLiteral("With tool TCP"), true);
    m_ikToolMode->addItem(QStringLiteral("Robot flange"), false);
    ikForm->addRow(QStringLiteral("IK target"), m_ikToolMode);
    layout->addLayout(ikForm);

    m_solveIkButton = new QPushButton(QStringLiteral("Solve IK and apply"), this);
    layout->addWidget(m_solveIkButton);

    m_trajectoryCombo = new QComboBox(this);
    robot_qt_viewer::makeHorizontallyCompressible(m_trajectoryCombo);
    layout->addWidget(m_trajectoryCombo);

    auto* poseTitle = new QLabel(QStringLiteral("Control Point Poses"), this);
    poseTitle->setProperty("panelTitle", true);
    layout->addWidget(poseTitle);

    m_poseTable = new QTableWidget(this);
    configureTrajectoryTable(m_poseTable, {
        QStringLiteral("#"),
        QStringLiteral("t"),
        QStringLiteral("Position"),
        QStringLiteral("Euler (deg)")
    }, 145);
    m_poseTable->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_poseTable);

    auto* jointTitle = new QLabel(QStringLiteral("IK Joint Values"), this);
    jointTitle->setProperty("panelTitle", true);
    layout->addWidget(jointTitle);

    m_jointTable = new QTableWidget(this);
    configureTrajectoryTable(m_jointTable, {
        QStringLiteral("#"),
        QStringLiteral("t"),
        QStringLiteral("Joint values")
    }, 145);
    layout->addWidget(m_jointTable);

    m_applyJointPointButton = new QPushButton(QStringLiteral("Apply selected joint point"), this);
    layout->addWidget(m_applyJointPointButton);

    auto* playbackLayout = new QHBoxLayout();
    playbackLayout->setSpacing(6);
    m_playbackDuration = new QDoubleSpinBox(this);
    m_playbackDuration->setRange(0.1, 3600.0);
    m_playbackDuration->setValue(5.0);
    m_playbackDuration->setSuffix(QStringLiteral(" s"));
    m_playbackButton = new QPushButton(QStringLiteral("Play IK result"), this);
    playbackLayout->addWidget(m_playbackDuration);
    playbackLayout->addWidget(m_playbackButton);
    layout->addLayout(playbackLayout);

    m_result = new QLabel(QStringLiteral("Select a robot and enter joint vectors."), this);
    m_result->setWordWrap(true);
    layout->addWidget(m_result);
    layout->addStretch(1);

    connect(m_planButton, &QPushButton::clicked, this, [this]() {
        emit planRequested(
            m_startJoints->text(),
            m_goalJoints->text(),
            m_jointNames->text(),
            m_duration->value(),
            m_sampleCount->value());
    });
    connect(m_importButton, &QPushButton::clicked,
        this, &MotionPlanningEditorWidget::importTrajectoryRequested);
    connect(m_solveIkButton, &QPushButton::clicked, this, [this]() {
        const bool useToolTransform = m_ikToolMode != nullptr &&
            m_ikToolMode->currentData().toBool();
        emit inverseKinematicsRequested(useToolTransform);
    });
    connect(m_applyJointPointButton, &QPushButton::clicked, this, [this]() {
        emit applySelectedJointPointRequested(m_jointTable != nullptr ? m_jointTable->currentRow() : -1);
    });
    connect(m_playbackButton, &QPushButton::clicked, this, [this]() {
        if(m_playbackActive) {
            emit playbackStopRequested();
        } else {
            emit playbackRequested(m_playbackDuration != nullptr ? m_playbackDuration->value() : 5.0);
        }
    });
    connect(m_poseTable, &QTableWidget::customContextMenuRequested,
        this, &MotionPlanningEditorWidget::showControlPointContextMenu);
    connect(m_trajectoryCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this]() {
            if(m_trajectoryCombo != nullptr) {
                emit trajectorySelectionChanged(m_trajectoryCombo->currentData().toString());
            }
            updateTrajectoryActions();
        });
    connect(m_jointTable, &QTableWidget::currentCellChanged, this, [this]() {
        updateTrajectoryActions();
    });
    updateTrajectoryActions();
}

bool MotionPlanningEditorWidget::editControlPointPose(
    ControlPointPoseEditorData& data,
    const QString& title)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);

    auto* layout = new QFormLayout(&dialog);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    auto* time = makePoseSpinBox(&dialog, 0.0, 3600.0, data.time, 0.1, QStringLiteral(" s"));
    auto* x = makePoseSpinBox(&dialog, -10000.0, 10000.0, data.x, 0.001, QStringLiteral(" m"));
    auto* y = makePoseSpinBox(&dialog, -10000.0, 10000.0, data.y, 0.001, QStringLiteral(" m"));
    auto* z = makePoseSpinBox(&dialog, -10000.0, 10000.0, data.z, 0.001, QStringLiteral(" m"));
    auto* roll = makePoseSpinBox(&dialog, -360.0, 360.0, data.rollDeg, 1.0, QStringLiteral(" deg"));
    auto* pitch = makePoseSpinBox(&dialog, -360.0, 360.0, data.pitchDeg, 1.0, QStringLiteral(" deg"));
    auto* yaw = makePoseSpinBox(&dialog, -360.0, 360.0, data.yawDeg, 1.0, QStringLiteral(" deg"));

    layout->addRow(QStringLiteral("Time"), time);
    layout->addRow(QStringLiteral("X"), x);
    layout->addRow(QStringLiteral("Y"), y);
    layout->addRow(QStringLiteral("Z"), z);
    layout->addRow(QStringLiteral("Roll"), roll);
    layout->addRow(QStringLiteral("Pitch"), pitch);
    layout->addRow(QStringLiteral("Yaw"), yaw);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    layout->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if(dialog.exec() != QDialog::Accepted) {
        return false;
    }

    data.time = time->value();
    data.x = x->value();
    data.y = y->value();
    data.z = z->value();
    data.rollDeg = roll->value();
    data.pitchDeg = pitch->value();
    data.yawDeg = yaw->value();
    return true;
}

void MotionPlanningEditorWidget::setPlaybackActive(bool active)
{
    m_playbackActive = active;
    if(m_playbackButton != nullptr) {
        m_playbackButton->setText(active
            ? QStringLiteral("Stop playback")
            : QStringLiteral("Play IK result"));
    }
    updateTrajectoryActions();
}

void MotionPlanningEditorWidget::setJointDefaults(
    const QString& jointNames,
    const QString& startJoints)
{
    if(m_jointNames->text().trimmed().isEmpty()) {
        m_jointNames->setText(jointNames);
    }
    if(m_startJoints->text().trimmed().isEmpty()) {
        m_startJoints->setText(startJoints);
    }
}

void MotionPlanningEditorWidget::setRobotId(const QString& robotId)
{
    m_robotValue->setText(robotId.isEmpty() ? QStringLiteral("No robot selected") : robotId);
    m_planButton->setEnabled(!robotId.isEmpty());
    if(m_importButton != nullptr) {
        m_importButton->setEnabled(!robotId.isEmpty());
    }
    updateTrajectoryActions();
}

void MotionPlanningEditorWidget::setResult(const QString& summary, bool success)
{
    m_result->setText(summary);
    m_result->setProperty("error", !success);
    m_result->style()->unpolish(m_result);
    m_result->style()->polish(m_result);
}

void MotionPlanningEditorWidget::setTrajectoryView(
    const QVector<TrajectoryListItem>& trajectories,
    const QString& selectedTrajectoryId,
    const QVector<TrajectoryPointRow>& posePoints,
    const QVector<TrajectoryPointRow>& jointPoints,
    const QString& emptyPoseText,
    const QString& emptyJointText)
{
    if(m_trajectoryCombo == nullptr || m_poseTable == nullptr || m_jointTable == nullptr) {
        return;
    }

    QSignalBlocker comboBlocker(m_trajectoryCombo);
    m_trajectoryCombo->clear();
    int selectedIndex = -1;
    for(const TrajectoryListItem& trajectory : trajectories) {
        QString label = trajectory.label.isEmpty() ? trajectory.id : trajectory.label;
        if(!trajectory.kind.isEmpty()) {
            label = QStringLiteral("%1 (%2, %3 pts)")
                .arg(label)
                .arg(trajectory.kind)
                .arg(trajectory.pointCount);
        }
        m_trajectoryCombo->addItem(label, trajectory.id);
        m_trajectoryCombo->setItemData(
            m_trajectoryCombo->count() - 1,
            trajectory.kind,
            Qt::UserRole + 1);
        if(trajectory.id == selectedTrajectoryId) {
            selectedIndex = m_trajectoryCombo->count() - 1;
        }
    }
    if(selectedIndex < 0 && m_trajectoryCombo->count() > 0) {
        selectedIndex = 0;
    }
    if(selectedIndex >= 0) {
        m_trajectoryCombo->setCurrentIndex(selectedIndex);
    }
    m_trajectoryCombo->setEnabled(m_trajectoryCombo->count() > 0);

    populateTrajectoryTable(m_poseTable, posePoints, emptyPoseText);
    populateTrajectoryTable(m_jointTable, jointPoints, emptyJointText);
    updateTrajectoryActions();
}

void MotionPlanningEditorWidget::updateTrajectoryActions()
{
    const bool hasRobot = m_robotValue != nullptr &&
        m_robotValue->text() != QStringLiteral("No robot selected");
    QString kind;
    if(m_trajectoryCombo != nullptr && m_trajectoryCombo->currentIndex() >= 0) {
        kind = m_trajectoryCombo->currentData(Qt::UserRole + 1).toString();
    }
    const bool hasCartesian = kind.contains(QStringLiteral("cartesian"));
    const bool hasJoint = kind.contains(QStringLiteral("joint"));
    const bool hasValidJointRow = m_jointTable != nullptr &&
        m_jointTable->currentRow() >= 0 &&
        m_jointTable->rowCount() > 0 &&
        !(m_jointTable->rowCount() == 1 && m_jointTable->columnSpan(0, 0) > 1);
    const bool hasAnyJointRow = m_jointTable != nullptr &&
        m_jointTable->rowCount() > 0 &&
        !(m_jointTable->rowCount() == 1 && m_jointTable->columnSpan(0, 0) > 1);

    if(m_ikToolMode != nullptr) {
        m_ikToolMode->setEnabled(hasRobot && hasCartesian);
    }
    if(m_solveIkButton != nullptr) {
        m_solveIkButton->setEnabled(hasRobot && hasCartesian);
    }
    if(m_applyJointPointButton != nullptr) {
        m_applyJointPointButton->setEnabled(hasRobot && hasJoint && hasValidJointRow);
    }
    if(m_playbackDuration != nullptr) {
        m_playbackDuration->setEnabled(!m_playbackActive && hasRobot && hasJoint && hasAnyJointRow);
    }
    if(m_playbackButton != nullptr) {
        m_playbackButton->setEnabled(m_playbackActive || (hasRobot && hasJoint && hasAnyJointRow));
    }
}

void MotionPlanningEditorWidget::showControlPointContextMenu(const QPoint& pos)
{
    if(m_poseTable == nullptr) {
        return;
    }

    QTableWidgetItem* item = m_poseTable->itemAt(pos);
    if(item == nullptr || m_poseTable->columnSpan(item->row(), 0) > 1) {
        return;
    }

    const int row = item->row();
    m_poseTable->setCurrentCell(row, item->column());

    QMenu menu(this);
    QAction* insertBefore = menu.addAction(QStringLiteral("Insert Before"));
    QAction* insertAfter = menu.addAction(QStringLiteral("Insert After"));
    menu.addSeparator();
    QAction* edit = menu.addAction(QStringLiteral("Edit"));
    QAction* remove = menu.addAction(QStringLiteral("Delete"));

    QAction* selectedAction = menu.exec(m_poseTable->viewport()->mapToGlobal(pos));
    if(selectedAction == insertBefore) {
        emit insertControlPointBeforeRequested(row);
    } else if(selectedAction == insertAfter) {
        emit insertControlPointAfterRequested(row);
    } else if(selectedAction == edit) {
        emit editControlPointRequested(row);
    } else if(selectedAction == remove) {
        emit deleteControlPointRequested(row);
    }
}
