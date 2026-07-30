#pragma once

#include "CdfDemoViewModel.h"

#include <QWidget>

namespace cdf_gui
{
    class CdfPathCanvasWidget final : public QWidget
    {
        Q_OBJECT

    public:
        explicit CdfPathCanvasWidget(QWidget* parent = nullptr);

        void setModel(const CanvasModel& model);

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        QPointF toWidget(const QPointF& point) const;
        QRectF toWidgetCircle(const CanvasObstacle& obstacle) const;

        CanvasModel m_model;
    };
}
