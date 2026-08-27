#include "CollisionDetectorConfigWidget.h"

#include "CollisionDetectorsWidget.h"
#include "CollisionSelectionSetsWidget.h"

#include <QVBoxLayout>

CollisionDetectorConfigWidget::CollisionDetectorConfigWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(8);

    m_detectorsWidget = new CollisionDetectorsWidget(this);
    rootLayout->addWidget(m_detectorsWidget, 1);

    m_selectionSetsWidget = new CollisionSelectionSetsWidget(this);
    m_selectionSetsWidget->hide();
}

CollisionSelectionSetsWidget* CollisionDetectorConfigWidget::selectionSetsWidget() const
{
    return m_selectionSetsWidget;
}

CollisionDetectorsWidget* CollisionDetectorConfigWidget::detectorsWidget() const
{
    return m_detectorsWidget;
}
