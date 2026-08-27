#include "CollisionLinkModelsWidget.h"

#include "RobotQtWidgetUtils.h"
#include "RobotQtViewerLocalization.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace
{
    using robot_qt_viewer::makeHorizontallyCompressible;
    using robot_qt_viewer::makePanelTitle;

    constexpr int kVariantRoleRole = Qt::UserRole;
    constexpr int kVariantSourceRole = Qt::UserRole + 1;
    constexpr int kVariantIdRole = Qt::UserRole + 2;
    constexpr int kVariantCurrentRole = Qt::UserRole + 3;
    constexpr int kVariantComplexityRowsRole = Qt::UserRole + 4;

    QLabel* makeValueLabel(QWidget* parent)
    {
        auto* label = new QLabel(parent);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        makeHorizontallyCompressible(label);
        return label;
    }

    QFrame* makeSection(
        QWidget* parent,
        const QString& title,
        QVBoxLayout** contentLayout,
        QLabel** titleLabel)
    {
        auto* frame = new QFrame(parent);
        frame->setProperty("inspectorSection", true);
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(10, 9, 10, 10);
        layout->setSpacing(7);
        auto* sectionTitle = makePanelTitle(title, frame);
        sectionTitle->setWordWrap(true);
        layout->addWidget(sectionTitle);
        *contentLayout = layout;
        if(titleLabel != nullptr) {
            *titleLabel = sectionTitle;
        }
        return frame;
    }

    QString localizedText(const char* key, const char* englishFallback)
    {
        if(qApp != nullptr) {
            const auto* localization =
                robot_qt_viewer::RobotQtViewerLocalizationService::installedOnApplication(*qApp);
            if(localization != nullptr) {
                return localization->text(
                    QString::fromLatin1(key),
                    QString::fromLatin1(englishFallback));
            }
        }
        return QString::fromLatin1(englishFallback);
    }

    QString plainTextToHtml(const QString& text)
    {
        QString html = text.toHtmlEscaped();
        html.replace("\n", "<br/>");
        return html;
    }

    QStringList metricRowsToStringList(const QVector<CollisionLinkModelMetricRowView>& rows)
    {
        QStringList result;
        for(const CollisionLinkModelMetricRowView& row : rows) {
            result << row.label << row.value;
        }
        return result;
    }
}

CollisionLinkModelsWidget::CollisionLinkModelsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    QVBoxLayout* targetSectionLayout = nullptr;
    QFrame* targetSection = makeSection(
        this, "Current Tree Node", &targetSectionLayout, &m_targetTitleLabel);
    m_targetLabel = makeValueLabel(targetSection);
    m_targetLabel->setProperty("inspectorValue", true);
    targetSectionLayout->addWidget(m_targetLabel);
    layout->addWidget(targetSection);

    QVBoxLayout* variantsSectionLayout = nullptr;
    QFrame* variantsSection = makeSection(
        this, "Variants", &variantsSectionLayout, &m_variantsTitleLabel);
    m_variantTree = new QTreeWidget(variantsSection);
    makeHorizontallyCompressible(m_variantTree);
    m_variantTree->setColumnCount(3);
    m_variantTree->setRootIsDecorated(false);
    m_variantTree->setItemsExpandable(false);
    m_variantTree->setIndentation(0);
    m_variantTree->setUniformRowHeights(true);
    m_variantTree->setAlternatingRowColors(true);
    m_variantTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_variantTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_variantTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_variantTree->setTextElideMode(Qt::ElideRight);
    m_variantTree->header()->setStretchLastSection(false);
    m_variantTree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_variantTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_variantTree->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_variantTree->header()->resizeSection(0, 84);
    m_variantTree->header()->resizeSection(2, 110);
    m_variantTree->setMinimumHeight(142);
    m_variantTree->setMaximumHeight(220);
    connect(m_variantTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*) {
        updateActionButtonsEnabled();
        updateSelectedVariantSummary();
        emit variantSelectionChanged();
    });
    variantsSectionLayout->addWidget(m_variantTree, 1);

    m_setCurrentVariantButton = new QPushButton("Set Current", this);
    robot_qt_viewer::configureActionButton(
        m_setCurrentVariantButton,
        robot_qt_viewer::UiActionRole::Accent);
    m_setCurrentVariantButton->setToolTip("Sets the selected collision model variant as the current project model.");
    m_setCurrentVariantButton->setEnabled(false);
    connect(m_setCurrentVariantButton, &QPushButton::clicked,
        this, &CollisionLinkModelsWidget::setCurrentVariantRequested);
    variantsSectionLayout->addWidget(m_setCurrentVariantButton);

    m_statusLabel = makeValueLabel(variantsSection);
    m_statusLabel->setTextFormat(Qt::RichText);
    m_statusLabel->setProperty("inspectorStatus", true);
    variantsSectionLayout->addWidget(m_statusLabel);
    layout->addWidget(variantsSection);

    QVBoxLayout* generateSectionLayout = nullptr;
    QFrame* generateSection =
        makeSection(
            this,
            "Generate Simplified Collision Model",
            &generateSectionLayout,
            &m_generateTitleLabel);
    m_generateCoacdButton = new QPushButton("Generate with COACD", this);
    robot_qt_viewer::configureActionButton(
        m_generateCoacdButton,
        robot_qt_viewer::UiActionRole::Accent);
    m_generateCoacdButton->setToolTip(
        "Generates a simplified collision model variant from the selected model.");
    m_generateCoacdButton->setEnabled(false);
    connect(m_generateCoacdButton, &QPushButton::clicked,
        this, &CollisionLinkModelsWidget::generateCoacdRequested);
    generateSectionLayout->addWidget(m_generateCoacdButton);
    layout->addWidget(generateSection);

    QVBoxLayout* complexitySectionLayout = nullptr;
    QFrame* complexitySection = makeSection(
        this,
        "Collision Model Complexity Analysis",
        &complexitySectionLayout,
        &m_complexityTitleLabel);

    m_complexityTable = new QTableWidget(0, 2, complexitySection);
    m_complexityTable->setHorizontalHeaderLabels(QStringList() << "Metric" << "Value");
    m_complexityTable->verticalHeader()->hide();
    m_complexityTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_complexityTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_complexityTable->setFocusPolicy(Qt::NoFocus);
    m_complexityTable->setAlternatingRowColors(true);
    m_complexityTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_complexityTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_complexityTable->setShowGrid(true);
    m_complexityTable->setMinimumHeight(184);
    m_complexityTable->setMaximumHeight(184);
    m_complexityTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    m_complexityTable->verticalHeader()->setDefaultSectionSize(28);
    m_complexityTable->verticalHeader()->setMinimumSectionSize(24);
    m_complexityTable->verticalHeader()->setFixedWidth(0);
    m_complexityTable->horizontalHeader()->setMinimumSectionSize(56);
    m_complexityTable->horizontalHeader()->setStretchLastSection(true);
    m_complexityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_complexityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    complexitySectionLayout->addWidget(m_complexityTable);
    layout->addWidget(complexitySection);

    auto* exitLayout = new QHBoxLayout();
    exitLayout->setContentsMargins(0, 0, 0, 0);
    exitLayout->setSpacing(8);
    m_applyButton = new QPushButton("Apply", this);
    robot_qt_viewer::configureActionButton(
        m_applyButton,
        robot_qt_viewer::UiActionRole::Primary);
    m_applyButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_applyButton->setToolTip("Leaves collision model configuration and returns to detector configuration.");
    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked,
        this, &CollisionLinkModelsWidget::applyConfigurationRequested);
    exitLayout->addWidget(m_applyButton);

    m_cancelButton = new QPushButton("Cancel", this);
    robot_qt_viewer::configureActionButton(
        m_cancelButton,
        robot_qt_viewer::UiActionRole::Standard);
    m_cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    m_cancelButton->setToolTip("Cancels the current preview and returns to detector configuration.");
    m_cancelButton->setEnabled(false);
    connect(m_cancelButton, &QPushButton::clicked,
        this, &CollisionLinkModelsWidget::cancelConfigurationRequested);
    exitLayout->addWidget(m_cancelButton);
    layout->addLayout(exitLayout);
    layout->addStretch(1);
    retranslateUi();
}

void CollisionLinkModelsWidget::setViewModel(const CollisionLinkModelsViewModel& viewModel)
{
    setSummary(viewModel.summary);

    if(m_variantTree == nullptr) {
        return;
    }

    QSignalBlocker variantBlocker(m_variantTree);
    m_variantTree->clear();

    QTreeWidgetItem* selectedVariant = nullptr;
    QTreeWidgetItem* currentVariant = nullptr;
    for(const CollisionLinkModelVariantItemView& variant : viewModel.variants) {
        auto* item = new QTreeWidgetItem(m_variantTree);
        item->setText(0, variant.current ? "[x]" : "[ ]");
        item->setText(1, variant.label);
        QString geometryAndRole = variant.typeLabel;
        if(!variant.roleLabel.isEmpty()) {
            if(!geometryAndRole.isEmpty()) {
                geometryAndRole += QStringLiteral(" / ");
            }
            geometryAndRole += variant.roleLabel;
        }
        item->setText(2, geometryAndRole.isEmpty() ? QString("-") : geometryAndRole);
        item->setTextAlignment(0, Qt::AlignCenter);
        const QString tooltip = variant.tooltip.isEmpty() ? variant.detail : variant.tooltip;
        for(int column = 0; column < m_variantTree->columnCount(); ++column) {
            item->setToolTip(column, tooltip);
        }
        item->setData(0, kVariantRoleRole, variant.role);
        item->setData(0, kVariantSourceRole, variant.source);
        item->setData(0, kVariantIdRole, variant.variantId);
        item->setData(0, kVariantCurrentRole, variant.current);
        item->setData(0, kVariantComplexityRowsRole, metricRowsToStringList(variant.complexityRows));
        if(!variant.enabled) {
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
        }
        if(variant.current) {
            currentVariant = item;
        }
        if(variant.selected) {
            selectedVariant = item;
        }
    }

    if(selectedVariant == nullptr) {
        selectedVariant = currentVariant;
    }
    if(selectedVariant == nullptr && m_variantTree->topLevelItemCount() > 0) {
        selectedVariant = m_variantTree->topLevelItem(0);
    }
    if(selectedVariant != nullptr) {
        m_variantTree->setCurrentItem(selectedVariant);
    }
    updateActionButtonsEnabled();
    updateSelectedVariantSummary();
}

void CollisionLinkModelsWidget::setSummary(const CollisionLinkModelsSummaryView& summary)
{
    if(m_targetLabel != nullptr) {
        m_targetLabel->setText(summary.target.displayName.isEmpty()
            ? localizedText(
                "collisionConfig.model.target.none",
                "No tree node selected")
            : summary.target.displayName);
    }
    if(m_statusLabel != nullptr) {
        m_baseStatusText = summary.statusText;
        updateSelectedVariantSummary();
    }
}

QString CollisionLinkModelsWidget::currentVariantId() const
{
    const QTreeWidgetItem* item = m_variantTree != nullptr ? m_variantTree->currentItem() : nullptr;
    return item != nullptr ? item->data(0, kVariantIdRole).toString() : QString();
}

QString CollisionLinkModelsWidget::currentVariantRole() const
{
    const QTreeWidgetItem* item = m_variantTree != nullptr ? m_variantTree->currentItem() : nullptr;
    return item != nullptr ? item->data(0, kVariantRoleRole).toString() : QString();
}

QString CollisionLinkModelsWidget::currentVariantSource() const
{
    const QTreeWidgetItem* item = m_variantTree != nullptr ? m_variantTree->currentItem() : nullptr;
    return item != nullptr ? item->data(0, kVariantSourceRole).toString() : QString();
}

QString CollisionLinkModelsWidget::appliedVariantId() const
{
    if(m_variantTree == nullptr) {
        return QString();
    }

    for(int row = 0; row < m_variantTree->topLevelItemCount(); ++row) {
        const QTreeWidgetItem* item = m_variantTree->topLevelItem(row);
        if(item != nullptr && item->data(0, kVariantCurrentRole).toBool()) {
            return item->data(0, kVariantIdRole).toString();
        }
    }
    return QString();
}

QString CollisionLinkModelsWidget::appliedVariantSource() const
{
    if(m_variantTree == nullptr) {
        return QString();
    }

    for(int row = 0; row < m_variantTree->topLevelItemCount(); ++row) {
        const QTreeWidgetItem* item = m_variantTree->topLevelItem(row);
        if(item != nullptr && item->data(0, kVariantCurrentRole).toBool()) {
            return item->data(0, kVariantSourceRole).toString();
        }
    }
    return QString();
}

QString CollisionLinkModelsWidget::currentElementId() const
{
    return QString();
}

bool CollisionLinkModelsWidget::hasCurrentVariant() const
{
    return !currentVariantId().isEmpty();
}

bool CollisionLinkModelsWidget::selectAppliedVariant()
{
    if(m_variantTree == nullptr) {
        return false;
    }

    for(int row = 0; row < m_variantTree->topLevelItemCount(); ++row) {
        QTreeWidgetItem* item = m_variantTree->topLevelItem(row);
        if(item != nullptr && item->data(0, kVariantCurrentRole).toBool()) {
            m_variantTree->setCurrentItem(item);
            return true;
        }
    }
    return false;
}

bool CollisionLinkModelsWidget::selectVariantBySourceRole(const QString& source, const QString& role)
{
    if(m_variantTree == nullptr) {
        return false;
    }

    for(int row = 0; row < m_variantTree->topLevelItemCount(); ++row) {
        QTreeWidgetItem* item = m_variantTree->topLevelItem(row);
        if(item == nullptr) {
            continue;
        }
        if(item->data(0, kVariantSourceRole).toString() == source &&
            item->data(0, kVariantRoleRole).toString() == role) {
            m_variantTree->setCurrentItem(item);
            return true;
        }
    }
    return false;
}

robot_qt_viewer::CollisionRuntimeProxyRequest CollisionLinkModelsWidget::proxyRequest(
    const QString& proxyType,
    const std::string& fallbackRole,
    bool useExistingCollisionAsInput) const
{
    robot_qt_viewer::CollisionRuntimeProxyRequest request;
    request.proxyType = proxyType.toStdString();
    request.role = fallbackRole;
    request.useExistingCollisionAsInput = useExistingCollisionAsInput;
    return request;
}

bool CollisionLinkModelsWidget::replaceOriginal() const
{
    return false;
}

void CollisionLinkModelsWidget::setContextActionsEnabled(bool hasRobot, bool hasLink)
{
    (void)hasRobot;
    if(hasLink) {
        m_variantActionsAllowed = true;
    }
    updateActionButtonsEnabled();
}

void CollisionLinkModelsWidget::setVariantActionsEnabled(bool canUseVariant, bool canShowVariant)
{
    (void)canUseVariant;
    m_variantActionsAllowed = canShowVariant;
    updateActionButtonsEnabled();
}

void CollisionLinkModelsWidget::setTaskExitEnabled(bool enabled)
{
    m_taskExitAllowed = enabled;
    updateActionButtonsEnabled();
}

bool CollisionLinkModelsWidget::selectedVariantIsCurrent() const
{
    const QTreeWidgetItem* item = m_variantTree != nullptr ? m_variantTree->currentItem() : nullptr;
    return item != nullptr && item->data(0, kVariantCurrentRole).toBool();
}

void CollisionLinkModelsWidget::updateSelectedVariantSummary()
{
    if(m_statusLabel == nullptr) {
        return;
    }

    QString text;
    if(!m_baseStatusText.isEmpty()) {
        text = plainTextToHtml(m_baseStatusText);
    }

    m_statusLabel->setText(text);

    const QTreeWidgetItem* item = m_variantTree != nullptr ? m_variantTree->currentItem() : nullptr;
    updateComplexityTable(item != nullptr
        ? item->data(0, kVariantComplexityRowsRole).toStringList()
        : QStringList());
}

void CollisionLinkModelsWidget::updateComplexityTable(const QStringList& rows)
{
    if(m_complexityTable == nullptr) {
        return;
    }

    const int rowCount = rows.size() / 2;
    m_complexityTable->setRowCount(rowCount);
    for(int row = 0; row < rowCount; ++row) {
        auto* labelItem = new QTableWidgetItem(rows.value(row * 2));
        auto* valueItem = new QTableWidgetItem(rows.value(row * 2 + 1));
        labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_complexityTable->setItem(row, 0, labelItem);
        m_complexityTable->setItem(row, 1, valueItem);
    }

    m_complexityTable->setVisible(rowCount > 0);
    if(m_complexityTable->parentWidget() != nullptr) {
        m_complexityTable->parentWidget()->setVisible(rowCount > 0);
    }
    QTimer::singleShot(0, this, &CollisionLinkModelsWidget::resetComplexityTableColumnWidths);
}

void CollisionLinkModelsWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if(event != nullptr && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void CollisionLinkModelsWidget::retranslateUi()
{
    if(m_targetTitleLabel != nullptr) {
        m_targetTitleLabel->setText(localizedText(
            "collisionConfig.model.target.title", "Current Tree Node"));
    }
    if(m_variantsTitleLabel != nullptr) {
        m_variantsTitleLabel->setText(localizedText(
            "collisionConfig.model.variants.title", "Variants"));
    }
    if(m_generateTitleLabel != nullptr) {
        m_generateTitleLabel->setText(localizedText(
            "collisionConfig.model.generate.title", "Generate Simplified Collision Model"));
    }
    if(m_complexityTitleLabel != nullptr) {
        m_complexityTitleLabel->setText(localizedText(
            "collisionConfig.model.complexity.title", "Collision Model Complexity Analysis"));
    }
    if(m_variantTree != nullptr) {
        m_variantTree->setHeaderLabels(QStringList()
            << localizedText("collisionConfig.model.variants.current", "Used")
            << localizedText("collisionConfig.model.variants.variant", "Variant")
            << localizedText("collisionConfig.model.variants.model", "Model"));
    }
    if(m_setCurrentVariantButton != nullptr) {
        m_setCurrentVariantButton->setText(localizedText(
            "collisionConfig.model.action.setCurrent", "Set Current"));
        m_setCurrentVariantButton->setToolTip(localizedText(
            "collisionConfig.model.action.setCurrent.tooltip",
            "Sets the selected collision model variant as the current project model."));
    }
    if(m_generateCoacdButton != nullptr) {
        m_generateCoacdButton->setText(localizedText(
            "collisionConfig.model.action.generateCoacd",
            "Generate with COACD"));
        m_generateCoacdButton->setToolTip(localizedText(
            "collisionConfig.model.action.generateCoacd.tooltip",
            "Generates a simplified collision model variant from the selected model."));
    }
    if(m_complexityTable != nullptr) {
        m_complexityTable->setHorizontalHeaderLabels(QStringList()
            << localizedText("collisionConfig.model.complexity.metric", "Metric")
            << localizedText("collisionConfig.model.complexity.value", "Value"));
    }
    if(m_applyButton != nullptr) {
        m_applyButton->setText(localizedText(
            "collisionConfig.model.action.apply", "Apply"));
        m_applyButton->setToolTip(localizedText(
            "collisionConfig.model.action.apply.tooltip",
            "Leaves collision model configuration and returns to detector configuration."));
    }
    if(m_cancelButton != nullptr) {
        m_cancelButton->setText(localizedText(
            "collisionConfig.model.action.cancel", "Cancel"));
        m_cancelButton->setToolTip(localizedText(
            "collisionConfig.model.action.cancel.tooltip",
            "Cancels the current preview and returns to detector configuration."));
    }
}

void CollisionLinkModelsWidget::resetComplexityTableColumnWidths()
{
    if(m_complexityTable == nullptr || !m_complexityTable->isVisible()) {
        return;
    }

    const int width = m_complexityTable->viewport()->width();
    if(width <= 0) {
        return;
    }

    const int firstColumnWidth = width / 2;
    m_complexityTable->horizontalHeader()->resizeSection(0, firstColumnWidth);
    m_complexityTable->horizontalHeader()->resizeSection(1, width - firstColumnWidth);
}

void CollisionLinkModelsWidget::updateActionButtonsEnabled()
{
    if(m_setCurrentVariantButton != nullptr) {
        m_setCurrentVariantButton->setEnabled(
            m_variantActionsAllowed &&
            hasCurrentVariant() &&
            !selectedVariantIsCurrent());
    }
    if(m_generateCoacdButton != nullptr) {
        m_generateCoacdButton->setEnabled(m_variantActionsAllowed);
    }
    if(m_applyButton != nullptr) {
        m_applyButton->setEnabled(m_taskExitAllowed);
    }
    if(m_cancelButton != nullptr) {
        m_cancelButton->setEnabled(m_taskExitAllowed);
    }
}
