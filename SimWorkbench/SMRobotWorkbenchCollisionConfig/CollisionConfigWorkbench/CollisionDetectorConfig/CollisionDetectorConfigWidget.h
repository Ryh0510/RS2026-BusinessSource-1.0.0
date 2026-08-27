#pragma once

#include <QWidget>

class CollisionDetectorsWidget;
class CollisionSelectionSetsWidget;

class CollisionDetectorConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CollisionDetectorConfigWidget(QWidget* parent = nullptr);

    CollisionSelectionSetsWidget* selectionSetsWidget() const;
    CollisionDetectorsWidget* detectorsWidget() const;

private:
    CollisionSelectionSetsWidget* m_selectionSetsWidget = nullptr;
    CollisionDetectorsWidget* m_detectorsWidget = nullptr;
};
