#pragma once
#include <array>
#include <QString>
#include <QVector>
#include <vector>

namespace robot_qt_viewer
{
    enum class TelemetrySpeedCalculationMethod { InstantaneousDifference, WindowAverage };
    struct TelemetryEndpoint { QString id; QString name; };
    struct TelemetrySpeedPoint {
        double elapsedSeconds = 0.0;
        double tcpLinearMmPerSecond = 0.0;
        double tcpAngularDegPerSecond = 0.0;
        QVector<double> jointAbsSpeed;
    };
    struct TelemetrySpeedStatistics {
        bool valid = false;
        TelemetrySpeedCalculationMethod method = TelemetrySpeedCalculationMethod::InstantaneousDifference;
        double minimumIntervalSeconds = 0.0;
        double startSeconds = 0.0;
        double endSeconds = 0.0;
        quint64 sampleCount = 0;
        double tcpLinearMaxMmPerSecond = 0.0;
        double tcpLinearAverageMmPerSecond = 0.0;
        double tcpAngularMaxDegPerSecond = 0.0;
        double tcpAngularAverageDegPerSecond = 0.0;
        QVector<double> jointMaxSpeed;
        QVector<double> jointAverageSpeed;
        QVector<TelemetrySpeedPoint> points;
        QString diagnostic;
    };
    struct TelemetrySnapshot {
        bool busy = false;
        bool streaming = false;
        bool fault = false;
        QVector<TelemetryEndpoint> endpoints;
        QString source;
        QString diagnostic;
        QString csvPath;
        quint64 sequence = 0;
        qint64 receivedUnixNs = 0;
        qint64 elapsedNs = 0;
        qint64 readLatencyNs = 0;
        std::vector<double> joints;
        bool tcpPoseValid = false;
        std::array<double, 6> tcpXyzwpr{}; // FANUC WORLD, mm / degrees.
        int tcpUserFrame = -1;
        int tcpUserTool = -1;
        QString tcpDiagnostic;
    };
    class IRobotQtViewerTelemetryPort {
    public:
        virtual ~IRobotQtViewerTelemetryPort() = default;
        virtual bool discover() = 0;
        virtual bool start(const QString& endpointId, int group, int intervalMs, const QString& csvPath) = 0;
        virtual void stop() = 0;
        virtual TelemetrySnapshot snapshot() const = 0;
        virtual TelemetrySpeedStatistics speedStatistics(double startSeconds, double endSeconds,
            TelemetrySpeedCalculationMethod method = TelemetrySpeedCalculationMethod::InstantaneousDifference,
            double minimumIntervalSeconds = 0.0) const = 0;
    };
}
