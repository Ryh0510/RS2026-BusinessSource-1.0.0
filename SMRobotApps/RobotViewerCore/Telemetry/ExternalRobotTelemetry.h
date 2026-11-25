#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace robot_telemetry
{
    struct Endpoint { std::string id; std::string name; };
    struct CartesianPose {
        bool valid = false;
        // FANUC WORLD (UF0), active tool TCP. XYZ in mm, WPR in degrees.
        // Orientation follows Rz(R) * Ry(P) * Rx(W).
        std::array<double, 6> xyzwpr{};
        int userFrame = -1;
        int userTool = -1;
        std::string diagnostic;
    };
    struct Reading {
        std::vector<double> joints;
        CartesianPose tcpPose;
    };
    struct Sample {
        std::uint64_t sequence = 0;
        std::int64_t receivedUnixNs = 0;
        std::int64_t elapsedNs = 0;
        std::int64_t readLatencyNs = 0;
        std::vector<double> joints;
        CartesianPose tcpPose;
    };
    enum class State { Idle, Discovering, Connecting, Streaming, Stopped, Fault };
    struct SpeedPoint {
        double elapsedSeconds = 0.0;
        double tcpLinearMmPerSecond = 0.0;
        double tcpAngularDegPerSecond = 0.0;
        std::vector<double> jointAbsSpeed;
    };
    enum class SpeedCalculationMethod { InstantaneousDifference, WindowAverage };
    struct SpeedStatistics {
        bool valid = false;
        SpeedCalculationMethod method = SpeedCalculationMethod::InstantaneousDifference;
        double minimumIntervalSeconds = 0.0;
        double startSeconds = 0.0;
        double endSeconds = 0.0;
        std::size_t sampleCount = 0;
        double tcpLinearMaxMmPerSecond = 0.0;
        double tcpLinearAverageMmPerSecond = 0.0;
        double tcpAngularMaxDegPerSecond = 0.0;
        double tcpAngularAverageDegPerSecond = 0.0;
        std::vector<double> jointMaxSpeed;
        std::vector<double> jointAverageSpeed;
        std::vector<SpeedPoint> points;
        std::string diagnostic;
    };
    struct Snapshot {
        State state = State::Idle;
        std::vector<Endpoint> endpoints;
        std::string source;
        std::string diagnostic;
        std::filesystem::path csvPath;
        int group = 1;
        int intervalMs = 20;
        Sample latest;
    };
    struct Options {
        std::string endpointId; // Empty selects the single running virtual controller.
        int group = 1;
        int intervalMs = 20;
        std::filesystem::path csvPath;
    };
    // A backend is created, used and destroyed exclusively on the sampling thread.
    class Backend {
    public:
        virtual ~Backend() = default;
        virtual std::vector<Endpoint> discover() = 0;
        virtual void attach(const std::string& endpointId, int group) = 0;
        virtual Reading read() = 0;
    };
    SpeedStatistics calculateSpeedStatistics(const std::vector<Sample>& samples,
        double startSeconds, double endSeconds,
        SpeedCalculationMethod method = SpeedCalculationMethod::InstantaneousDifference,
        double minimumIntervalSeconds = 0.0);
    std::unique_ptr<Backend> makeRoboguideBackend();
    class Session final {
    public:
        using BackendFactory = std::unique_ptr<Backend>(*)();
        explicit Session(BackendFactory factory = makeRoboguideBackend);
        ~Session();
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        bool discover();
        bool start(const Options& options);
        void stop(); // Requests stop without blocking the UI on a COM call.
        Snapshot snapshot() const;
        SpeedStatistics speedStatistics(double startSeconds, double endSeconds,
            SpeedCalculationMethod method = SpeedCalculationMethod::InstantaneousDifference,
            double minimumIntervalSeconds = 0.0) const;
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
