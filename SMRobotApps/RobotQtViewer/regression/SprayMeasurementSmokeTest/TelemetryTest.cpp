#include "Telemetry/ExternalRobotTelemetry.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <thread>
#include <sstream>

using namespace robot_telemetry;
namespace {
    std::atomic<int> mode{0};
    std::atomic<int> destroyed{0};
    std::thread::id mainThread;
    std::atomic<bool> wrongThread{false};
    struct FakeBackend : Backend {
        std::thread::id owner = std::this_thread::get_id();
        int sequence = 0;
        FakeBackend() { if(owner == mainThread) wrongThread = true; }
        ~FakeBackend() { if(owner != std::this_thread::get_id()) wrongThread = true; ++destroyed; }
        std::vector<Endpoint> discover() override {
            if(mode == 1) return {};
            if(mode == 2) return {{"one", "A"}, {"two", "B"}};
            return {{"one", "A"}};
        }
        void attach(const std::string& id, int group) override {
            if((id != "one" && id != "two") || group != 1) throw std::runtime_error("Bad endpoint/group");
        }
        Reading read() override {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            ++sequence;
            if(mode == 3 && sequence > 3) throw std::runtime_error("Disconnected");
            if(mode == 4 && sequence > 3) return {{1.}, {}};
            if(mode == 5) return {{std::numeric_limits<double>::quiet_NaN()}, {}};
            Reading reading;
            reading.joints = {double(sequence), -20., 230., 4., 5., 6., 700.};
            reading.tcpPose.valid = true;
            reading.tcpPose.xyzwpr = {1000. + sequence, -2., 300., -180., 5., -6.};
            reading.tcpPose.userFrame = 0; reading.tcpPose.userTool = 2;
            if(mode == 6) { reading.tcpPose.valid = false; reading.tcpPose.diagnostic = "Unsupported pose"; }
            if(mode == 7) reading.tcpPose.xyzwpr[3] = std::numeric_limits<double>::quiet_NaN();
            if(mode == 8) reading.tcpPose.userFrame = 2;
            return reading;
        }
    };
    std::unique_ptr<Backend> fake() { return std::make_unique<FakeBackend>(); }
    bool terminal(State s) { return s == State::Stopped || s == State::Fault; }
    template<class F> void wait(F predicate, int timeoutSeconds = 10) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
        while(!predicate()) {
            if(std::chrono::steady_clock::now() > end) throw std::runtime_error("Timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    void require(bool value, const char* message) { if(!value) throw std::runtime_error(message); }
    void tests() {
        mainThread = std::this_thread::get_id();
        {
            std::vector<Sample> samples(3);
            for(std::size_t i = 0; i < samples.size(); ++i) {
                samples[i].elapsedNs = static_cast<std::int64_t>(i) * 1000000000LL;
                samples[i].joints = {i == 0 ? 0. : i == 1 ? 10. : 30., i == 0 ? 0. : i == 1 ? 5. : 2.};
                samples[i].tcpPose.valid = true; samples[i].tcpPose.xyzwpr = {i == 0 ? 0. : i == 1 ? 3. : 9., 0., 0., i == 0 ? 0. : i == 1 ? 10. : 25., 0., 0.};
            }
            const auto speeds = calculateSpeedStatistics(samples, 0., 2.);
            require(speeds.valid && speeds.sampleCount == 2 && speeds.tcpLinearMaxMmPerSecond == 6. &&
                std::abs(speeds.tcpAngularMaxDegPerSecond - 15.) < 1.e-10 && speeds.jointMaxSpeed[0] == 20. && speeds.jointMaxSpeed[1] == 5.,
                "Speed statistics use timestamped joint and TCP differences");
            const auto selected = calculateSpeedStatistics(samples, 1., 2.);
            require(selected.valid && selected.sampleCount == 1 && selected.tcpLinearAverageMmPerSecond == 6.,
                "Speed statistics honor the selected time interval");
            const auto averaged = calculateSpeedStatistics(samples, 0., 2., SpeedCalculationMethod::WindowAverage, 1.0);
            require(averaged.valid && averaged.sampleCount == 2 && averaged.jointAverageSpeed[0] == 15. &&
                averaged.tcpLinearAverageMmPerSecond == 4.5 && std::abs(averaged.tcpAngularAverageDegPerSecond - 12.5) < 1.e-10,
                "Window-average speed uses all timestamped segments over the minimum interval");
        }
        {
            const auto sample = [](double time, double joint, const std::array<double, 6>& pose) {
                Sample s; s.elapsedNs = static_cast<std::int64_t>(std::llround(time * 1.e9));
                s.joints = {joint}; s.tcpPose.valid = true; s.tcpPose.userFrame = 0; s.tcpPose.userTool = 1;
                s.tcpPose.xyzwpr = pose; return s;
            };
            const auto close = [](double a, double b) { return std::abs(a - b) < 1.e-8; };
            std::vector<Sample> turn = {sample(0., 0., {0.,0.,0.,0.,0.,0.}),
                sample(.2, 10., {10.,0.,0.,0.,0.,10.}), sample(1., 0., {0.,0.,0.,0.,0.,0.})};
            for(const auto method : {SpeedCalculationMethod::InstantaneousDifference, SpeedCalculationMethod::WindowAverage}) {
                const auto speeds = calculateSpeedStatistics(turn, 0., 1., method, 1.);
                require(speeds.valid && close(speeds.jointAverageSpeed[0], 20.) &&
                    close(speeds.tcpLinearAverageMmPerSecond, 20.) && close(speeds.tcpAngularAverageDegPerSecond, 20.),
                    "Irregular sampling and reversal use total path / covered time, never end-minus-start or sample-count mean");
                if(method == SpeedCalculationMethod::WindowAverage) require(speeds.sampleCount == 1 &&
                    close(speeds.points[0].jointAbsSpeed[0], 20.), "Full window includes every intermediate sample");
            }
            const auto clipped = calculateSpeedStatistics(turn, .1, .9);
            require(clipped.valid && close(clipped.jointAverageSpeed[0], 17.1875),
                "Selection boundaries integrate partial sample segments with actual time weights");
            std::vector<Sample> poses = {sample(0., 725., {0.,0.,0.,0.,90.,0.}),
                sample(1., 1085., {0.,0.,0.,60.,90.,60.})};
            for(const auto method : {SpeedCalculationMethod::InstantaneousDifference, SpeedCalculationMethod::WindowAverage}) {
                const auto speeds = calculateSpeedStatistics(poses, 0., 1., method, 1.);
                require(speeds.valid && close(speeds.tcpAngularMaxDegPerSecond, 0.) &&
                    close(speeds.jointMaxSpeed[0], 360.), "Euler singularity produces zero physical angular speed, joint full turns remain 360 deg/s");
            }
            poses[0].tcpPose.xyzwpr = {0.,0.,0.,0.,0.,0.};
            poses[1].tcpPose.xyzwpr = {0.,0.,0.,90.,90.,0.};
            require(close(calculateSpeedStatistics(poses, 0., 1.).tcpAngularMaxDegPerSecond, 120.),
                "Combined-axis physical rotation uses quaternion geodesic angle (120 degrees)");
            poses[0].tcpPose.xyzwpr = {0.,0.,0.,10.,20.,30.};
            poses[1].tcpPose.xyzwpr = {0.,0.,0.,190.,160.,210.};
            require(close(calculateSpeedStatistics(poses, 0., 1.).tcpAngularMaxDegPerSecond, 0.),
                "Equivalent WPR representations have zero physical angular speed");
            poses[0].tcpPose.xyzwpr = {0.,0.,0.,0.,0.,179.};
            poses[1].tcpPose.xyzwpr = {0.,0.,0.,0.,0.,-179.};
            require(close(calculateSpeedStatistics(poses, 0., 1.).tcpAngularMaxDegPerSecond, 2.),
                "TCP orientation crossing 180 degrees uses shortest physical rotation");
            poses[1].tcpPose.userTool = 2;
            for(const auto method : {SpeedCalculationMethod::InstantaneousDifference, SpeedCalculationMethod::WindowAverage}) {
                const auto speeds = calculateSpeedStatistics(poses, 0., 1., method, 1.);
                require(speeds.valid && std::isnan(speeds.points[0].tcpLinearMmPerSecond) &&
                    std::isnan(speeds.tcpAngularAverageDegPerSecond) && close(speeds.jointMaxSpeed[0], 360.),
                    "UT change creates a TCP gap, never a fake jump or zero; joint speeds remain available");
            }
            poses[1].tcpPose.userTool = 1; poses[1].tcpPose.userFrame = 2;
            require(std::isnan(calculateSpeedStatistics(poses,0.,1.).tcpLinearAverageMmPerSecond),
                "Coordinate-frame changes cannot create physical TCP speeds");
            poses[1].tcpPose.userFrame = 0; poses[1].tcpPose.xyzwpr[0] = std::numeric_limits<double>::quiet_NaN();
            require(std::isnan(calculateSpeedStatistics(poses,0.,1.).points[0].tcpAngularDegPerSecond),
                "Nonfinite TCP data remain explicitly unavailable");
            turn[1].tcpPose.valid = false;
            require(calculateSpeedStatistics(turn,0.,1.).valid &&
                std::isnan(calculateSpeedStatistics(turn,0.,1.,SpeedCalculationMethod::WindowAverage,1.).points[0].tcpLinearMmPerSecond),
                "Missing TCP samples do not dilute speed averages with fabricated zeros");
            turn[2].joints.push_back(0.);
            require(calculateSpeedStatistics(turn,0.,1.).sampleCount == 1, "Changing joint dimension cannot index outside result vectors");
            turn[1].elapsedNs = 0;
            require(!calculateSpeedStatistics(turn,0.,1.).valid, "Nonmonotonic timestamps are explicitly rejected");
            std::vector<Sample> longRun;
            for(int i = 0; i < 20000; ++i) longRun.push_back(sample(i * .001, i * .002, {i * .003,0.,0.,0.,0.,0.}));
            const auto manyWindows = calculateSpeedStatistics(longRun, 0., 19.999,
                SpeedCalculationMethod::WindowAverage, .5);
            require(manyWindows.valid && manyWindows.sampleCount == 19500 &&
                close(manyWindows.points.back().jointAbsSpeed[0], 2.) &&
                close(manyWindows.points.back().tcpLinearMmPerSecond, 3.),
                "20000-frame sliding windows preserve all samples and accurate fractional-time integrals");
            std::vector<Sample> native = {sample(0.,0.,{}), sample(1.,0.,{})};
            native[0].joints = {0., 20., 30.}; native[1].joints = {0., 30., 10.};
            const auto encoderSpeeds = calculateSpeedStatistics(native,0.,1.);
            require(close(encoderSpeeds.jointMaxSpeed[1],10.) && close(encoderSpeeds.jointMaxSpeed[2],20.),
                "J2/J3 speeds use independent raw controller encoders, not the coupled scene J3");
        }
        const auto folder = std::filesystem::temp_directory_path() / ("rs_telemetry_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder);
        Session session(fake);
        require(session.discover(), "Start discovery");
        wait([&] { return terminal(session.snapshot().state); });
        require(session.snapshot().endpoints.size() == 1 && session.snapshot().latest.sequence == 0, "Discovery does not fabricate data");
        Options options; options.csvPath = folder / "record.csv"; options.intervalMs = 10;
        require(session.start(options), "Start auto attach");
        require(!session.start(options) && !session.discover(), "Reject overlapping sessions");
        wait([&] { return session.snapshot().latest.sequence >= 5; });
        const auto first = session.snapshot();
        wait([&] { return session.snapshot().latest.sequence > first.latest.sequence; });
        const auto last = session.snapshot();
        require(last.source == "one" && last.latest.joints.size() == 7 && last.latest.joints[2] == 230., "Preserve dynamic dimensions and multi-turn values");
        require(last.latest.tcpPose.valid && last.latest.tcpPose.userFrame == 0 && last.latest.tcpPose.userTool == 2 &&
            last.latest.tcpPose.xyzwpr[0] == 1000. + last.latest.joints[0] && last.latest.tcpPose.xyzwpr[3] == -180.,
            "Pose values/frame/tool retained with corresponding joint reading");
        require(last.latest.elapsedNs > first.latest.elapsedNs && last.latest.receivedUnixNs > 1700000000000000000LL && last.latest.readLatencyNs >= 1000000, "Real timestamp/latency semantics");
        session.stop(); wait([&] { return terminal(session.snapshot().state); });
        const auto stopped = session.snapshot().latest.sequence;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        require(session.snapshot().latest.sequence == stopped, "Stop never emits stale new samples");
        std::ifstream file(options.csvPath); std::string line; int records = 0;
        std::uint64_t previousSequence = 0; std::int64_t previousElapsed = -1;
        bool header = false;
        while(std::getline(file,line)) {
            if(line.find("sequence,received_utc_unix_ns") == 0) { header = line.find("J7") != std::string::npos; continue; }
            if(line.empty() || line[0] == '#') continue;
            auto a=line.find(','), b=line.find(',',a+1), c=line.find(',',b+1);
            const auto sequence=std::stoull(line.substr(0,a));
            const auto elapsed=std::stoll(line.substr(b+1,c-b-1));
            require(sequence == previousSequence + 1 && elapsed > previousElapsed, "CSV order and monotonic elapsed");
            std::istringstream columns(line); std::vector<std::string> fields; std::string field;
            while(std::getline(columns, field, ',')) fields.push_back(field);
            require(fields.size() == 21 && fields[11] == "1" && fields[18] == "0" && fields[19] == "2" &&
                std::stod(fields[12]) == 1000. + std::stod(fields[4]), "CSV appends accurate TCP values and UF/UT to joint columns");
            previousSequence=sequence; previousElapsed=elapsed; ++records;
        }
        require(header && records == int(stopped), "All collected rows persisted");
        require(session.start(options), "Start overwrite rejection"); wait([&] { return terminal(session.snapshot().state); });
        require(session.snapshot().state == State::Fault && session.snapshot().latest.sequence == 0, "Never overwrite existing recordings");
        for(int scenario=1; scenario<=5; ++scenario) {
            mode=scenario; options.csvPath=folder/(std::to_string(scenario)+".csv"); options.endpointId.clear();
            require(session.start(options), "Start scenario"); wait([&] { return terminal(session.snapshot().state); });
            require(session.snapshot().state == State::Fault, "Failures must be explicit");
            require(session.snapshot().latest.sequence == ((scenario==3 || scenario==4) ? 3 : 0), "Invalid/disconnected reads never create samples");
        }
        mode=2; options.endpointId="two"; options.csvPath=folder/"selected.csv";
        require(session.start(options), "Select one of multiple controllers");
        wait([&] { return session.snapshot().latest.sequence >= 2; });
        require(session.snapshot().source == "two", "Explicit endpoint selection");
        session.stop(); wait([&] { return terminal(session.snapshot().state); });
        for(int scenario = 6; scenario <= 8; ++scenario) {
            mode = scenario; options.endpointId.clear(); options.csvPath = folder / (std::to_string(scenario) + ".csv");
            require(session.start(options), "Start unavailable/invalid Cartesian scenario");
            wait([&] { return session.snapshot().latest.sequence >= 2 || terminal(session.snapshot().state); });
            const auto snapshot = session.snapshot();
            require(snapshot.state == State::Streaming && !snapshot.latest.tcpPose.valid &&
                std::isnan(snapshot.latest.tcpPose.xyzwpr[0]) && !snapshot.latest.tcpPose.diagnostic.empty(),
                "Bad/missing Cartesian pose stays invalid while valid joint telemetry continues");
            session.stop(); wait([&] { return terminal(session.snapshot().state); });
            std::ifstream invalid(options.csvPath); std::string row; bool foundInvalid = false;
            while(std::getline(invalid, row)) if(row.find(",0,nan,nan,nan,nan,nan,nan,") != std::string::npos) foundInvalid = true;
            require(foundInvalid, "Unavailable Cartesian CSV uses explicit invalid flag and nan, never old coordinates");
        }
        require(!wrongThread && destroyed >= 9, "Backend lifecycle stays on worker thread");
        std::cout << "Telemetry tests passed; CSV evidence: " << folder.u8string() << '\n';
    }
}
int main(int argc, char** argv) {
    try {
        if(argc >= 3 && std::string(argv[1]) == "--live") {
            Session session;
            Options options; options.csvPath = std::filesystem::u8path(argv[2]);
            if(argc > 3) options.endpointId = argv[3];
            session.start(options);
            wait([&] { const auto s=session.snapshot(); return s.latest.sequence >= 150 || terminal(s.state); }, 30);
            session.stop(); wait([&] { return terminal(session.snapshot().state); });
            const auto s=session.snapshot();
            std::cout << "source=" << s.source << "\nsamples=" << s.latest.sequence << "\nstate=" << int(s.state)
                << "\ndiagnostic=" << s.diagnostic << "\nendpoints=" << s.endpoints.size() << '\n';
            for(double q:s.latest.joints) std::cout << q << ',';
            std::cout << "\nTCP valid=" << s.latest.tcpPose.valid << " UF=" << s.latest.tcpPose.userFrame << " UT=" << s.latest.tcpPose.userTool << '\n';
            for(double v : s.latest.tcpPose.xyzwpr) std::cout << v << ',';
            std::cout << "\nTCP diagnostic=" << s.latest.tcpPose.diagnostic << '\n';
            std::cout << std::setprecision(17);
            for(const auto method : {SpeedCalculationMethod::InstantaneousDifference, SpeedCalculationMethod::WindowAverage}) {
                const auto speeds = session.speedStatistics(0., s.latest.elapsedNs * 1.e-9, method, .25);
                std::cout << "speed_method=" << int(method) << " valid=" << speeds.valid << " points=" << speeds.sampleCount
                    << " TCP_linear_mean_mm_s=" << speeds.tcpLinearAverageMmPerSecond
                    << " TCP_angular_mean_deg_s=" << speeds.tcpAngularAverageDegPerSecond << '\n';
                for(std::size_t j = 0; j < speeds.jointAverageSpeed.size(); ++j)
                    std::cout << "J" << j+1 << " mean_deg_s=" << speeds.jointAverageSpeed[j]
                        << " max_deg_s=" << speeds.jointMaxSpeed[j] << '\n';
                if(!speeds.valid) throw std::runtime_error("Live speed statistics unavailable: " + speeds.diagnostic);
            }
            return s.state == State::Fault || s.latest.sequence < 150 || !s.latest.tcpPose.valid ? 1 : 0;
        }
        tests(); return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
