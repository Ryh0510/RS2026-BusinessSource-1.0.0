#include "MotionPlanningEditorWidget.h"

#include "RobotQtWidgetUtils.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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

    m_trajectoryCombo = new QComboBox(this);
    robot_qt_viewer::makeHorizontallyCompressible(m_trajectoryCombo);
    layout->addWidget(m_trajectoryCombo);

    m_pointsTable = new QTableWidget(this);
    m_pointsTable->setColumnCount(4);
    m_pointsTable->setHorizontalHeaderLabels({
        QStringLiteral("#"),
        QStringLiteral("t"),
        QStringLiteral("Position / joints"),
        QStringLiteral("Euler (deg)")
    });
    m_pointsTable->verticalHeader()->setVisible(false);
    m_pointsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_pointsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_pointsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_pointsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_pointsTable->setColumnWidth(0, 42);
    m_pointsTable->setColumnWidth(1, 56);
    m_pointsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pointsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pointsTable->setMinimumHeight(180);
    layout->addWidget(m_pointsTable);

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
    connect(m_trajectoryCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this]() {
            if(m_trajectoryCombo != nullptr) {
                emit trajectorySelectionChanged(m_trajectoryCombo->currentData().toString());
            }
        });
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
    const QVector<TrajectoryPointRow>& points,
    const QString& emptyText)
{
    if(m_trajectoryCombo == nullptr || m_pointsTable == nullptr) {
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

    m_pointsTable->clearSpans();
    m_pointsTable->setRowCount(points.size());
    auto makeItem = [](const QString& text) {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    for(int row = 0; row < points.size(); ++row) {
        const TrajectoryPointRow& point = points[row];
        m_pointsTable->setItem(row, 0, makeItem(QString::number(point.index)));
        m_pointsTable->setItem(row, 1, makeItem(point.timeText));
        m_pointsTable->setItem(row, 2, makeItem(point.valueText));
        m_pointsTable->setItem(row, 3, makeItem(point.orientationText));
    }
    if(points.empty()) {
        m_pointsTable->setRowCount(1);
        m_pointsTable->setSpan(0, 0, 1, 4);
        m_pointsTable->setItem(0, 0, makeItem(emptyText));
    }
    m_pointsTable->resizeColumnToContents(0);
    m_pointsTable->resizeColumnToContents(1);
    m_pointsTable->resizeColumnToContents(3);
}
