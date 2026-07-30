#pragma once

#include "CDFAlgorithms/CdfCollisionOracle.h"
#include "CDFAlgorithms/CdfTypes.h"

#include <vector>

namespace cdf
{
    class CdfPlanningService
    {
    public:
        PlanningResult plan(const PlanningRequest& request) const;
        PlanningResult plan(const PlanningRequest& request, const ICdfDistanceOracle& oracle) const;

        ClearanceSample clearanceAndGradient(
            const PlanningRequest& request,
            const JointVector& q) const;
        ClearanceSample clearanceAndGradient(
            const ICdfDistanceOracle& oracle,
            double finiteDifferenceStep,
            const JointVector& q) const;

        Path makeInitialPath(
            const PlanningRequest& request,
            const ICdfDistanceOracle& oracle) const;
        Path repairPath(
            const PlanningRequest& request,
            const ICdfDistanceOracle& oracle,
            const Path& seedPath,
            std::vector<CdfRepairIteration>* trace = nullptr) const;

        bool isPathCollisionFree(
            const PlanningRequest& request,
            const ICdfDistanceOracle& oracle,
            const Path& path) const;
        std::vector<double> sampleClearances(
            const PlanningRequest& request,
            const ICdfDistanceOracle& oracle,
            const Path& path) const;

    private:
        Path makeLinearPath(const PlanningRequest& request) const;
        Path makeSampledTreePath(const PlanningRequest& request, const ICdfDistanceOracle& oracle) const;
    };

    PlanningRequest makeDistanceFieldDemoRequest();
    PlanningRequest makeOmplSeedDemoRequest();
    PlanningRequest makeRepairDemoRequest();
}
