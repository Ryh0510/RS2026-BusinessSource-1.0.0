#pragma once
#include <QJsonObject>
#include <QString>
#include <atomic>
#include <functional>

namespace robot_qt_viewer {
// File/Qt adapter only; numerical ownership is ChaomaiPlaneOptimizer.
QJsonObject runNativePlaneOptimization(const QJsonObject& request, const QString& runDirectory,
    bool compareOnly, const std::atomic_bool& cancelled,
    const std::function<void(double, const QString&)>& progress);
}
