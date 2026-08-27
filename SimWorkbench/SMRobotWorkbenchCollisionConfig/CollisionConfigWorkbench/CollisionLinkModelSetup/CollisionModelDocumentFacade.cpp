#include "CollisionModelDocumentFacade.h"

#include "CollisionLinkModelsController.h"

#include <SimulationProject/ProjectDocument.h>

namespace robot_qt_viewer
{
    CollisionModelDocumentFacade::CollisionModelDocumentFacade(
        const simulation_project::ProjectDocument& document)
        : m_document(document)
    {
    }

    CollisionLinkModelsViewModel CollisionModelDocumentFacade::buildViewModel(
        const QString& selectedRobotId,
        const QString& selectedLinkName,
        const QString& selectedObjectId,
        const QString& selectedAttachmentId,
        const QString& activeDetectorId,
        const QString& previousVariantId,
        const QString& visibleVariantId,
        const QString& qualityMessage,
        const std::filesystem::path& projectBasePath,
        const CollisionRuntimeRobotSummary* robotSummary,
        const std::vector<CollisionRuntimeDetectorInfo>& runtimeDetectors) const
    {
        return CollisionLinkModelsController::buildViewModel(
            m_document,
            selectedRobotId,
            selectedLinkName,
            selectedObjectId,
            selectedAttachmentId,
            activeDetectorId,
            previousVariantId,
            visibleVariantId,
            qualityMessage,
            projectBasePath,
            robotSummary,
            runtimeDetectors);
    }

    CollisionLinkModelsSummaryView CollisionModelDocumentFacade::buildSummary(
        const QString& selectedRobotId,
        const QString& selectedLinkName,
        const QString& selectedObjectId,
        const QString& selectedAttachmentId,
        const QString& activeDetectorId,
        const QString& qualityMessage,
        const CollisionRuntimeRobotSummary* robotSummary,
        const std::vector<CollisionRuntimeDetectorInfo>& runtimeDetectors) const
    {
        return CollisionLinkModelsController::buildSummary(
            m_document,
            selectedRobotId,
            selectedLinkName,
            selectedObjectId,
            selectedAttachmentId,
            activeDetectorId,
            qualityMessage,
            robotSummary,
            runtimeDetectors);
    }
}
