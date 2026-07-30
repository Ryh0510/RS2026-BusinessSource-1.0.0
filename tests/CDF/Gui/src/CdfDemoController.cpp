#include "CdfDemoController.h"

namespace cdf_gui
{
    namespace
    {
        void applyEndpointJoints(cdf::CdfDocument& document, const QVector<double>& startJoints, const QVector<double>& goalJoints)
        {
            const std::size_t jointCount = document.request().start.size();
            for (std::size_t i = 0; i < jointCount; ++i)
            {
                if (static_cast<int>(i) < startJoints.size())
                    document.setStartJoint(i, startJoints[static_cast<int>(i)]);
                if (static_cast<int>(i) < goalJoints.size())
                    document.setGoalJoint(i, goalJoints[static_cast<int>(i)]);
            }
        }
    }

    CdfDemoController::CdfDemoController(QObject* parent)
        : QObject(parent)
    {
    }

    CdfDemoViewModel CdfDemoController::currentViewModel() const
    {
        return m_builder.build(m_document);
    }

    void CdfDemoController::planFromEndpoints(const QVector<double>& startJoints, const QVector<double>& goalJoints)
    {
        const double clearance = m_document.request().targetClearance;
        m_document.loadDemo(cdf::DemoCase::Repair);
        m_document.setTargetClearance(clearance);
        applyEndpointJoints(m_document, startJoints, goalJoints);
        m_document.setInitialPathStrategy(cdf::InitialPathStrategy::OmplStyleSampledTree);
        m_document.runPlanning();
        publish();
    }

    void CdfDemoController::showDistanceFieldCase()
    {
        m_document.loadDemo(cdf::DemoCase::DistanceField);
        publish();
    }

    void CdfDemoController::showOmplSeedCase()
    {
        m_document.loadDemo(cdf::DemoCase::OmplSeed);
        publish();
    }

    void CdfDemoController::showRepairCase()
    {
        m_document.loadDemo(cdf::DemoCase::Repair);
        publish();
    }

    void CdfDemoController::setTargetClearance(double clearance)
    {
        m_document.setTargetClearance(clearance);
        m_document.runPlanning();
        publish();
    }

    void CdfDemoController::rerun()
    {
        m_document.runPlanning();
        publish();
    }

    void CdfDemoController::publish()
    {
        const CdfDemoViewModel model = currentViewModel();
        emit viewModelChanged(model);
        emit statusMessageRequested(model.statusText, 3500);
    }
}
