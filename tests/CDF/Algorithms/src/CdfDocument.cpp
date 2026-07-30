#include "CDFAlgorithms/CdfDocument.h"

#include <stdexcept>
#include <utility>

namespace cdf
{
    CdfDocument::CdfDocument()
    {
        loadDemo(DemoCase::Repair);
    }

    void CdfDocument::loadDemo(DemoCase demoCase)
    {
        m_activeDemo = demoCase;
        switch (demoCase)
        {
        case DemoCase::DistanceField:
            m_request = makeDistanceFieldDemoRequest();
            break;
        case DemoCase::OmplSeed:
            m_request = makeOmplSeedDemoRequest();
            break;
        case DemoCase::Repair:
            m_request = makeRepairDemoRequest();
            break;
        }
        m_result = m_service.plan(m_request);
    }

    void CdfDocument::setTargetClearance(double clearance)
    {
        m_request.targetClearance = clearance;
    }

    void CdfDocument::setSafetyMargin(double margin)
    {
        m_request.safetyMargin = margin;
    }

    void CdfDocument::setStartJoint(std::size_t index, double value)
    {
        if (index >= m_request.start.size())
            throw std::out_of_range("Start joint index is out of range.");
        m_request.start[index] = value;
    }

    void CdfDocument::setGoalJoint(std::size_t index, double value)
    {
        if (index >= m_request.goal.size())
            throw std::out_of_range("Goal joint index is out of range.");
        m_request.goal[index] = value;
    }

    void CdfDocument::setObstacle(std::size_t index, const CircularObstacle& obstacle)
    {
        if (index >= m_request.obstacles.size())
            throw std::out_of_range("Obstacle index is out of range.");
        m_request.obstacles[index] = obstacle;
    }

    void CdfDocument::setOmplSeedPath(Path seedPath)
    {
        m_request.omplSeedPath = std::move(seedPath);
        m_request.initialPathStrategy = InitialPathStrategy::OmplSeedPath;
    }

    void CdfDocument::setInitialPathStrategy(InitialPathStrategy strategy)
    {
        m_request.initialPathStrategy = strategy;
    }

    PlanningResult CdfDocument::runPlanning()
    {
        m_result = m_service.plan(m_request);
        return m_result;
    }

    ClearanceSample CdfDocument::evaluateAt(const JointVector& q) const
    {
        return m_service.clearanceAndGradient(m_request, q);
    }

    const PlanningRequest& CdfDocument::request() const
    {
        return m_request;
    }

    const PlanningResult& CdfDocument::result() const
    {
        return m_result;
    }

    DemoCase CdfDocument::activeDemo() const
    {
        return m_activeDemo;
    }
}
