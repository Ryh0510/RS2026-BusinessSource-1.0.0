#pragma once

#include <SimulationProject/ProjectSession.h>

namespace simulation_project
{
    struct ProjectDocument;
}

namespace robot_qt_viewer
{
    class RobotQtViewerDocumentController;
    class RobotQtViewerEventHub;
    class RobotQtViewerOperationStatusStore;
    class RobotQtViewerSelectionModel;
    class RobotQtViewerViewportPreviewState;
    class RobotQtViewerViewportServices;

    class RobotQtViewerDocumentContext
    {
    public:
        RobotQtViewerDocumentContext(
            simulation_project::ProjectSession& session,
            RobotQtViewerDocumentController& documentController,
            RobotQtViewerSelectionModel& selectionModel,
            RobotQtViewerViewportPreviewState& viewportPreviewState,
            RobotQtViewerEventHub& eventHub,
            RobotQtViewerOperationStatusStore& operationStatusStore);

        simulation_project::ProjectSession& projectSession();
        const simulation_project::ProjectSession& projectSession() const;
        const simulation_project::ProjectDocument& document() const;
        RobotQtViewerDocumentController& documentController();
        const RobotQtViewerDocumentController& documentController() const;
        RobotQtViewerSelectionModel& selectionModel();
        const RobotQtViewerSelectionModel& selectionModel() const;
        RobotQtViewerViewportPreviewState& viewportPreviewState();
        const RobotQtViewerViewportPreviewState& viewportPreviewState() const;
        RobotQtViewerEventHub& eventHub();
        const RobotQtViewerEventHub& eventHub() const;
        RobotQtViewerOperationStatusStore& operationStatusStore();
        const RobotQtViewerOperationStatusStore& operationStatusStore() const;
        void setViewportServices(RobotQtViewerViewportServices* viewportServices);
        RobotQtViewerViewportServices* viewportServices() const;

    private:
        simulation_project::ProjectSession& m_session;
        RobotQtViewerDocumentController& m_documentController;
        RobotQtViewerSelectionModel& m_selectionModel;
        RobotQtViewerViewportPreviewState& m_viewportPreviewState;
        RobotQtViewerEventHub& m_eventHub;
        RobotQtViewerOperationStatusStore& m_operationStatusStore;
        RobotQtViewerViewportServices* m_viewportServices = nullptr;
    };
}
