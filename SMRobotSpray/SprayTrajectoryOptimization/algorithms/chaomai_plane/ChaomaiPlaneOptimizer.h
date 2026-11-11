#pragma once
// Private domain implementation. SI geometry, seconds, micrometers in fields.
// No Qt, interpreter, subprocess, or desktop-demo dependency.
#include <array>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace chaomai_plane {
using Json = nlohmann::json;
using Vec = std::array<double, 3>;
using Triangle = std::array<Vec, 3>;
struct Surface { std::vector<Triangle> triangles; std::vector<Vec> normals; };
struct Plan {
    std::vector<Vec> p;
    std::vector<std::array<double, 4>> q;
    std::vector<double> time;
    std::vector<int> on, command, row;
    std::vector<std::string> phase;
    Json strokes = Json::array();
    bool inferredSprayFlags = false;
};
struct Cell { Vec a, b; };
struct Profile {
    std::vector<Vec> points, normals;
    std::vector<Cell> cells;
    Vec low{}, high{};
};
struct Grid {
    // Millimeters at the native prediction boundary.
    std::vector<Vec> positions, normals;
    std::vector<double> areas;
    std::vector<unsigned char> copper;
    std::vector<double> heights;
    std::vector<Cell> cells;
};
using Parameters = std::array<double, 20>;
using Progress = std::function<void(double, const std::string&)>;
using Evaluator = std::function<std::vector<double>(const Grid&, const Plan&, const Parameters&, double, double, double)>;
struct Context {
    const std::atomic_bool& cancelled;
    Progress progress;
    void check() const;
    void report(double value, const std::string& message) const;
};
std::array<double, 4> inverseKinematics(Vec p, double yaw);
void identifyStrokes(Plan& plan);
void validatePlan(Plan& plan, bool sourceSlots = true);
Plan planFromJson(const Json& json, bool sourceSlots = true);
Json planToJson(const Plan& plan);
Profile sampleProfile(const Surface& surface, const Plan& original, double pitch, const Context& context);
std::vector<double> heightLevels(bool dense);
Grid makeGrid(const Profile& profile, const std::vector<double>& heights, bool copperOnly = false);
Plan buildPlan(const Plan& original, const Profile& profile, const Json& solution, const Context& context);
std::vector<double> predict(const Surface& surface, const Grid& grid, const Plan& plan,
    const Parameters& parameters, double step, const Context& context, double base = 0, double span = 0);
Json metrics(const std::vector<double>& values, const Grid& grid, double target, double tolerance);
Json refit(const Surface& surface, const Plan& original, const Profile& profile, Json solution,
    const Parameters& parameters, double step, double target, double tolerance,
    const Context& context, const Evaluator& evaluate);
const char* initialSolution();
const char* implementationSignature();
// Meaningful numerical regression of the bounded solver, used by native tests.
bool boundedSolverRegression();
}
