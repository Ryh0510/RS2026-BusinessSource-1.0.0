#pragma once
#include <SlicingTrajPlan/BurTrajPlan_global.h>
#include <SectionSweepTrajPlan/SingleSideScanPlanner.h>
#include <array>
#include <atomic>
#include <memory>

namespace blade_region {
enum class Region { Body, LowerBlend, UpperBlend, LowerInner, UpperInner, Excluded, Review, Count };
enum class Method { Slicing, ReferenceOffsets, Disabled };
struct Parameters {
    int spanAxis = 2;
    double endFraction = .22;
    double bodyCosine = .22;
    double platformCosine = .78;
    double sliceSpacing = .008;
    double passSpacing = .008;
    double sampleSpacing = .002;
    double firstOffset = -.012;
    double lastOffset = .012;
    double sprayDistance = .080;
    double spraySpeed = .022;
    double transferSpeed = .12;
    double blendLength = .012;
    std::array<Method, 5> methods{{Method::Slicing, Method::ReferenceOffsets,
        Method::ReferenceOffsets, Method::Slicing, Method::Slicing}};
};
struct Curve {
    Region region = Region::Body;
    std::vector<Eigen::Vector3d> points;
    std::vector<Eigen::Vector3d> normals;
    bool closed = false;
};
struct ReferenceBand {
    Region region = Region::LowerBlend;
    std::vector<Curve> bodyBoundary, platformBoundary, reference;
};
struct Report {
    bool success = false;
    std::string message;
};
// The session owns geometry, semantic labels and every downstream stage.
// Geometry, offsets, distances, speeds and output poses are SI throughout.
class BurTrajPlan_SHAREDLIB Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Report identify(const std::vector<section_sweep::Triangle>& mesh, const Parameters&,
        const std::atomic_bool* cancel = nullptr);
    Report fitReferences(const std::atomic_bool* cancel = nullptr);
    Report generate(Region, const std::atomic_bool* cancel = nullptr);
    Report connect(const std::atomic_bool* cancel = nullptr);
    void setParameters(const Parameters&);
    void clear();
    void clearPaths();
    void clearConnection();
    bool identified() const;
    bool referencesReady() const;
    const Parameters& parameters() const;
    const std::vector<section_sweep::Triangle>& mesh() const;
    const std::vector<Region>& labels() const;
    const std::array<double, 7>& regionAreas() const;
    const std::vector<ReferenceBand>& bands() const;
    const std::vector<Curve>& slices() const;
    const std::vector<Curve>& paths() const;
    const section_sweep::Result& trajectory() const;
    const std::vector<int>& trajectoryRegions() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
