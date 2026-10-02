#pragma once

#include <QString>
#include <QStringList>

#include <SimulationProject/ProjectDocument.h>

#include <filesystem>
#include <string>

class QWidget;

namespace robot_qt_viewer
{
    class CollisionConfigWorkbenchShellPort
    {
    public:
        virtual ~CollisionConfigWorkbenchShellPort() = default;

        virtual bool isUpdating() const = 0;
        virtual bool isCollisionModelConfigurationActive() const = 0;
        virtual bool canHandoffCollisionModelConfiguration() const = 0;
        virtual void cancelCollisionModelConfigurationForHandoff(const QString& sourceId) = 0;
        virtual void showCollisionModelConfiguration() = 0;
        virtual void generateCollisionCoacd() = 0;
        virtual void setSelectedCollisionVariantCurrent() = 0;
        virtual QString currentCollisionSelectionSetId() const = 0;
        virtual QString currentCollisionDetectorId() const = 0;
        virtual void refreshInspector(const QString& qualityMessage) = 0;
        virtual void refreshCollisionDetectorList() = 0;
        virtual void refreshCollisionSelectionSetList() = 0;
        virtual void refreshCollisionSelectionSetMemberList() = 0;
        virtual void refreshCollisionDetectorPropertyEditors() = 0;
        virtual void refreshCollisionDetectorDetails() = 0;
        virtual void refreshCollisionElementList(const QString& qualityMessage) = 0;
        virtual void refreshCollisionModelSummary(const QString& qualityMessage) = 0;
        virtual void saveOverridesToProject() = 0;
        virtual void requestSaveOverridesToProject() = 0;
        virtual void requestSaveOverridesAsSidecar(QWidget* parentWidget) = 0;
        virtual void requestExportRobotUrdfWithCollision(QWidget* parentWidget) = 0;
        virtual bool selectedRobotHasCollisionOverrides() const = 0;
        virtual std::filesystem::path selectedRobotSourcePath() const = 0;
        virtual void saveOverridesAsSidecar(
            const std::filesystem::path& sidecarPath,
            const std::string& portableSidecarPath) = 0;
        virtual void exportRobotUrdfWithCollision(
            const std::filesystem::path& sourceUrdfPath,
            const std::filesystem::path& outputUrdfPath) = 0;
        virtual void addMemberToDetectorDraftSet(
            const QString& side,
            const QString& displayName,
            const QStringList& robotLinks,
            const simulation_project::CollisionSelectionSetMemberDesc& member) = 0;
    };
}
