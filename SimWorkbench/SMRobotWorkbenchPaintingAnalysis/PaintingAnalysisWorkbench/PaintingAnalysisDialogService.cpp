#include "PaintingAnalysisDialogService.h"

#include <RobotQtViewerFileDialog.h>

namespace robot_qt_viewer
{
    QString PaintingAnalysisDialogService::selectModelFile(QWidget* parent)
    {
        return getOpenFileName(
            QStringLiteral("paintingAnalysis.model.open"),
            parent,
            QStringLiteral("Open Coating Analysis Model"),
            QString(),
            QStringLiteral("Mesh Models (*.stl *.obj *.dae *.ply);;All Files (*.*)"));
    }
}
