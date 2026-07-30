#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace cdf_gui
{
    struct CanvasObstacle
    {
        double x = 0.0;
        double y = 0.0;
        double radius = 0.0;
        QString name;
    };

    struct CanvasModel
    {
        QVector<CanvasObstacle> obstacles;
        QVector<QPointF> initialPath;
        QVector<QPointF> repairedPath;
        QPointF start;
        QPointF goal;
        QRectF bounds;
    };

    struct CdfDemoViewModel
    {
        QString title;
        QString summary;
        QString clearanceText;
        QString gradientText;
        QString seedText;
        QString repairText;
        QString statusText;
        double targetClearance = 0.0;
        bool collisionFree = false;
        QVector<QVector<double>> robotJointPath;
        QVector<double> startJoints;
        QVector<double> goalJoints;
        CanvasModel canvas;
    };
}
