#include "CdfDemoViewModelBuilder.h"

#include "CDFAlgorithms/CdfCollisionOracle.h"

#include <QPointF>
#include <QString>

#include <algorithm>
#include <cmath>

namespace cdf_gui
{
    namespace
    {
        QVector<double> toVector(const cdf::JointVector& values)
        {
            QVector<double> result;
            result.reserve(static_cast<int>(values.size()));
            for (double value : values)
                result.push_back(value);
            return result;
        }

        QVector<QVector<double>> toJointPath(const cdf::Path& path)
        {
            QVector<QVector<double>> result;
            result.reserve(static_cast<int>(path.size()));
            for (const cdf::JointVector& q : path)
                result.push_back(toVector(q));
            return result;
        }

        QPointF toolPoint(const cdf::JointVector& q)
        {
            if (q.size() == 3)
            {
                const cdf::ThreeDofRobotPose pose = cdf::ThreeDofRobot3RModel().forwardKinematics(q);
                return QPointF(pose.tool.x, pose.tool.y);
            }
            if (q.size() >= 2)
                return QPointF(q[0], q[1]);
            return QPointF();
        }

        QVector<QPointF> toToolPoints(const cdf::Path& path)
        {
            QVector<QPointF> points;
            points.reserve(static_cast<int>(path.size()));
            for (const cdf::JointVector& q : path)
                points.push_back(toolPoint(q));
            return points;
        }

        QString demoTitle(cdf::DemoCase demoCase)
        {
            switch (demoCase)
            {
            case cdf::DemoCase::DistanceField:
                return QStringLiteral("Online CDF Distance Field");
            case cdf::DemoCase::OmplSeed:
                return QStringLiteral("OMPL Seed Path");
            case cdf::DemoCase::Repair:
                return QStringLiteral("CDF Path Repair");
            }
            return QStringLiteral("CDF Planning");
        }

        QString strategyText(const cdf::PlanningRequest& request, const cdf::PlanningResult& result)
        {
            if (result.statistics.usedProvidedOmplSeed)
                return QStringLiteral("provided OMPL seed");
            if (result.statistics.usedSampledTreeSeed)
                return QStringLiteral("OMPL-style sampled seed");
            if (request.initialPathStrategy == cdf::InitialPathStrategy::OmplSeedPath)
                return QStringLiteral("OMPL seed requested, falling back to linear seed");
            return QStringLiteral("linear seed");
        }

        cdf::JointVector midpoint(const cdf::PlanningRequest& request)
        {
            cdf::JointVector q = request.start;
            for (std::size_t i = 0; i < q.size(); ++i)
                q[i] = 0.5 * (request.start[i] + request.goal[i]);
            return q;
        }
    }

    CdfDemoViewModel CdfDemoViewModelBuilder::build(const cdf::CdfDocument& document) const
    {
        const cdf::PlanningRequest& request = document.request();
        const cdf::PlanningResult& result = document.result();
        const cdf::ClearanceSample sample = document.evaluateAt(midpoint(request));

        CdfDemoViewModel model;
        model.title = demoTitle(document.activeDemo());
        model.targetClearance = request.targetClearance;
        model.collisionFree = result.statistics.collisionFree;
        model.summary = QStringLiteral("start [%1, %2, %3]  goal [%4, %5, %6]")
            .arg(request.start[0], 0, 'f', 2)
            .arg(request.start[1], 0, 'f', 2)
            .arg(request.start.size() > 2 ? request.start[2] : 0.0, 0, 'f', 2)
            .arg(request.goal[0], 0, 'f', 2)
            .arg(request.goal[1], 0, 'f', 2)
            .arg(request.goal.size() > 2 ? request.goal[2] : 0.0, 0, 'f', 2);
        model.clearanceText = QStringLiteral("min before %1, min after %2")
            .arg(result.statistics.minClearanceBeforeRepair, 0, 'f', 4)
            .arg(result.statistics.minClearanceAfterRepair, 0, 'f', 4);
        model.gradientText = QStringLiteral("midpoint d=%1, grad=[%2, %3, %4]")
            .arg(sample.signedDistance, 0, 'f', 4)
            .arg(sample.gradient.size() > 0 ? sample.gradient[0] : 0.0, 0, 'f', 3)
            .arg(sample.gradient.size() > 1 ? sample.gradient[1] : 0.0, 0, 'f', 3)
            .arg(sample.gradient.size() > 2 ? sample.gradient[2] : 0.0, 0, 'f', 3);
        model.seedText = QStringLiteral("%1, %2 waypoints")
            .arg(strategyText(request, result))
            .arg(result.statistics.waypointCountBeforeRepair);
        model.repairText = QStringLiteral("CDF-QP repair: %1 iterations, %2 waypoints")
            .arg(result.statistics.repairIterations)
            .arg(result.statistics.waypointCountAfterRepair);
        model.statusText = QString::fromStdString(result.message);
        model.robotJointPath = toJointPath(result.repairedPath.empty() ? result.initialPath : result.repairedPath);
        model.startJoints = toVector(request.start);
        model.goalJoints = toVector(request.goal);

        for (const cdf::CircularObstacle& obstacle : request.obstacles)
        {
            model.canvas.obstacles.push_back({
                obstacle.x,
                obstacle.y,
                obstacle.radius + request.safetyMargin,
                QString::fromStdString(obstacle.name)
            });
        }
        model.canvas.initialPath = toToolPoints(result.initialPath);
        model.canvas.repairedPath = toToolPoints(result.repairedPath);
        model.canvas.start = toolPoint(request.start);
        model.canvas.goal = toolPoint(request.goal);
        model.canvas.bounds = QRectF(-0.58, -0.50, 1.16, 1.0);
        return model;
    }
}
