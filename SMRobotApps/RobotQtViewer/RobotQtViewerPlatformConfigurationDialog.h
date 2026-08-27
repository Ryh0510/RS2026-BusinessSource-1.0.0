#pragma once

#include "RobotQtViewerPlatformProfile.h"

#include <QDialog>
#include <QVector>

#include <filesystem>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QTreeWidget;

namespace robot_qt_viewer
{
    class RobotQtViewerPlatformConfigurationDialog final : public QDialog
    {
    public:
        RobotQtViewerPlatformConfigurationDialog(
            const RobotQtViewerWorkbenchPackageRegistry& catalog,
            const std::filesystem::path& profilesDirectory,
            const std::filesystem::path& overlaysDirectory,
            const QString& currentProfileId,
            QWidget* parent = nullptr);

        QString selectedProfileId() const;

    private:
        struct AvailableProfile
        {
            std::filesystem::path path;
            RobotQtViewerPlatformProfile profile;
        };

        void loadAvailableProfiles(const QString& currentProfileId);
        void refreshModeItems();
        void refreshDependencyState();
        void applySelection();
        std::filesystem::path overlayPath(const QString& profileId) const;
        const AvailableProfile* selectedProfile() const;

        const RobotQtViewerWorkbenchPackageRegistry& m_catalog;
        std::filesystem::path m_profilesDirectory;
        std::filesystem::path m_overlaysDirectory;
        QVector<AvailableProfile> m_profiles;
        QComboBox* m_profileCombo = nullptr;
        QLabel* m_profileSummary = nullptr;
        QTreeWidget* m_modeTree = nullptr;
        QDialogButtonBox* m_buttons = nullptr;
        bool m_updatingModeItems = false;
    };
}
