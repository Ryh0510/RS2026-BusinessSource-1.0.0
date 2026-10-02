#pragma once

#include "RobotQtViewerEvents.h"

#include <QObject>
#include <QString>

#include <functional>

namespace robot_qt_viewer
{
    class IRobotQtViewerAssemblyViewportPort;
    class IRobotQtViewerCollisionViewportPort;
    class IRobotQtViewerSelectionViewportPort;
    class RobotQtViewerViewportPreviewState;

    class RobotQtViewerViewportEventController : public QObject
    {
    public:
        using LinkFrameVisibleQuery = std::function<bool(const QString&, const QString&)>;

        RobotQtViewerViewportEventController(
            IRobotQtViewerSelectionViewportPort& selectionViewport,
            IRobotQtViewerAssemblyViewportPort& assemblyViewport,
            IRobotQtViewerCollisionViewportPort& collisionViewport,
            const RobotQtViewerViewportPreviewState& viewportPreviewState,
            LinkFrameVisibleQuery linkFrameVisible,
            QObject* parent = nullptr);

        void handleEvent(const RobotQtViewerEvent& event);

    private:
        void applySelection(const RobotQtViewerSelectionPayload& selection);
        void applyViewportPreview(const RobotQtViewerViewportPreviewPayload& preview);

        IRobotQtViewerSelectionViewportPort& m_selectionViewport;
        IRobotQtViewerAssemblyViewportPort& m_assemblyViewport;
        IRobotQtViewerCollisionViewportPort& m_collisionViewport;
        const RobotQtViewerViewportPreviewState& m_viewportPreviewState;
        LinkFrameVisibleQuery m_linkFrameVisible;
    };
}
