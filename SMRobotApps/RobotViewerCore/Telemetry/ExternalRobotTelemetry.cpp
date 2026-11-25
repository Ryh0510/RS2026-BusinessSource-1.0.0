#include "ExternalRobotTelemetry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <limits>
#include <stdexcept>
#include <thread>

namespace robot_telemetry
{
    namespace {
        using Steady = std::chrono::steady_clock;
        std::int64_t ns(Steady::duration duration) {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        }
        std::string csv(const std::string& value) {
            std::string result = "\"";
            for(char c : value) { if(c == '"') result += '"'; result += c; }
            return result + '"';
        }
    }
    SpeedStatistics calculateSpeedStatistics(const std::vector<Sample>& samples,
        double startSeconds, double endSeconds,
        SpeedCalculationMethod method, double minimumIntervalSeconds)
    {
        SpeedStatistics result;
        result.startSeconds = startSeconds; result.endSeconds = endSeconds;
        result.method = method; result.minimumIntervalSeconds = minimumIntervalSeconds;
        if(!std::isfinite(startSeconds) || !std::isfinite(endSeconds) || startSeconds < 0. || endSeconds <= startSeconds) {
            result.diagnostic = "Select a positive time interval"; return result;
        }
        if(method != SpeedCalculationMethod::InstantaneousDifference && method != SpeedCalculationMethod::WindowAverage) {
            result.diagnostic = "Unknown speed calculation method"; return result;
        }
        if(method == SpeedCalculationMethod::WindowAverage &&
            (!std::isfinite(minimumIntervalSeconds) || minimumIntervalSeconds <= 0.)) {
            result.diagnostic = "Select a positive minimum calculation interval"; return result;
        }
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double pi = 3.14159265358979323846;
        const auto finite = [](const auto& v) {
            return std::all_of(v.begin(), v.end(), [](double x) { return std::isfinite(x); });
        };
        // Unit quaternion for FANUC Rz(R)*Ry(P)*Rx(W). Euler-component length
        // is not a physical rotation angle, notably at P=+/-90 degrees.
        const auto quaternion = [pi](const std::array<double, 6>& pose) {
            const double w = std::remainder(pose[3], 360.) * pi / 360.;
            const double p = std::remainder(pose[4], 360.) * pi / 360.;
            const double r = std::remainder(pose[5], 360.) * pi / 360.;
            const double cw = std::cos(w), sw = std::sin(w), cp = std::cos(p), sp = std::sin(p);
            const double cr = std::cos(r), sr = std::sin(r);
            return std::array<double, 4>{cr*cp*cw + sr*sp*sw, cr*cp*sw - sr*sp*cw,
                cr*sp*cw + sr*cp*sw, sr*cp*cw - cr*sp*sw};
        };
        struct Segment {
            double start = 0., end = 0.;
            std::vector<double> joints;
            double linear = 0., angular = 0.;
            bool tcpValid = false;
        };
        std::vector<Segment> segments;
        std::size_t dimensions = 0;
        for(std::size_t i = 1; i < samples.size(); ++i) {
            const auto& a = samples[i - 1]; const auto& b = samples[i];
            if(a.elapsedNs < 0 || b.elapsedNs <= a.elapsedNs) {
                result.diagnostic = "Sample elapsed timestamps must be strictly increasing"; return result;
            }
            const double ta = a.elapsedNs * 1.e-9, tb = b.elapsedNs * 1.e-9;
            if(tb <= startSeconds || ta >= endSeconds || a.joints.empty() ||
                a.joints.size() != b.joints.size() || !finite(a.joints) || !finite(b.joints)) continue;
            if(dimensions == 0) dimensions = a.joints.size();
            if(a.joints.size() != dimensions) continue;
            const double dt = (b.elapsedNs - a.elapsedNs) * 1.e-9;
            Segment segment; segment.start = std::max(ta, startSeconds); segment.end = std::min(tb, endSeconds);
            segment.joints.resize(dimensions);
            for(std::size_t j = 0; j < dimensions; ++j)
                // Keep native controller axes and full turns. Model J2/J3
                // coupling belongs only to scene following, not encoder speed.
                segment.joints[j] = std::abs(b.joints[j] - a.joints[j]) / dt;
            if(!finite(segment.joints)) continue;
            if(a.tcpPose.valid && b.tcpPose.valid && finite(a.tcpPose.xyzwpr) && finite(b.tcpPose.xyzwpr) &&
                a.tcpPose.userFrame == b.tcpPose.userFrame && a.tcpPose.userTool == b.tcpPose.userTool) {
                segment.linear = std::hypot(b.tcpPose.xyzwpr[0] - a.tcpPose.xyzwpr[0],
                    b.tcpPose.xyzwpr[1] - a.tcpPose.xyzwpr[1], b.tcpPose.xyzwpr[2] - a.tcpPose.xyzwpr[2]) / dt;
                const auto qa = quaternion(a.tcpPose.xyzwpr), qb = quaternion(b.tcpPose.xyzwpr);
                // conj(qa)*qb; atan2 retains precision for very small rotations.
                const double w = qa[0]*qb[0] + qa[1]*qb[1] + qa[2]*qb[2] + qa[3]*qb[3];
                const double x = qa[0]*qb[1] - qa[1]*qb[0] - qa[2]*qb[3] + qa[3]*qb[2];
                const double y = qa[0]*qb[2] - qa[2]*qb[0] - qa[3]*qb[1] + qa[1]*qb[3];
                const double z = qa[0]*qb[3] - qa[3]*qb[0] - qa[1]*qb[2] + qa[2]*qb[1];
                segment.angular = 2. * std::atan2(std::hypot(x, y, z), std::abs(w)) * (180. / pi) / dt;
                segment.tcpValid = std::isfinite(segment.linear) && std::isfinite(segment.angular);
            }
            segments.push_back(std::move(segment));
        }
        if(segments.empty()) { result.diagnostic = "No adjacent valid samples in the selected interval"; return result; }
        struct Integral {
            double covered = 0., tcpCovered = 0., linear = 0., angular = 0.;
            std::vector<double> joints;
        };
        const auto accumulate = [](Integral& value, const Segment& segment, double dt) {
            value.covered += dt;
            for(std::size_t j = 0; j < value.joints.size(); ++j) value.joints[j] += segment.joints[j] * dt;
            if(segment.tcpValid) {
                value.tcpCovered += dt; value.linear += segment.linear * dt; value.angular += segment.angular * dt;
            }
        };
        Integral zero; zero.joints.assign(dimensions, 0.);
        std::vector<Integral> prefix{zero}; prefix.reserve(segments.size() + 1);
        for(const auto& segment : segments) {
            auto total = prefix.back(); accumulate(total, segment, segment.end - segment.start);
            prefix.push_back(std::move(total));
        }
        // Prefix path-length integrals include every valid sample segment.
        // Each sliding window is O(log N), rather than rescanning 20000 frames.
        const auto integralAt = [&](double t) {
            const auto it = std::upper_bound(segments.begin(), segments.end(), t,
                [](double time, const Segment& segment) { return time < segment.end; });
            Integral total = prefix[static_cast<std::size_t>(it - segments.begin())];
            if(it != segments.end() && t > it->start) accumulate(total, *it, t - it->start);
            return total;
        };
        result.jointMaxSpeed.assign(dimensions, 0.);
        result.tcpLinearMaxMmPerSecond = result.tcpAngularMaxDegPerSecond = nan;
        const auto append = [&](SpeedPoint point) {
            for(std::size_t j = 0; j < dimensions; ++j)
                result.jointMaxSpeed[j] = std::max(result.jointMaxSpeed[j], point.jointAbsSpeed[j]);
            if(std::isfinite(point.tcpLinearMmPerSecond)) {
                if(!std::isfinite(result.tcpLinearMaxMmPerSecond)) {
                    result.tcpLinearMaxMmPerSecond = result.tcpAngularMaxDegPerSecond = 0.;
                }
                result.tcpLinearMaxMmPerSecond = std::max(result.tcpLinearMaxMmPerSecond, point.tcpLinearMmPerSecond);
                result.tcpAngularMaxDegPerSecond = std::max(result.tcpAngularMaxDegPerSecond, point.tcpAngularDegPerSecond);
            }
            result.points.push_back(std::move(point));
        };
        if(method == SpeedCalculationMethod::InstantaneousDifference) {
            for(const auto& segment : segments) {
                SpeedPoint point; point.elapsedSeconds = segment.end; point.jointAbsSpeed = segment.joints;
                point.tcpLinearMmPerSecond = segment.tcpValid ? segment.linear : nan;
                point.tcpAngularDegPerSecond = segment.tcpValid ? segment.angular : nan;
                append(std::move(point));
            }
        } else {
            for(const auto& sample : samples) {
                const double tolerance = 1.e-9 * std::max(1., minimumIntervalSeconds);
                const double sampleEnd = sample.elapsedNs * 1.e-9;
                if(sampleEnd > endSeconds + tolerance) continue;
                const double windowEnd = std::min(sampleEnd, endSeconds), windowStart = windowEnd - minimumIntervalSeconds;
                if(windowStart < startSeconds - tolerance) continue;
                const auto a = integralAt(std::max(windowStart, startSeconds)), b = integralAt(windowEnd);
                const double covered = b.covered - a.covered, tcpCovered = b.tcpCovered - a.tcpCovered;
                if(covered + tolerance < minimumIntervalSeconds || covered <= 0.) continue;
                SpeedPoint point; point.elapsedSeconds = windowEnd; point.jointAbsSpeed.resize(dimensions);
                for(std::size_t j = 0; j < dimensions; ++j) point.jointAbsSpeed[j] = (b.joints[j] - a.joints[j]) / covered;
                const bool tcpComplete = tcpCovered > 0. && tcpCovered + tolerance >= minimumIntervalSeconds;
                point.tcpLinearMmPerSecond = tcpComplete ? (b.linear - a.linear) / tcpCovered : nan;
                point.tcpAngularDegPerSecond = tcpComplete ? (b.angular - a.angular) / tcpCovered : nan;
                append(std::move(point));
            }
        }
        result.sampleCount = result.points.size();
        if(result.points.empty()) { result.diagnostic = "No complete valid calculation interval"; return result; }
        // Whole-interval means integrate each segment ONCE. Arithmetic averaging
        // sample speeds (or overlapping windows) biases irregular sampling.
        const auto& total = prefix.back();
        result.jointAverageSpeed.resize(dimensions);
        for(std::size_t j = 0; j < dimensions; ++j) result.jointAverageSpeed[j] = total.joints[j] / total.covered;
        result.tcpLinearAverageMmPerSecond = total.tcpCovered > 0. ? total.linear / total.tcpCovered : nan;
        result.tcpAngularAverageDegPerSecond = total.tcpCovered > 0. ? total.angular / total.tcpCovered : nan;
        result.valid = true;
        return result;
    }

    struct Session::Impl {
        BackendFactory factory;
        mutable std::mutex mutex;
        std::condition_variable wake;
        std::thread worker;
        std::atomic<bool> cancel{false};
        Snapshot value;
        std::deque<Sample> history;
        explicit Impl(BackendFactory f) : factory(f) {}
        bool launch(const Options& options, bool scanOnly) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if(value.state == State::Discovering || value.state == State::Connecting || value.state == State::Streaming) return false;
            }
            if(worker.joinable()) worker.join();
            cancel = false;
            {
                std::lock_guard<std::mutex> lock(mutex);
                value = {};
                history.clear();
                value.state = State::Discovering;
                value.group = options.group;
                value.intervalMs = options.intervalMs;
                value.csvPath = options.csvPath;
            }
            worker = std::thread([this, options, scanOnly] { run(options, scanOnly); });
            return true;
        }
        void run(const Options& options, bool scanOnly) {
            std::ofstream output;
            State finalState = State::Stopped;
            std::string diagnostic;
            try {
                if(options.group < 1 || options.intervalMs < 10 || options.intervalMs > 1000)
                    throw std::runtime_error("Invalid group or sampling interval (10..1000 ms)");
                auto backend = factory();
                auto endpoints = backend->discover();
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    value.endpoints = endpoints;
                }
                if(!scanOnly && !cancel) {
                    auto selected = endpoints.end();
                    if(options.endpointId.empty() && endpoints.size() == 1) selected = endpoints.begin();
                    else selected = std::find_if(endpoints.begin(), endpoints.end(), [&](const Endpoint& e) { return e.id == options.endpointId; });
                    if(selected == endpoints.end()) throw std::runtime_error(endpoints.empty()
                        ? "No running ROBOGUIDE virtual controller found. Open a workstation first."
                        : "Multiple running controllers found. Select one and start again.");
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        value.source = selected->id;
                        value.state = State::Connecting;
                    }
                    backend->attach(selected->id, options.group);
                    if(options.csvPath.empty()) throw std::runtime_error("A CSV recording path is required");
                    if(std::filesystem::exists(options.csvPath)) throw std::runtime_error("CSV already exists; choose a new recording file");
                    output.open(options.csvPath, std::ios::out | std::ios::binary);
                    if(!output) throw std::runtime_error("Cannot create CSV recording");
                    output.imbue(std::locale::classic());
                    output << "# source=" << csv(selected->id) << "\n# group=" << options.group
                        << "\n# interval_ms=" << options.intervalMs
                        << "\n# time=host_receive_utc_unix_ns; elapsed=host_steady_ns; latency=read_duration_ns"
                        << "\n# joints=FANUC_controller_native; rotational_axes_deg; external_axes_controller_units\n"
                        << "# tcp=FANUC_WORLD_UF0_active_UT; XYZ_mm; WPR_deg; Rz(R)*Ry(P)*Rx(W)\n";
                    const auto started = Steady::now();
                    auto next = started;
                    auto lastFlush = started;
                    std::size_t dimension = 0;
                    std::uint64_t sequence = 0;
                    while(!cancel) {
                        const auto before = Steady::now();
                        auto reading = backend->read();
                        auto& joints = reading.joints;
                        const auto received = std::chrono::system_clock::now();
                        const auto after = Steady::now();
                        if(joints.empty() || !std::all_of(joints.begin(), joints.end(), [](double q) { return std::isfinite(q); }))
                            throw std::runtime_error("Controller returned invalid joint data");
                        auto& pose = reading.tcpPose;
                        if(pose.valid && (!std::all_of(pose.xyzwpr.begin(), pose.xyzwpr.end(), [](double v) { return std::isfinite(v); })
                            || pose.userFrame != 0 || pose.userTool < 0)) {
                            pose.valid = false;
                            pose.diagnostic = "Invalid WORLD TCP coordinates or frame/tool identity";
                        }
                        if(!pose.valid) {
                            pose.xyzwpr.fill(std::numeric_limits<double>::quiet_NaN());
                            if(pose.diagnostic.empty()) pose.diagnostic = "Cartesian TCP pose unavailable";
                        }
                        if(dimension && dimension != joints.size()) throw std::runtime_error("Controller joint count changed; recording stopped");
                        if(!dimension) {
                            dimension = joints.size();
                            output << "sequence,received_utc_unix_ns,elapsed_ns,read_latency_ns";
                            for(std::size_t i = 0; i < dimension; ++i) output << ",J" << i+1;
                            output << ",tcp_valid,tcp_x_mm,tcp_y_mm,tcp_z_mm,tcp_w_deg,tcp_p_deg,tcp_r_deg,tcp_uf,tcp_ut,tcp_status\n";
                        }
                        Sample sample;
                        sample.sequence = ++sequence;
                        sample.receivedUnixNs = std::chrono::duration_cast<std::chrono::nanoseconds>(received.time_since_epoch()).count();
                        sample.elapsedNs = ns(after - started);
                        sample.readLatencyNs = ns(after - before);
                        sample.joints = std::move(joints);
                        sample.tcpPose = std::move(reading.tcpPose);
                        output << sample.sequence << ',' << sample.receivedUnixNs << ',' << sample.elapsedNs << ',' << sample.readLatencyNs;
                        for(double q : sample.joints) output << ',' << std::setprecision(17) << q;
                        output << ',' << (sample.tcpPose.valid ? 1 : 0);
                        for(double v : sample.tcpPose.xyzwpr) {
                            if(sample.tcpPose.valid) output << ',' << std::setprecision(17) << v;
                            else output << ",nan";
                        }
                        std::string poseStatus = sample.tcpPose.valid ? "OK" : sample.tcpPose.diagnostic;
                        std::replace(poseStatus.begin(), poseStatus.end(), '\n', ' ');
                        std::replace(poseStatus.begin(), poseStatus.end(), '\r', ' ');
                        output << ',' << sample.tcpPose.userFrame << ',' << sample.tcpPose.userTool << ',' << csv(poseStatus) << '\n';
                        if(after - lastFlush >= std::chrono::seconds(1)) { output.flush(); lastFlush = after; }
                        if(!output) throw std::runtime_error("CSV write failed; recording stopped");
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            value.latest = std::move(sample);
                            history.push_back(value.latest);
                            constexpr std::size_t kHistoryLimit = 20000;
                            if(history.size() > kHistoryLimit) history.pop_front();
                            value.state = State::Streaming;
                        }
                        next += std::chrono::milliseconds(options.intervalMs);
                        if(next < after) next = after + std::chrono::milliseconds(options.intervalMs);
                        std::unique_lock<std::mutex> lock(mutex);
                        wake.wait_until(lock, next, [this] { return cancel.load(); });
                    }
                }
                if(scanOnly) diagnostic = endpoints.empty() ? "No running virtual controller found" : "Discovery complete";
            } catch(const std::exception& error) {
                finalState = State::Fault;
                diagnostic = error.what();
            }
            if(output.is_open()) {
                output.flush();
                if(!output) { finalState = State::Fault; diagnostic = "CSV flush failed"; }
                output.close();
            }
            // Terminal state is published only after backend COM objects and output are released.
            std::lock_guard<std::mutex> lock(mutex);
            value.state = finalState;
            value.diagnostic = diagnostic;
        }
    };
    Session::Session(BackendFactory factory) : m_impl(std::make_unique<Impl>(factory)) {}
    Session::~Session() { stop(); if(m_impl->worker.joinable()) m_impl->worker.join(); }
    bool Session::discover() { return m_impl->launch({}, true); }
    bool Session::start(const Options& options) { return m_impl->launch(options, false); }
    void Session::stop() { m_impl->cancel = true; m_impl->wake.notify_all(); }
    Snapshot Session::snapshot() const {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        return m_impl->value;
    }
    SpeedStatistics Session::speedStatistics(double startSeconds, double endSeconds,
        SpeedCalculationMethod method, double minimumIntervalSeconds) const {
        std::vector<Sample> samples;
        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            samples.assign(m_impl->history.begin(), m_impl->history.end());
        }
        return calculateSpeedStatistics(samples, startSeconds, endSeconds, method, minimumIntervalSeconds);
    }
}
