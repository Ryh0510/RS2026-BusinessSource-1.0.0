#include "CollisionDetectorQueryDialog.h"

#include "RobotQtWidgetUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace
{
    using robot_qt_viewer::configureInspectorCombo;
    using robot_qt_viewer::configureInspectorForm;
    using robot_qt_viewer::makeHorizontallyCompressible;

}

CollisionDetectorQueryDialog::CollisionDetectorQueryDialog(
    const CollisionDetectorQueryContractView& contract,
    QWidget* parent)
    : QDialog(parent)
    , m_initialContract(contract)
{
    setWindowTitle("Configure Detector");
    setModal(true);
    resize(720, 560);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    auto* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    configureInspectorForm(form);

    m_idLabel = new QLabel(this);
    m_idLabel->setText(contract.id.isEmpty() ? QStringLiteral("<unassigned>") : contract.id);
    m_idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_idLabel->setToolTip("Stable detector id used by other modules.");
    form->addRow("id", m_idLabel);

    m_nameEdit = new QLineEdit(this);
    makeHorizontallyCompressible(m_nameEdit);
    m_nameEdit->setText(contract.name);
    m_nameEdit->setToolTip("Detector display name. Keep names unique when external modules query detectors by name.");
    form->addRow("name", m_nameEdit);

    m_collisionCheck = new QCheckBox("Collision", this);
    robot_qt_viewer::configureInspectorToggle(m_collisionCheck);
    m_collisionCheck->setChecked(true);
    m_collisionCheck->setEnabled(false);
    m_collisionCheck->setToolTip("Boolean collision detection is always performed by an enabled detector.");
    form->addRow(m_collisionCheck);

    m_contactsCheck = new QCheckBox("Contacts", this);
    robot_qt_viewer::configureInspectorToggle(m_contactsCheck);
    m_contactsCheck->setChecked(contract.contacts);
    m_contactsCheck->setToolTip("Request contact points and fill the Contacts result table.");
    form->addRow(m_contactsCheck);

    m_maxContactsSpin = new QDoubleSpinBox(this);
    m_maxContactsSpin->setDecimals(0);
    m_maxContactsSpin->setRange(0.0, 10000.0);
    m_maxContactsSpin->setSingleStep(1.0);
    m_maxContactsSpin->setValue(contract.maxContacts);
    m_maxContactsSpin->setToolTip("Maximum number of contact points; available when Contacts is enabled.");
    m_maxContactsSpin->setEnabled(m_contactsCheck->isChecked());
    form->addRow("max contacts", m_maxContactsSpin);

    m_normalsCheck = new QCheckBox("Normals", this);
    robot_qt_viewer::configureInspectorToggle(m_normalsCheck);
    m_normalsCheck->setChecked(contract.normals);
    m_normalsCheck->setToolTip("Request contact normals when contact points are available.");
    form->addRow(m_normalsCheck);
    connect(m_contactsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if(m_maxContactsSpin != nullptr) {
            m_maxContactsSpin->setEnabled(checked);
        }
        if(m_normalsCheck != nullptr) {
            if(!checked) {
                m_normalsCheck->setChecked(false);
            }
            m_normalsCheck->setEnabled(checked);
        }
    });
    if(!m_contactsCheck->isChecked()) {
        m_normalsCheck->setChecked(false);
        m_normalsCheck->setEnabled(false);
    }

    m_nearestCheck = new QCheckBox("Nearest / distance", this);
    robot_qt_viewer::configureInspectorToggle(m_nearestCheck);
    m_nearestCheck->setChecked(contract.nearest);
    m_nearestCheck->setToolTip("Request nearest points and distance for this detector.");
    form->addRow(m_nearestCheck);

    m_distanceThresholdSpin = new QDoubleSpinBox(this);
    m_distanceThresholdSpin->setDecimals(4);
    m_distanceThresholdSpin->setRange(0.0, 1000000.0);
    m_distanceThresholdSpin->setSingleStep(0.01);
    m_distanceThresholdSpin->setValue(contract.distanceThreshold);
    m_distanceThresholdSpin->setToolTip("Distance threshold; available when Nearest / distance is enabled.");
    m_distanceThresholdSpin->setEnabled(m_nearestCheck->isChecked());
    form->addRow("distance threshold", m_distanceThresholdSpin);
    connect(m_nearestCheck, &QCheckBox::toggled,
        m_distanceThresholdSpin, &QWidget::setEnabled);

    rootLayout->addLayout(form);

    auto* bindingLabel = new QLabel("Target model bindings", this);
    rootLayout->addWidget(bindingLabel);
    m_bindingTable = new QTableWidget(contract.modelBindings.size(), 4, this);
    m_bindingTable->setHorizontalHeaderLabels(
        QStringList() << "Target" << "Context" << "Mode" << "Model ID");
    m_bindingTable->verticalHeader()->hide();
    m_bindingTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_bindingTable->setMinimumHeight(150);
    m_bindingTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_bindingTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_bindingTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_bindingTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    for(int row = 0; row < contract.modelBindings.size(); ++row) {
        const CollisionDetectorModelBindingView& binding = contract.modelBindings[row];
        auto* targetItem = new QTableWidgetItem(binding.targetLabel);
        targetItem->setFlags(targetItem->flags() & ~Qt::ItemIsEditable);
        targetItem->setToolTip(binding.targetLabel);
        m_bindingTable->setItem(row, 0, targetItem);

        auto* contextItem = new QTableWidgetItem(binding.context);
        contextItem->setFlags(contextItem->flags() & ~Qt::ItemIsEditable);
        m_bindingTable->setItem(row, 1, contextItem);

        auto* modeCombo = new QComboBox(m_bindingTable);
        configureInspectorCombo(modeCombo);
        modeCombo->addItem("Current", "Current");
        modeCombo->addItem("Explicit model", "ExplicitModel");
        const int modeIndex = modeCombo->findData(binding.mode);
        modeCombo->setCurrentIndex(modeIndex >= 0 ? modeIndex : 0);
        m_bindingTable->setCellWidget(row, 2, modeCombo);
        m_bindingModeCombos.push_back(modeCombo);

        auto* modelIdEdit = new QLineEdit(m_bindingTable);
        makeHorizontallyCompressible(modelIdEdit);
        modelIdEdit->setText(binding.modelId);
        modelIdEdit->setPlaceholderText("Stable modelId");
        modelIdEdit->setEnabled(modeCombo->currentData().toString() == "ExplicitModel");
        m_bindingTable->setCellWidget(row, 3, modelIdEdit);
        m_bindingModelIdEdits.push_back(modelIdEdit);
        connect(modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [modeCombo, modelIdEdit](int) {
                const bool explicitModel = modeCombo->currentData().toString() == "ExplicitModel";
                modelIdEdit->setEnabled(explicitModel);
                if(!explicitModel) {
                    modelIdEdit->clear();
                }
            });
    }
    if(contract.modelBindings.isEmpty()) {
        m_bindingTable->setRowCount(1);
        auto* emptyItem = new QTableWidgetItem("No concrete detector target is available for model binding.");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsEditable);
        m_bindingTable->setItem(0, 0, emptyItem);
        m_bindingTable->setSpan(0, 0, 1, 4);
    }
    rootLayout->addWidget(m_bindingTable, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    robot_qt_viewer::configureDialogButtonBox(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    rootLayout->addWidget(buttons);
}

CollisionDetectorQueryContractView CollisionDetectorQueryDialog::contract() const
{
    CollisionDetectorQueryContractView view = m_initialContract;
    view.name = m_nameEdit != nullptr ? m_nameEdit->text().trimmed() : QString();
    view.contacts = m_contactsCheck != nullptr && m_contactsCheck->isChecked();
    view.normals = m_normalsCheck != nullptr && m_normalsCheck->isChecked();
    view.nearest = m_nearestCheck != nullptr && m_nearestCheck->isChecked();
    view.maxContacts = m_maxContactsSpin != nullptr ? m_maxContactsSpin->value() : 0.0;
    view.distanceThreshold = m_distanceThresholdSpin != nullptr ? m_distanceThresholdSpin->value() : 0.0;
    for(int row = 0; row < view.modelBindings.size() && row < m_bindingModeCombos.size(); ++row) {
        view.modelBindings[row].mode = m_bindingModeCombos[row]->currentData().toString();
        view.modelBindings[row].modelId = view.modelBindings[row].mode == "ExplicitModel"
            ? m_bindingModelIdEdits[row]->text().trimmed()
            : QString();
    }
    return view;
}
