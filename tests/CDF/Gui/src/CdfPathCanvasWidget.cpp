#include "CdfPathCanvasWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>

#include <algorithm>

namespace cdf_gui
{
    namespace
    {
        QRectF drawingRect(const QWidget& widget)
        {
            return widget.rect().adjusted(18, 18, -18, -18);
        }
    }

    CdfPathCanvasWidget::CdfPathCanvasWidget(QWidget* parent)
        : QWidget(parent)
    {
        setMinimumSize(520, 420);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void CdfPathCanvasWidget::setModel(const CanvasModel& model)
    {
        m_model = model;
        update();
    }

    void CdfPathCanvasWidget::paintEvent(QPaintEvent* event)
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor(246, 247, 249));

        const QRectF area = drawingRect(*this);
        painter.setPen(QPen(QColor(188, 196, 207), 1));
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRect(area);

        painter.setPen(QPen(QColor(229, 233, 239), 1));
        for (int i = 1; i < 8; ++i)
        {
            const double x = area.left() + area.width() * i / 8.0;
            const double y = area.top() + area.height() * i / 8.0;
            painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
            painter.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(218, 82, 74, 115));
        for (const CanvasObstacle& obstacle : m_model.obstacles)
            painter.drawEllipse(toWidgetCircle(obstacle));

        const auto drawPath = [&](const QVector<QPointF>& points, const QColor& color, Qt::PenStyle style, int width) {
            if (points.size() < 2)
                return;
            QPen pen(color, width);
            pen.setStyle(style);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            QPainterPath painterPath;
            painterPath.moveTo(toWidget(points.front()));
            for (int i = 1; i < points.size(); ++i)
                painterPath.lineTo(toWidget(points[i]));
            painter.drawPath(painterPath);
        };

        drawPath(m_model.initialPath, QColor(92, 116, 145), Qt::DashLine, 2);
        drawPath(m_model.repairedPath, QColor(31, 132, 106), Qt::SolidLine, 4);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(33, 93, 190));
        painter.drawEllipse(toWidget(m_model.start), 6, 6);
        painter.setBrush(QColor(27, 150, 88));
        painter.drawEllipse(toWidget(m_model.goal), 6, 6);
    }

    QPointF CdfPathCanvasWidget::toWidget(const QPointF& point) const
    {
        const QRectF area = drawingRect(*this);
        const QRectF bounds = m_model.bounds.isValid() ? m_model.bounds : QRectF(-2.0, -1.6, 4.0, 3.2);
        const double x = area.left() + (point.x() - bounds.left()) / bounds.width() * area.width();
        const double y = area.bottom() - (point.y() - bounds.top()) / bounds.height() * area.height();
        return QPointF(x, y);
    }

    QRectF CdfPathCanvasWidget::toWidgetCircle(const CanvasObstacle& obstacle) const
    {
        const QPointF center = toWidget(QPointF(obstacle.x, obstacle.y));
        const QRectF area = drawingRect(*this);
        const QRectF bounds = m_model.bounds.isValid() ? m_model.bounds : QRectF(-2.0, -1.6, 4.0, 3.2);
        const double scale = std::min(area.width() / bounds.width(), area.height() / bounds.height());
        const double radius = obstacle.radius * scale;
        return QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0);
    }
}
