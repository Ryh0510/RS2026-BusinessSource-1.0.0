#include "RobotQtViewerDocumentContext.h"

#include "RobotQtViewerDocumentController.h"
#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerOperationStatus.h"
#include "RobotQtViewerSelectionModel.h"
#include "RobotQtViewerViewportPreviewState.h"

namespace robot_qt_viewer
{
    RobotQtViewerDocumentContext::RobotQtViewerDocumentContext(
        simulation_project::ProjectSession& session,
        RobotQtViewerDocumentController& documentController,
        RobotQtViewerSelectionModel& selectionModel,
        RobotQtViewerViewportPreviewState& viewportPreviewState,
        RobotQtViewerEventHub& eventHub,
        RobotQtViewerOperationStatusStore& operationStatusStore)
        : m_session(session)
        , m_documentController(documentController)
        , m_selectionModel(selectionModel)
        , m_viewportPreviewState(viewportPreviewState)
        , m_eventHub(eventHub)
        , m_operationStatusStore(operationStatusStore)
    {
    }

    simulation_project::ProjectSession& RobotQtViewerDocumentContext::projectSession()
    {
        return m_session;
    }

    const simulation_project::ProjectSession& RobotQtViewerDocumentContext::projectSession() const
    {
        return m_session;
    }

    const simulation_project::ProjectDocument& RobotQtViewerDocumentContext::document() const
    {
        return m_session.document();
    }

    RobotQtViewerDocumentController& RobotQtViewerDocumentContext::documentController()
    {
        return m_documentController;
    }

    const RobotQtViewerDocumentController& RobotQtViewerDocumentContext::documentController() const
    {
        return m_documentController;
    }

    RobotQtViewerSelectionModel& RobotQtViewerDocumentContext::selectionModel()
    {
        return m_selectionModel;
    }

    const RobotQtViewerSelectionModel& RobotQtViewerDocumentContext::selectionModel() const
    {
        return m_selectionModel;
    }

    RobotQtViewerViewportPreviewState& RobotQtViewerDocumentContext::viewportPreviewState()
    {
        return m_viewportPreviewState;
    }

    const RobotQtViewerViewportPreviewState& RobotQtViewerDocumentContext::viewportPreviewState() const
    {
        return m_viewportPreviewState;
    }

    RobotQtViewerEventHub& RobotQtViewerDocumentContext::eventHub()
    {
        return m_eventHub;
    }

    const RobotQtViewerEventHub& RobotQtViewerDocumentContext::eventHub() const
    {
        return m_eventHub;
    }

    RobotQtViewerOperationStatusStore& RobotQtViewerDocumentContext::operationStatusStore()
    {
        return m_operationStatusStore;
    }

    const RobotQtViewerOperationStatusStore& RobotQtViewerDocumentContext::operationStatusStore() const
    {
        return m_operationStatusStore;
    }

    void RobotQtViewerDocumentContext::setViewportServices(RobotQtViewerViewportServices* viewportServices)
    {
        m_viewportServices = viewportServices;
    }

    RobotQtViewerViewportServices* RobotQtViewerDocumentContext::viewportServices() const
    {
        return m_viewportServices;
    }

    void RobotQtViewerDocumentContext::setDocumentViewport(
        IRobotQtViewerDocumentViewportPort* viewport)
    {
        m_documentViewport = viewport;
    }

    IRobotQtViewerDocumentViewportPort* RobotQtViewerDocumentContext::documentViewport() const
    {
        return m_documentViewport;
    }

    void RobotQtViewerDocumentContext::setSelectionViewport(
        IRobotQtViewerSelectionViewportPort* viewport)
    {
        m_selectionViewport = viewport;
    }

    IRobotQtViewerSelectionViewportPort* RobotQtViewerDocumentContext::selectionViewport() const
    {
        return m_selectionViewport;
    }

    void RobotQtViewerDocumentContext::setAssemblyViewport(
        IRobotQtViewerAssemblyViewportPort* viewport)
    {
        m_assemblyViewport = viewport;
    }

    IRobotQtViewerAssemblyViewportPort* RobotQtViewerDocumentContext::assemblyViewport() const
    {
        return m_assemblyViewport;
    }

    void RobotQtViewerDocumentContext::setCollisionViewport(
        IRobotQtViewerCollisionViewportPort* viewport)
    {
        m_collisionViewport = viewport;
    }

    IRobotQtViewerCollisionViewportPort* RobotQtViewerDocumentContext::collisionViewport() const
    {
        return m_collisionViewport;
    }

    void RobotQtViewerDocumentContext::setVisualizationViewport(
        IRobotQtViewerVisualizationViewportPort* viewport)
    {
        m_visualizationViewport = viewport;
    }

    IRobotQtViewerVisualizationViewportPort* RobotQtViewerDocumentContext::visualizationViewport() const
    {
        return m_visualizationViewport;
    }
    void RobotQtViewerDocumentContext::setMotionPlanningViewport(IRobotQtViewerMotionPlanningViewportPort* viewport)
    {
        m_motionPlanningViewport = viewport;
    }

    IRobotQtViewerMotionPlanningViewportPort* RobotQtViewerDocumentContext::motionPlanningViewport() const
    {
        return m_motionPlanningViewport;
    }

}
