#pragma once

#include "CDFAlgorithms/CdfPlanningService.h"
#include "CDFAlgorithms/CdfTypes.h"

namespace cdf
{
    enum class DemoCase
    {
        DistanceField,
        OmplSeed,
        Repair
    };

    class CdfDocument final
    {
    public:
        CdfDocument();

        void loadDemo(DemoCase demoCase);
        void setTargetClearance(double clearance);
        void setSafetyMargin(double margin);
        void setStartJoint(std::size_t index, double value);
        void setGoalJoint(std::size_t index, double value);
        void setObstacle(std::size_t index, const CircularObstacle& obstacle);
        void setOmplSeedPath(Path seedPath);
        void setInitialPathStrategy(InitialPathStrategy strategy);

        PlanningResult runPlanning();
        ClearanceSample evaluateAt(const JointVector& q) const;

        const PlanningRequest& request() const;
        const PlanningResult& result() const;
        DemoCase activeDemo() const;

    private:
        PlanningRequest m_request;
        PlanningResult m_result;
        CdfPlanningService m_service;
        DemoCase m_activeDemo = DemoCase::Repair;
    };
}
