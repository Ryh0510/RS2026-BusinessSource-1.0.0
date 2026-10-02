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
    class IRobotQtViewerMotionPlanningViewportPort;
    class IRobotQtViewerAssemblyViewportPort;
    class IRobotQtViewerCollisionViewportPort;
    class IRobotQtViewerDocumentViewportPort;
    class IRobotQtViewerSelectionViewportPort;
    class IRobotQtViewerVisualizationViewportPort;

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
        void setMotionPlanningViewport(IRobotQtViewerMotionPlanningViewportPort* viewport);
        IRobotQtViewerMotionPlanningViewportPort* motionPlanningViewport() const;
        void setViewportServices(RobotQtViewerViewportServices* viewportServices);
        RobotQtViewerViewportServices* viewportServices() const;
        void setDocumentViewport(IRobotQtViewerDocumentViewportPort* viewport);
        IRobotQtViewerDocumentViewportPort* documentViewport() const;
        void setSelectionViewport(IRobotQtViewerSelectionViewportPort* viewport);
        IRobotQtViewerSelectionViewportPort* selectionViewport() const;
        void setAssemblyViewport(IRobotQtViewerAssemblyViewportPort* viewport);
        IRobotQtViewerAssemblyViewportPort* assemblyViewport() const;
        void setCollisionViewport(IRobotQtViewerCollisionViewportPort* viewport);
        IRobotQtViewerCollisionViewportPort* collisionViewport() const;
        void setVisualizationViewport(IRobotQtViewerVisualizationViewportPort* viewport);
        IRobotQtViewerVisualizationViewportPort* visualizationViewport() const;

    private:
        simulation_project::ProjectSession& m_session;
        RobotQtViewerDocumentController& m_documentController;
        RobotQtViewerSelectionModel& m_selectionModel;
        RobotQtViewerViewportPreviewState& m_viewportPreviewState;
        RobotQtViewerEventHub& m_eventHub;
        RobotQtViewerOperationStatusStore& m_operationStatusStore;
        IRobotQtViewerMotionPlanningViewportPort* m_motionPlanningViewport = nullptr;
        RobotQtViewerViewportServices* m_viewportServices = nullptr;
        IRobotQtViewerDocumentViewportPort* m_documentViewport = nullptr;
        IRobotQtViewerSelectionViewportPort* m_selectionViewport = nullptr;
        IRobotQtViewerAssemblyViewportPort* m_assemblyViewport = nullptr;
        IRobotQtViewerCollisionViewportPort* m_collisionViewport = nullptr;
        IRobotQtViewerVisualizationViewportPort* m_visualizationViewport = nullptr;
    };
}
