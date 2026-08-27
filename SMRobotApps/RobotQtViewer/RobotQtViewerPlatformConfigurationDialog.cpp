#include "RobotQtViewerPlatformConfigurationDialog.h"

#include "RobotQtWidgetUtils.h"

#include <QByteArray>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <set>

namespace robot_qt_viewer
{
    namespace
    {
        constexpr int kModeIdRole = Qt::UserRole;
        constexpr int kOptionalRole = Qt::UserRole + 1;
    }

    RobotQtViewerPlatformConfigurationDialog::RobotQtViewerPlatformConfigurationDialog(
        const RobotQtViewerWorkbenchPackageRegistry& catalog,
        const std::filesystem::path& profilesDirectory,
        const std::filesystem::path& overlaysDirectory,
        const QString& currentProfileId,
        QWidget* parent)
        : QDialog(parent)
        , m_catalog(catalog)
        , m_profilesDirectory(profilesDirectory)
        , m_overlaysDirectory(overlaysDirectory)
    {
        setWindowTitle(QStringLiteral("Configure Simulation Platform"));
        resize(780, 520);

        auto* layout = new QVBoxLayout(this);
        auto* profileLayout = new QFormLayout();
        m_profileCombo = new QComboBox(this);
        configureInspectorCombo(m_profileCombo);
        profileLayout->addRow(QStringLiteral("Simulation platform"), m_profileCombo);
        layout->addLayout(profileLayout);

        m_profileSummary = new QLabel(this);
        m_profileSummary->setWordWrap(true);
        layout->addWidget(m_profileSummary);

        m_modeTree = new QTreeWidget(this);
        m_modeTree->setColumnCount(4);
        m_modeTree->setHeaderLabels({
            QStringLiteral("Enabled"),
            QStringLiteral("Workbench"),
            QStringLiteral("Package source"),
            QStringLiteral("Policy")
        });
        m_modeTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_modeTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_modeTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_modeTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        layout->addWidget(m_modeTree, 1);

        m_buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            this);
        configureDialogButtonBox(m_buttons);
        layout->addWidget(m_buttons);

        connect(m_profileCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            [this](int) { refreshModeItems(); });
        connect(m_modeTree, &QTreeWidget::itemChanged,
            this, [this](QTreeWidgetItem*, int) { refreshDependencyState(); });
        connect(m_buttons, &QDialogButtonBox::accepted, this, [this]() {
            applySelection();
        });
        connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        loadAvailableProfiles(currentProfileId);
    }

    QString RobotQtViewerPlatformConfigurationDialog::selectedProfileId() const
    {
        const AvailableProfile* available = selectedProfile();
        return available == nullptr ? QString() : available->profile.id;
    }

    void RobotQtViewerPlatformConfigurationDialog::loadAvailableProfiles(
        const QString& currentProfileId)
    {
        int currentIndex = -1;
        QStringList loadErrors;
        const QVector<std::filesystem::path> paths =
            RobotQtViewerPlatformProfileIo::discoverProfiles(m_profilesDirectory);
        for(const std::filesystem::path& path : paths) {
            RobotQtViewerPlatformProfile profile;
            QString errorMessage;
            if(!RobotQtViewerPlatformProfileIo::loadProfile(path, &profile, &errorMessage)) {
                loadErrors.push_back(errorMessage);
                continue;
            }
            AvailableProfile available;
            available.path = path;
            available.profile = profile;
            const int index = m_profiles.size();
            m_profiles.push_back(available);
            m_profileCombo->addItem(profile.displayName, profile.id);
            if(profile.id == currentProfileId) {
                currentIndex = index;
            }
        }
        if(currentIndex < 0 && !m_profiles.isEmpty()) {
            currentIndex = 0;
        }
        if(currentIndex >= 0) {
            m_profileCombo->setCurrentIndex(currentIndex);
        }
        if(m_profiles.isEmpty()) {
            m_profileSummary->setText(loadErrors.isEmpty()
                ? QStringLiteral("No platform profiles were found.")
                : loadErrors.join(QLatin1Char('\n')));
            m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
            return;
        }
        refreshModeItems();
    }

    void RobotQtViewerPlatformConfigurationDialog::refreshModeItems()
    {
        m_updatingModeItems = true;
        m_modeTree->clear();
        const AvailableProfile* available = selectedProfile();
        if(available == nullptr) {
            m_updatingModeItems = false;
            return;
        }

        RobotQtViewerPlatformUserOverlay overlay;
        QString overlayError;
        if(!RobotQtViewerPlatformProfileIo::loadOverlay(
               overlayPath(available->profile.id), &overlay, &overlayError)) {
            overlay = {};
            overlay.profileId = available->profile.id;
        }
        const RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(
                m_catalog, available->profile, overlay);
        QString summary = QStringLiteral("%1  |  Profile ID: %2")
            .arg(available->profile.displayName, available->profile.id);
        if(!overlayError.isEmpty()) {
            summary += QStringLiteral("\nOverlay ignored: %1").arg(overlayError);
        }
        if(!resolved.diagnostics.isEmpty()) {
            summary += QStringLiteral("\n%1").arg(resolved.diagnosticText());
        }
        m_profileSummary->setText(summary);

        std::set<QString> modeIds;
        for(const QString& modeId : available->profile.requiredModeIds) {
            modeIds.insert(modeId);
        }
        for(const QString& featureId : available->profile.requiredFeatureIds) {
            if(const RobotQtViewerWorkbenchFeatureDesc* featureDesc =
                    m_catalog.feature(featureId)) {
                for(const QString& modeId : featureDesc->requiredModeIds) {
                    modeIds.insert(modeId);
                }
            }
        }
        for(const QString& modeId : available->profile.optionalModeIds) {
            modeIds.insert(modeId);
        }
        for(const QString& modeId : resolved.requiredModeIds) {
            modeIds.insert(modeId);
        }
        for(const QString& modeId : resolved.enabledModeIds) {
            modeIds.insert(modeId);
        }

        QStringList orderedModeIds;
        for(const QString& modeId : available->profile.modeOrder) {
            if(modeIds.find(modeId) != modeIds.end() && !orderedModeIds.contains(modeId)) {
                orderedModeIds.push_back(modeId);
            }
        }
        for(const QString& modeId : modeIds) {
            if(!orderedModeIds.contains(modeId)) {
                orderedModeIds.push_back(modeId);
            }
        }

        for(const QString& modeId : orderedModeIds) {
            const RobotQtViewerWorkbenchModeDesc* modeDesc =
                m_catalog.registeredMode(modeId);
            const bool required = resolved.requiredModeIds.contains(modeId);
            const bool optional = available->profile.optionalModeIds.contains(modeId) && !required;
            auto* item = new QTreeWidgetItem(m_modeTree);
            item->setData(0, kModeIdRole, modeId);
            item->setData(0, kOptionalRole, optional);
            item->setCheckState(0,
                resolved.containsMode(modeId) ? Qt::Checked : Qt::Unchecked);
            item->setText(1, modeDesc == nullptr ? modeId : modeDesc->descriptor.displayName);
            if(modeDesc == nullptr) {
                item->setText(2, QStringLiteral("Unavailable"));
                item->setText(3, required
                    ? QStringLiteral("Required, missing")
                    : QStringLiteral("Optional, missing"));
                item->setDisabled(true);
                continue;
            }
            const RobotQtViewerWorkbenchPackageDesc* packageDesc =
                m_catalog.package(modeDesc->packageId);
            item->setText(2,
                packageDesc != nullptr &&
                    packageDesc->source == RobotQtViewerWorkbenchPackageSource::Prebuilt
                    ? QStringLiteral("Prebuilt")
                    : QStringLiteral("Source"));
            item->setText(3, required
                ? QStringLiteral("Required")
                : (optional ? QStringLiteral("Optional") : QStringLiteral("Dependency")));
            if(!optional || !available->profile.allowUserOverrides) {
                item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            }
        }
        m_updatingModeItems = false;
    }

    void RobotQtViewerPlatformConfigurationDialog::refreshDependencyState()
    {
        if(m_updatingModeItems) {
            return;
        }
        const AvailableProfile* available = selectedProfile();
        if(available == nullptr) {
            return;
        }

        RobotQtViewerPlatformUserOverlay overlay;
        overlay.profileId = available->profile.id;
        for(int index = 0; index < m_modeTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = m_modeTree->topLevelItem(index);
            if(item == nullptr || !item->data(0, kOptionalRole).toBool()) {
                continue;
            }
            const QString modeId = item->data(0, kModeIdRole).toString();
            if(item->checkState(0) == Qt::Checked) {
                overlay.enabledOptionalModeIds.push_back(modeId);
            } else {
                overlay.disabledOptionalModeIds.push_back(modeId);
            }
        }

        const RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(
                m_catalog, available->profile, overlay);
        m_updatingModeItems = true;
        for(int index = 0; index < m_modeTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = m_modeTree->topLevelItem(index);
            if(item == nullptr) {
                continue;
            }
            const QString modeId = item->data(0, kModeIdRole).toString();
            item->setCheckState(0,
                resolved.containsMode(modeId) ? Qt::Checked : Qt::Unchecked);
        }
        m_updatingModeItems = false;

        QString summary = QStringLiteral("%1  |  Profile ID: %2")
            .arg(available->profile.displayName, available->profile.id);
        if(!resolved.diagnostics.isEmpty()) {
            summary += QStringLiteral("\n%1").arg(resolved.diagnosticText());
        }
        m_profileSummary->setText(summary);
    }

    void RobotQtViewerPlatformConfigurationDialog::applySelection()
    {
        const AvailableProfile* available = selectedProfile();
        if(available == nullptr) {
            return;
        }
        RobotQtViewerPlatformUserOverlay overlay;
        overlay.profileId = available->profile.id;
        for(int index = 0; index < m_modeTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = m_modeTree->topLevelItem(index);
            if(item == nullptr || !item->data(0, kOptionalRole).toBool()) {
                continue;
            }
            const QString modeId = item->data(0, kModeIdRole).toString();
            if(item->checkState(0) == Qt::Checked) {
                overlay.enabledOptionalModeIds.push_back(modeId);
            } else {
                overlay.disabledOptionalModeIds.push_back(modeId);
            }
        }

        const RobotQtViewerResolvedPlatformComposition resolved =
            RobotQtViewerPlatformProfileResolver::resolve(
                m_catalog, available->profile, overlay);
        if(!resolved.succeeded()) {
            QMessageBox::critical(
                this,
                QStringLiteral("Invalid Platform Configuration"),
                resolved.diagnosticText());
            return;
        }
        QString saveError;
        if(!RobotQtViewerPlatformProfileIo::saveOverlay(
               overlayPath(available->profile.id), overlay, &saveError)) {
            QMessageBox::critical(
                this,
                QStringLiteral("Platform Configuration"),
                saveError);
            return;
        }
        QSettings settings;
        settings.setValue(QStringLiteral("platform/lastProfileId"), available->profile.id);
        accept();
    }

    std::filesystem::path RobotQtViewerPlatformConfigurationDialog::overlayPath(
        const QString& profileId) const
    {
        const QByteArray fileName =
            (profileId + QStringLiteral(".overlay.json")).toUtf8();
        return m_overlaysDirectory / std::filesystem::u8path(fileName.constData());
    }

    const RobotQtViewerPlatformConfigurationDialog::AvailableProfile*
    RobotQtViewerPlatformConfigurationDialog::selectedProfile() const
    {
        const int index = m_profileCombo == nullptr ? -1 : m_profileCombo->currentIndex();
        return index >= 0 && index < m_profiles.size() ? &m_profiles[index] : nullptr;
    }
}
