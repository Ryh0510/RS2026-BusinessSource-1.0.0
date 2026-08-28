#pragma once

#include "RobotQtViewerProductProfile.h"

#include <QDialog>
#include <QHash>
#include <QVector>

#include <filesystem>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QShowEvent;
class QToolButton;
class QTreeWidget;

namespace robot_qt_viewer
{
    class RobotQtViewerProductProfileConfigurationDialog final : public QDialog
    {
    public:
        RobotQtViewerProductProfileConfigurationDialog(
            const RobotQtViewerWorkbenchPackageRegistry& catalog,
            const std::filesystem::path& profilesDirectory,
            const QString& currentProfileId,
            QWidget* parent = nullptr);

        QString selectedProfileId() const;
        bool configurationChanged() const;

    protected:
        void showEvent(QShowEvent* event) override;

    private:
        struct AvailableProfile
        {
            std::filesystem::path path;
            RobotQtViewerProductProfile profile;
            bool builtInFallback = false;
            bool newProfile = false;
            bool dirty = false;
        };

        void loadAvailableProfiles(const QString& currentProfileId);
        void captureEditedProfile();
        void refreshEditor();
        void refreshWorkbenchItems();
        void refreshDependencyState();
        void updateWorkbenchRowGeometry();
        void includeWorkbenchAndDependencies(const QString& workbenchId);
        void ensureValidDefaultWorkbench();
        void createProfile();
        void duplicateProfile();
        void deleteProfile();
        bool saveCurrentProfile();
        void activateSelection();
        void updateCommandState();
        QString makeUniqueProfileId(const QString& baseId) const;
        QStringList orderedCatalogWorkbenchIds(const QStringList& preferredOrder) const;
        AvailableProfile* selectedProfile();
        const AvailableProfile* selectedProfile() const;

        const RobotQtViewerWorkbenchPackageRegistry& m_catalog;
        std::filesystem::path m_profilesDirectory;
        QString m_currentProfileId;
        QVector<AvailableProfile> m_profiles;
        QComboBox* m_profileCombo = nullptr;
        QLineEdit* m_profileIdEdit = nullptr;
        QLineEdit* m_profileNameEdit = nullptr;
        QLabel* m_runningProfileLabel = nullptr;
        QLabel* m_profileSummary = nullptr;
        QLabel* m_runtimeIdentityLabel = nullptr;
        QTreeWidget* m_workbenchTree = nullptr;
        QButtonGroup* m_defaultWorkbenchGroup = nullptr;
        QHash<QString, QCheckBox*> m_workbenchEnabledChecks;
        QHash<QString, QRadioButton*> m_defaultWorkbenchButtons;
        QToolButton* m_newProfileButton = nullptr;
        QToolButton* m_duplicateProfileButton = nullptr;
        QToolButton* m_deleteProfileButton = nullptr;
        QPushButton* m_saveProfileButton = nullptr;
        QPushButton* m_activateButton = nullptr;
        QPushButton* m_cancelButton = nullptr;
        RobotQtViewerProductProfile m_initialProfile;
        int m_loadedProfileIndex = -1;
        bool m_updatingEditor = false;
        bool m_pendingRestartRequired = false;
        bool m_configurationChanged = false;
        QString m_activatedProfileId;
    };
}
