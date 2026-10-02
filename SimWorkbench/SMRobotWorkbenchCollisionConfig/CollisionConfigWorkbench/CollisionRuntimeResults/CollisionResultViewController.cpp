#include "CollisionResultViewController.h"

#include "CollisionResultDocumentFacade.h"
#include "CollisionResultsWidget.h"
#include "CollisionWorkbenchServices.h"
#include "RobotQtViewerDocumentContext.h"
#include "RobotQtViewerViewportPorts.h"

#include <vector>

namespace robot_qt_viewer
{
    CollisionResultViewController::CollisionResultViewController(
        CollisionResultsWidget& widget,
        RobotQtViewerDocumentContext& context,
        CollisionWorkbenchServices& appServices,
        CollisionResultDocumentFacade& documentFacade)
        : m_widget(widget)
        , m_context(context)
        , m_appServices(appServices)
        , m_documentFacade(documentFacade)
    {
    }

    void CollisionResultViewController::refresh(const QString& detectorId)
    {
        std::vector<CollisionRuntimeDetectorInfo> runtimeDetectors;
        if(m_context.collisionViewport() != nullptr) {
            runtimeDetectors = m_context.collisionViewport()->collisionRuntimeDetectors();
        }

        const CollisionResultsViewModel viewModel = m_documentFacade.buildViewModel(
            detectorId,
            m_context.collisionViewport() != nullptr,
            runtimeDetectors,
            m_appServices.collisionPairRobotA(),
            m_appServices.collisionPairLinkA());
        m_widget.setResults(viewModel);
    }
}
