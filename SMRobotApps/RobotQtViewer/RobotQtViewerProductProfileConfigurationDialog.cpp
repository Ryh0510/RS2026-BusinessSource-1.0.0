#include "RobotQtViewerProductProfileConfigurationDialog.h"

#include "RobotQtViewerLocalization.h"
#include "RobotQtWidgetUtils.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <set>

namespace robot_qt_viewer
{
    namespace
    {
        constexpr int kWorkbenchIdRole = Qt::UserRole;

        QString platformText(const QString& key, const QString& fallback)
        {
            const RobotQtViewerLocalizationService* localization = qApp == nullptr
                ? nullptr
                : RobotQtViewerLocalizationService::installedOnApplication(*qApp);
            return localization == nullptr ? fallback : localization->text(key, fallback);
        }

        QString runtimeBuildIdentity()
        {
            return platformText(
                QStringLiteral("platform.manager.runtimeIdentity"),
                QStringLiteral("Executable: %1\nBuild: %2, %3-bit, compiled %4"))
                .arg(
                    QCoreApplication::applicationFilePath(),
                    QString::fromLatin1(SMROBOT_WORKBENCH_BUILD_CONFIGURATION),
                    QString::number(sizeof(void*) * 8),
                    QString::fromLatin1(__DATE__ " " __TIME__));
        }
    }

    RobotQtViewerProductProfileConfigurationDialog::RobotQtViewerProductProfileConfigurationDialog(
        const RobotQtViewerWorkbenchPackageRegistry& catalog,
        const std::filesystem::path& profilesDirectory,
        const QString& currentProfileId,
        QWidget* parent)
        : QDialog(parent)
        , m_catalog(catalog)
        , m_profilesDirectory(profilesDirectory)
        , m_currentProfileId(currentProfileId)
    {
        setWindowTitle(platformText(
            QStringLiteral("platform.manager.title"),
            QStringLiteral("Product Profile Manager")));
        setMinimumSize(760, 540);
        resize(1120, 780);

        auto* layout = new QVBoxLayout(this);
        auto* profileRow = new QHBoxLayout();
        profileRow->addWidget(new QLabel(platformText(
            QStringLiteral("platform.dialog.productProfile"),
            QStringLiteral("Product Profile")), this));
        m_profileCombo = new QComboBox(this);
        m_profileCombo->setObjectName(QStringLiteral("productProfileCombo"));
        configureInspectorCombo(m_profileCombo, 24);
        profileRow->addWidget(m_profileCombo, 1);

        m_newProfileButton = new QToolButton(this);
        m_newProfileButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        m_newProfileButton->setToolTip(platformText(
            QStringLiteral("platform.manager.newProfile"),
            QStringLiteral("New Product Profile")));
        profileRow->addWidget(m_newProfileButton);

        m_duplicateProfileButton = new QToolButton(this);
        m_duplicateProfileButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
        m_duplicateProfileButton->setToolTip(platformText(
            QStringLiteral("platform.manager.duplicateProfile"),
            QStringLiteral("Duplicate Product Profile")));
        profileRow->addWidget(m_duplicateProfileButton);

        m_deleteProfileButton = new QToolButton(this);
        m_deleteProfileButton->setObjectName(QStringLiteral("deleteProductProfileButton"));
        m_deleteProfileButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
        m_deleteProfileButton->setToolTip(platformText(
            QStringLiteral("platform.manager.deleteProfile"),
            QStringLiteral("Delete Product Profile")));
        profileRow->addWidget(m_deleteProfileButton);
        layout->addLayout(profileRow);

        m_runningProfileLabel = new QLabel(this);
        m_runningProfileLabel->setObjectName(QStringLiteral("runningProductProfileLabel"));
        m_runningProfileLabel->setWordWrap(true);
        layout->addWidget(m_runningProfileLabel);

        auto* profileForm = new QFormLayout();
        configureInspectorForm(profileForm);
        m_profileIdEdit = new QLineEdit(this);
        m_profileNameEdit = new QLineEdit(this);
        profileForm->addRow(platformText(
            QStringLiteral("platform.manager.profileId"),
            QStringLiteral("Profile ID")), m_profileIdEdit);
        profileForm->addRow(platformText(
            QStringLiteral("platform.manager.displayName"),
            QStringLiteral("Display Name")), m_profileNameEdit);
        layout->addLayout(profileForm);

        m_profileSummary = new QLabel(this);
        m_profileSummary->setWordWrap(true);
        layout->addWidget(m_profileSummary);

        m_workbenchTree = new QTreeWidget(this);
        m_workbenchTree->setObjectName(QStringLiteral("productProfileWorkbenchTree"));
        m_workbenchTree->setColumnCount(2);
        m_workbenchTree->setHeaderLabels({
            platformText(QStringLiteral("platform.manager.workbenchTree"),
                QStringLiteral("Workbench")),
            platformText(QStringLiteral("platform.manager.dependencies"),
                QStringLiteral("Dependencies / Status"))
        });
        m_workbenchTree->header()->setMinimumSectionSize(220);
        m_workbenchTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
        m_workbenchTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_workbenchTree->header()->setStretchLastSection(true);
        m_workbenchTree->setColumnWidth(0, 420);
        m_workbenchTree->setAlternatingRowColors(true);
        m_workbenchTree->setRootIsDecorated(false);
        m_workbenchTree->setTextElideMode(Qt::ElideRight);
        m_workbenchTree->setUniformRowHeights(false);
        m_workbenchTree->setWordWrap(false);
        m_workbenchTree->setMinimumHeight(120);
        layout->addWidget(m_workbenchTree, 1);

        auto* footer = new QWidget(this);
        footer->setObjectName(QStringLiteral("productProfileFooter"));
        footer->setMinimumHeight(88);
        auto* footerLayout = new QVBoxLayout(footer);
        footerLayout->setContentsMargins(0, 2, 0, 0);
        footerLayout->setSpacing(2);

        auto* commandBar = new QWidget(footer);
        commandBar->setObjectName(QStringLiteral("productProfileCommandBar"));
        commandBar->setMinimumHeight(44);
        auto* commandLayout = new QHBoxLayout(commandBar);
        commandLayout->setContentsMargins(0, 4, 0, 0);
        commandLayout->addStretch(1);
        m_saveProfileButton = new QPushButton(platformText(
            QStringLiteral("platform.manager.saveProfile"),
            QStringLiteral("Save Current Profile")), commandBar);
        m_saveProfileButton->setObjectName(QStringLiteral("saveCurrentProductProfileButton"));
        m_activateButton = new QPushButton(platformText(
            QStringLiteral("platform.manager.activateRestart"),
            QStringLiteral("Switch and Restart")), commandBar);
        m_activateButton->setObjectName(QStringLiteral("switchProductProfileButton"));
        m_cancelButton = new QPushButton(platformText(
            QStringLiteral("platform.manager.cancel"),
            QStringLiteral("Cancel")), commandBar);
        m_cancelButton->setObjectName(QStringLiteral("cancelProductProfileButton"));
        configureActionButton(m_saveProfileButton, UiActionRole::Standard);
        configureActionButton(m_activateButton, UiActionRole::Primary);
        configureActionButton(m_cancelButton, UiActionRole::Standard);
        for(QPushButton* button : { m_saveProfileButton, m_activateButton, m_cancelButton }) {
            QSizePolicy policy = button->sizePolicy();
            policy.setHorizontalPolicy(QSizePolicy::Minimum);
            button->setSizePolicy(policy);
            button->setMinimumSize(QSize(130, 36));
        }
        commandLayout->addWidget(m_saveProfileButton);
        commandLayout->addWidget(m_activateButton);
        commandLayout->addWidget(m_cancelButton);
        footerLayout->addWidget(commandBar);

        m_runtimeIdentityLabel = new QLabel(runtimeBuildIdentity(), footer);
        m_runtimeIdentityLabel->setObjectName(QStringLiteral("platformRuntimeIdentityLabel"));
        m_runtimeIdentityLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_runtimeIdentityLabel->setWordWrap(true);
        footerLayout->addWidget(m_runtimeIdentityLabel);
        layout->addWidget(footer);

        m_defaultWorkbenchGroup = new QButtonGroup(this);
        m_defaultWorkbenchGroup->setExclusive(true);

        connect(m_profileCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if(m_updatingEditor) {
                    return;
                }
                captureEditedProfile();
                m_loadedProfileIndex = index;
                refreshEditor();
            });
        connect(m_newProfileButton, &QToolButton::clicked,
            this, &RobotQtViewerProductProfileConfigurationDialog::createProfile);
        connect(m_duplicateProfileButton, &QToolButton::clicked,
            this, &RobotQtViewerProductProfileConfigurationDialog::duplicateProfile);
        connect(m_deleteProfileButton, &QToolButton::clicked,
            this, &RobotQtViewerProductProfileConfigurationDialog::deleteProfile);
        connect(m_profileNameEdit, &QLineEdit::textEdited, this, [this]() {
            if(AvailableProfile* profile = selectedProfile()) {
                profile->dirty = true;
            }
            refreshDependencyState();
        });
        connect(m_profileIdEdit, &QLineEdit::textEdited, this, [this]() {
            if(AvailableProfile* profile = selectedProfile()) {
                profile->dirty = true;
            }
            refreshDependencyState();
        });
        connect(m_saveProfileButton, &QPushButton::clicked,
            this, [this]() { saveCurrentProfile(); });
        connect(m_activateButton, &QPushButton::clicked,
            this, &RobotQtViewerProductProfileConfigurationDialog::activateSelection);
        connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

        loadAvailableProfiles(currentProfileId);
    }

    QString RobotQtViewerProductProfileConfigurationDialog::selectedProfileId() const
    {
        if(!m_activatedProfileId.isEmpty()) {
            return m_activatedProfileId;
        }
        const AvailableProfile* available = selectedProfile();
        return available == nullptr ? QString() : available->profile.id;
    }

    bool RobotQtViewerProductProfileConfigurationDialog::configurationChanged() const
    {
        return m_configurationChanged;
    }

    void RobotQtViewerProductProfileConfigurationDialog::showEvent(QShowEvent* event)
    {
        QDialog::showEvent(event);
        QScreen* targetScreen = screen();
        if(targetScreen == nullptr && parentWidget() != nullptr) {
            targetScreen = parentWidget()->screen();
        }
        if(targetScreen == nullptr) {
            targetScreen = QApplication::primaryScreen();
        }
        if(targetScreen == nullptr) {
            return;
        }
        const QRect bounds = targetScreen->availableGeometry().adjusted(12, 12, -12, -12);
        const int clientWidth = std::max(1, bounds.width() - 16);
        const int clientHeight = std::max(1, bounds.height() - 40);
        const int preferredWidth = qBound(980, bounds.width() * 4 / 5, 1440);
        const int preferredHeight = qBound(720, bounds.height() * 4 / 5, 980);
        setMinimumSize(qMin(760, clientWidth), qMin(540, clientHeight));
        resize(qMin(preferredWidth, clientWidth), qMin(preferredHeight, clientHeight));
        move(bounds.center() - rect().center());
        const QRect placed = frameGeometry();
        move(
            qBound(bounds.left(), placed.left(), bounds.right() - placed.width() + 1),
            qBound(bounds.top(), placed.top(), bounds.bottom() - placed.height() + 1));
    }

    QStringList RobotQtViewerProductProfileConfigurationDialog::orderedCatalogWorkbenchIds(
        const QStringList& preferredOrder) const
    {
        QStringList result;
        for(const QString& workbenchId : preferredOrder) {
            if(m_catalog.registeredWorkbench(workbenchId) != nullptr && !result.contains(workbenchId)) {
                result.push_back(workbenchId);
            }
        }
        QVector<const RobotQtViewerWorkbenchDesc*> remaining;
        for(const RobotQtViewerWorkbenchDesc& workbench : m_catalog.workbenches()) {
            if(!result.contains(workbench.descriptor.id)) {
                remaining.push_back(&workbench);
            }
        }
        std::sort(remaining.begin(), remaining.end(),
            [](const RobotQtViewerWorkbenchDesc* left,
               const RobotQtViewerWorkbenchDesc* right) {
                if(left->defaultOrder != right->defaultOrder) {
                    return left->defaultOrder < right->defaultOrder;
                }
                return left->descriptor.id < right->descriptor.id;
            });
        for(const RobotQtViewerWorkbenchDesc* workbench : remaining) {
            result.push_back(workbench->descriptor.id);
        }
        return result;
    }

    void RobotQtViewerProductProfileConfigurationDialog::loadAvailableProfiles(
        const QString& currentProfileId)
    {
        m_updatingEditor = true;
        int currentIndex = -1;
        for(const std::filesystem::path& path :
            RobotQtViewerProductProfileIo::discoverProfiles(m_profilesDirectory)) {
            RobotQtViewerProductProfile profile;
            QString errorMessage;
            if(!RobotQtViewerProductProfileIo::loadProfile(path, &profile, &errorMessage)) {
                continue;
            }
            AvailableProfile available;
            available.path = path;
            available.profile = profile;
            m_profiles.push_back(available);
            m_profileCombo->addItem(profile.displayName, profile.id);
            if(profile.id == currentProfileId) {
                currentIndex = m_profiles.size() - 1;
            }
        }
        if(currentIndex < 0) {
            AvailableProfile fallback;
            fallback.profile = makeRobotQtViewerBuiltInBaseProductProfile(m_catalog);
            fallback.builtInFallback = true;
            m_profiles.push_back(fallback);
            m_profileCombo->addItem(fallback.profile.displayName, fallback.profile.id);
            currentIndex = m_profiles.size() - 1;
        }
        m_loadedProfileIndex = currentIndex;
        m_profileCombo->setCurrentIndex(currentIndex);
        m_initialProfile = m_profiles[currentIndex].profile;
        m_runningProfileLabel->setText(platformText(
            QStringLiteral("platform.manager.runningProfile"),
            QStringLiteral("Running Product Profile: %1  |  Profile ID: %2"))
            .arg(m_initialProfile.displayName, m_initialProfile.id));
        m_updatingEditor = false;
        refreshEditor();
    }

    void RobotQtViewerProductProfileConfigurationDialog::captureEditedProfile()
    {
        AvailableProfile* available = selectedProfile();
        if(available == nullptr || m_updatingEditor) {
            return;
        }
        available->profile.id = m_profileIdEdit->text().trimmed();
        available->profile.displayName = m_profileNameEdit->text().trimmed();
        available->profile.workbenchIds.clear();
        available->profile.defaultWorkbenchId.clear();
        for(int index = 0; index < m_workbenchTree->topLevelItemCount(); ++index) {
            const QString workbenchId =
                m_workbenchTree->topLevelItem(index)->data(0, kWorkbenchIdRole).toString();
            QCheckBox* enabled = m_workbenchEnabledChecks.value(workbenchId, nullptr);
            QRadioButton* defaultWorkbench =
                m_defaultWorkbenchButtons.value(workbenchId, nullptr);
            if(enabled != nullptr && enabled->isChecked()) {
                available->profile.workbenchIds.push_back(workbenchId);
                if(defaultWorkbench != nullptr && defaultWorkbench->isChecked()) {
                    available->profile.defaultWorkbenchId = workbenchId;
                }
            }
        }
    }

    void RobotQtViewerProductProfileConfigurationDialog::refreshEditor()
    {
        AvailableProfile* available = selectedProfile();
        if(available == nullptr) {
            return;
        }
        m_updatingEditor = true;
        m_profileIdEdit->setText(available->profile.id);
        m_profileIdEdit->setReadOnly(!available->newProfile);
        m_profileNameEdit->setText(available->profile.displayName);
        m_profileNameEdit->setReadOnly(available->builtInFallback);
        m_updatingEditor = false;
        refreshWorkbenchItems();
        refreshDependencyState();
    }

    void RobotQtViewerProductProfileConfigurationDialog::refreshWorkbenchItems()
    {
        AvailableProfile* available = selectedProfile();
        if(available == nullptr) {
            return;
        }
        m_updatingEditor = true;
        m_workbenchTree->clear();
        m_workbenchEnabledChecks.clear();
        m_defaultWorkbenchButtons.clear();
        delete m_defaultWorkbenchGroup;
        m_defaultWorkbenchGroup = new QButtonGroup(this);
        m_defaultWorkbenchGroup->setExclusive(true);

        for(const QString& workbenchId :
            orderedCatalogWorkbenchIds(available->profile.workbenchIds)) {
            const RobotQtViewerWorkbenchDesc* workbench =
                m_catalog.registeredWorkbench(workbenchId);
            if(workbench == nullptr) {
                continue;
            }
            auto* item = new QTreeWidgetItem(m_workbenchTree);
            item->setData(0, kWorkbenchIdRole, workbenchId);
            item->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
            auto* cell = new QWidget(m_workbenchTree);
            cell->setObjectName(QStringLiteral("productProfileWorkbenchCell"));
            cell->setAttribute(Qt::WA_TranslucentBackground);
            auto* cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(4, 2, 4, 2);
            cellLayout->setSpacing(1);
            auto* enabled = new QCheckBox(workbench->descriptor.displayName, cell);
            enabled->setObjectName(QStringLiteral("productProfileWorkbenchEnabledCheck"));
            enabled->setProperty("workbenchId", workbenchId);
            enabled->setChecked(available->profile.workbenchIds.contains(workbenchId));
            auto* defaultWorkbench = new QRadioButton(platformText(
                QStringLiteral("platform.manager.defaultWorkbench"),
                QStringLiteral("Default Workbench")), cell);
            defaultWorkbench->setObjectName(
                QStringLiteral("productProfileDefaultWorkbenchRadio"));
            defaultWorkbench->setProperty("workbenchId", workbenchId);
            defaultWorkbench->setChecked(
                available->profile.defaultWorkbenchId == workbenchId);
            defaultWorkbench->setVisible(enabled->isChecked());
            cellLayout->addWidget(enabled);
            cellLayout->addWidget(defaultWorkbench);
            m_workbenchTree->setItemWidget(item, 0, cell);
            m_workbenchEnabledChecks.insert(workbenchId, enabled);
            m_defaultWorkbenchButtons.insert(workbenchId, defaultWorkbench);
            m_defaultWorkbenchGroup->addButton(defaultWorkbench);

            connect(enabled, &QCheckBox::toggled, this,
                [this, workbenchId](bool checked) {
                    if(m_updatingEditor) {
                        return;
                    }
                    if(AvailableProfile* profile = selectedProfile()) {
                        profile->dirty = true;
                    }
                    if(checked) {
                        includeWorkbenchAndDependencies(workbenchId);
                    }
                    refreshDependencyState();
                });
            connect(defaultWorkbench, &QRadioButton::toggled, this,
                [this](bool checked) {
                    if(m_updatingEditor || !checked) {
                        return;
                    }
                    if(AvailableProfile* profile = selectedProfile()) {
                        profile->dirty = true;
                    }
                    refreshDependencyState();
                });
        }
        m_updatingEditor = false;
    }

    void RobotQtViewerProductProfileConfigurationDialog::updateWorkbenchRowGeometry()
    {
        for(int index = 0; index < m_workbenchTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = m_workbenchTree->topLevelItem(index);
            const QString workbenchId = item->data(0, kWorkbenchIdRole).toString();
            QCheckBox* enabled = m_workbenchEnabledChecks.value(workbenchId, nullptr);
            QRadioButton* defaultWorkbench =
                m_defaultWorkbenchButtons.value(workbenchId, nullptr);
            QWidget* cell = m_workbenchTree->itemWidget(item, 0);
            if(enabled == nullptr || defaultWorkbench == nullptr || cell == nullptr) {
                continue;
            }

            const int controlHeight = qMax(
                enabled->sizeHint().height(), defaultWorkbench->sizeHint().height());
            const int rowHeight = enabled->isChecked()
                ? controlHeight * 2 + 7
                : controlHeight + 6;
            cell->setFixedHeight(rowHeight);
            item->setSizeHint(0, QSize(0, rowHeight));
            item->setSizeHint(1, QSize(0, rowHeight));
        }
        m_workbenchTree->doItemsLayout();
        m_workbenchTree->viewport()->update();
    }

    void RobotQtViewerProductProfileConfigurationDialog::includeWorkbenchAndDependencies(
        const QString& workbenchId)
    {
        std::set<QString> visiting;
        std::function<void(const QString&)> include = [&](const QString& id) {
            if(!visiting.insert(id).second) {
                return;
            }
            if(QCheckBox* check = m_workbenchEnabledChecks.value(id, nullptr)) {
                check->setChecked(true);
            }
            const RobotQtViewerWorkbenchDesc* workbench = m_catalog.registeredWorkbench(id);
            if(workbench != nullptr) {
                for(const QString& dependencyId : workbench->requiredWorkbenchIds) {
                    include(dependencyId);
                }
            }
        };
        m_updatingEditor = true;
        include(workbenchId);
        m_updatingEditor = false;
    }

    void RobotQtViewerProductProfileConfigurationDialog::ensureValidDefaultWorkbench()
    {
        QRadioButton* selected = nullptr;
        for(auto iterator = m_defaultWorkbenchButtons.cbegin();
            iterator != m_defaultWorkbenchButtons.cend(); ++iterator) {
            QCheckBox* check = m_workbenchEnabledChecks.value(iterator.key(), nullptr);
            if(check != nullptr && check->isChecked() && iterator.value()->isChecked()) {
                selected = iterator.value();
                break;
            }
        }
        if(selected != nullptr) {
            return;
        }
        for(int index = 0; index < m_workbenchTree->topLevelItemCount(); ++index) {
            const QString id =
                m_workbenchTree->topLevelItem(index)->data(0, kWorkbenchIdRole).toString();
            QCheckBox* check = m_workbenchEnabledChecks.value(id, nullptr);
            if(check != nullptr && check->isChecked()) {
                m_defaultWorkbenchButtons.value(id)->setChecked(true);
                return;
            }
        }
    }

    void RobotQtViewerProductProfileConfigurationDialog::refreshDependencyState()
    {
        AvailableProfile* available = selectedProfile();
        if(available == nullptr || m_updatingEditor) {
            return;
        }
        ensureValidDefaultWorkbench();
        captureEditedProfile();
        const bool writable = !available->builtInFallback;
        const RobotQtViewerResolvedProductComposition resolved =
            RobotQtViewerProductProfileResolver::resolve(m_catalog, available->profile);
        m_updatingEditor = true;
        for(int index = 0; index < m_workbenchTree->topLevelItemCount(); ++index) {
            QTreeWidgetItem* item = m_workbenchTree->topLevelItem(index);
            const QString workbenchId = item->data(0, kWorkbenchIdRole).toString();
            QCheckBox* enabled = m_workbenchEnabledChecks.value(workbenchId, nullptr);
            QRadioButton* defaultWorkbench =
                m_defaultWorkbenchButtons.value(workbenchId, nullptr);
            QStringList dependencySources;
            for(const QString& sourceId : available->profile.workbenchIds) {
                const RobotQtViewerWorkbenchDesc* source = m_catalog.registeredWorkbench(sourceId);
                if(source != nullptr && source->requiredWorkbenchIds.contains(workbenchId)) {
                    dependencySources.push_back(source->descriptor.displayName);
                }
            }
            enabled->setEnabled(writable && dependencySources.isEmpty());
            defaultWorkbench->setVisible(enabled->isChecked());
            defaultWorkbench->setEnabled(writable && enabled->isChecked());

            QStringList status;
            if(enabled->isChecked()) {
                status.push_back(platformText(
                    QStringLiteral("platform.manager.enabledInComposition"),
                    QStringLiteral("Included in this Product Profile")));
            } else {
                status.push_back(platformText(
                    QStringLiteral("platform.manager.excludedFromComposition"),
                    QStringLiteral("Not included")));
            }
            if(!dependencySources.isEmpty()) {
                status.push_back(platformText(
                    QStringLiteral("platform.manager.requiredByWorkbenches"),
                    QStringLiteral("Required by: %1"))
                    .arg(dependencySources.join(QStringLiteral(", "))));
            }
            const RobotQtViewerWorkbenchDesc* workbench =
                m_catalog.registeredWorkbench(workbenchId);
            if(workbench != nullptr && !workbench->requiredWorkbenchIds.isEmpty()) {
                QStringList names;
                for(const QString& dependencyId : workbench->requiredWorkbenchIds) {
                    const RobotQtViewerWorkbenchDesc* dependency =
                        m_catalog.registeredWorkbench(dependencyId);
                    names.push_back(dependency == nullptr
                        ? dependencyId
                        : dependency->descriptor.displayName);
                }
                status.push_back(platformText(
                    QStringLiteral("platform.manager.requiresWorkbenches"),
                    QStringLiteral("Requires: %1"))
                    .arg(names.join(QStringLiteral(", "))));
            }
            for(const RobotQtViewerProductProfileDiagnostic& diagnostic : resolved.diagnostics) {
                if(diagnostic.subjectId == workbenchId) {
                    status.push_back(diagnostic.message);
                }
            }
            item->setText(1, status.join(QStringLiteral("; ")));
            item->setToolTip(1, item->text(1));
        }
        m_updatingEditor = false;
        updateWorkbenchRowGeometry();

        QString summary = platformText(
            QStringLiteral("platform.manager.selectedProfile"),
            QStringLiteral("Selected Product Profile: %1  |  Profile ID: %2"))
            .arg(available->profile.displayName, available->profile.id);
        if(available->builtInFallback) {
            summary += QStringLiteral("\n") + platformText(
                QStringLiteral("platform.dialog.builtInFallback"),
                QStringLiteral("Built-in fallback: shipped base profile was not found."));
        }
        if(!resolved.diagnostics.isEmpty()) {
            summary += QStringLiteral("\n") + resolved.diagnosticText();
        }
        m_profileSummary->setText(summary);
        m_pendingRestartRequired =
            available->profile.id != m_currentProfileId ||
            available->profile.displayName != m_initialProfile.displayName ||
            available->profile.requiredFeatureIds != m_initialProfile.requiredFeatureIds ||
            available->profile.workbenchIds != m_initialProfile.workbenchIds ||
            available->profile.defaultWorkbenchId != m_initialProfile.defaultWorkbenchId;
        updateCommandState();
    }

    void RobotQtViewerProductProfileConfigurationDialog::createProfile()
    {
        captureEditedProfile();
        AvailableProfile available;
        available.profile.id = makeUniqueProfileId(QStringLiteral("product-profile"));
        available.profile.displayName = platformText(
            QStringLiteral("platform.manager.newProfileName"),
            QStringLiteral("New Product Profile"));
        QString initialId = robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);
        if(m_catalog.registeredWorkbench(initialId) == nullptr && !m_catalog.workbenches().isEmpty()) {
            initialId = m_catalog.workbenches().front().descriptor.id;
        }
        std::function<void(const QString&)> append = [&](const QString& id) {
            const RobotQtViewerWorkbenchDesc* workbench = m_catalog.registeredWorkbench(id);
            if(workbench == nullptr || available.profile.workbenchIds.contains(id)) {
                return;
            }
            for(const QString& dependencyId : workbench->requiredWorkbenchIds) {
                append(dependencyId);
            }
            available.profile.workbenchIds.push_back(id);
        };
        append(initialId);
        available.profile.defaultWorkbenchId = initialId;
        available.newProfile = true;
        available.dirty = true;
        m_profiles.push_back(available);
        m_profileCombo->addItem(available.profile.displayName, available.profile.id);
        m_profileCombo->setCurrentIndex(m_profiles.size() - 1);
    }

    void RobotQtViewerProductProfileConfigurationDialog::duplicateProfile()
    {
        captureEditedProfile();
        const AvailableProfile* source = selectedProfile();
        if(source == nullptr) {
            return;
        }
        AvailableProfile duplicate;
        duplicate.profile = source->profile;
        duplicate.profile.id = makeUniqueProfileId(source->profile.id + QStringLiteral("-copy"));
        duplicate.profile.displayName = platformText(
            QStringLiteral("platform.manager.copyName"),
            QStringLiteral("%1 Copy")).arg(source->profile.displayName);
        duplicate.newProfile = true;
        duplicate.dirty = true;
        m_profiles.push_back(duplicate);
        m_profileCombo->addItem(duplicate.profile.displayName, duplicate.profile.id);
        m_profileCombo->setCurrentIndex(m_profiles.size() - 1);
    }

    void RobotQtViewerProductProfileConfigurationDialog::deleteProfile()
    {
        captureEditedProfile();
        const AvailableProfile* selected = selectedProfile();
        if(selected == nullptr || selected->builtInFallback ||
            selected->profile.id == m_currentProfileId) {
            return;
        }
        if(QMessageBox::question(this,
               platformText(QStringLiteral("platform.manager.deleteProfileTitle"),
                   QStringLiteral("Delete Product Profile")),
               platformText(QStringLiteral("platform.manager.deleteProfileConfirm"),
                   QStringLiteral("Delete Product Profile '%1'?"))
                   .arg(selected->profile.displayName),
               QMessageBox::Yes | QMessageBox::No,
               QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
        if(!selected->newProfile && !selected->path.empty()) {
            std::error_code error;
            if(!std::filesystem::remove(selected->path, error) || error) {
                QMessageBox::critical(this,
                    platformText(QStringLiteral("platform.manager.deleteProfileTitle"),
                        QStringLiteral("Delete Product Profile")),
                    QString::fromStdString(error.message()));
                return;
            }
        }
        const int removedIndex = m_loadedProfileIndex;
        m_updatingEditor = true;
        m_profiles.removeAt(removedIndex);
        m_profileCombo->removeItem(removedIndex);
        m_updatingEditor = false;
        m_loadedProfileIndex = std::min(removedIndex, m_profiles.size() - 1);
        m_profileCombo->setCurrentIndex(m_loadedProfileIndex);
        refreshEditor();
    }

    bool RobotQtViewerProductProfileConfigurationDialog::saveCurrentProfile()
    {
        refreshDependencyState();
        AvailableProfile* available = selectedProfile();
        if(available == nullptr || available->builtInFallback) {
            return false;
        }
        for(int index = 0; index < m_profiles.size(); ++index) {
            if(index != m_loadedProfileIndex &&
                m_profiles[index].profile.id == available->profile.id) {
                QMessageBox::critical(this,
                    platformText(QStringLiteral("platform.dialog.invalidTitle"),
                        QStringLiteral("Invalid Product Profile")),
                    platformText(QStringLiteral("platform.manager.duplicateId"),
                        QStringLiteral("A Product Profile with this ID already exists.")));
                return false;
            }
        }
        const RobotQtViewerResolvedProductComposition resolved =
            RobotQtViewerProductProfileResolver::resolve(m_catalog, available->profile);
        if(!resolved.succeeded()) {
            QMessageBox::critical(this,
                platformText(QStringLiteral("platform.dialog.invalidTitle"),
                    QStringLiteral("Invalid Product Profile")),
                resolved.diagnosticText());
            return false;
        }
        const QByteArray fileName =
            (available->profile.id + QStringLiteral(".platform.json")).toUtf8();
        const std::filesystem::path targetPath = available->newProfile
            ? m_profilesDirectory / std::filesystem::u8path(fileName.constData())
            : available->path;
        QString errorMessage;
        if(!RobotQtViewerProductProfileIo::saveProfile(
               targetPath, available->profile, &errorMessage)) {
            QMessageBox::critical(this,
                platformText(QStringLiteral("platform.manager.saveFailedTitle"),
                    QStringLiteral("Cannot Save Product Profile")),
                errorMessage);
            return false;
        }
        available->path = targetPath;
        available->newProfile = false;
        available->dirty = false;
        m_profileCombo->setItemText(m_loadedProfileIndex, available->profile.displayName);
        m_profileCombo->setItemData(m_loadedProfileIndex, available->profile.id);
        m_profileIdEdit->setReadOnly(true);
        updateCommandState();
        return true;
    }

    void RobotQtViewerProductProfileConfigurationDialog::activateSelection()
    {
        AvailableProfile* available = selectedProfile();
        if(available == nullptr) {
            return;
        }
        if((available->dirty || available->newProfile) && !saveCurrentProfile()) {
            return;
        }
        const RobotQtViewerResolvedProductComposition resolved =
            RobotQtViewerProductProfileResolver::resolve(m_catalog, available->profile);
        if(!resolved.succeeded()) {
            QMessageBox::critical(this,
                platformText(QStringLiteral("platform.dialog.invalidTitle"),
                    QStringLiteral("Invalid Product Profile")),
                resolved.diagnosticText());
            return;
        }
        m_activatedProfileId = available->profile.id;
        m_configurationChanged =
            m_activatedProfileId != m_currentProfileId || m_pendingRestartRequired;
        accept();
    }

    void RobotQtViewerProductProfileConfigurationDialog::updateCommandState()
    {
        const AvailableProfile* available = selectedProfile();
        const bool hasProfile = available != nullptr;
        m_duplicateProfileButton->setEnabled(hasProfile);
        m_deleteProfileButton->setEnabled(
            hasProfile && !available->builtInFallback &&
            available->profile.id != m_currentProfileId);
        m_saveProfileButton->setEnabled(
            hasProfile && !available->builtInFallback &&
            (available->dirty || available->newProfile));
        m_activateButton->setEnabled(hasProfile);
        m_activateButton->setText(m_pendingRestartRequired
            ? platformText(QStringLiteral("platform.manager.activateRestart"),
                QStringLiteral("Switch and Restart"))
            : platformText(QStringLiteral("platform.manager.confirm"),
                QStringLiteral("OK")));
    }

    QString RobotQtViewerProductProfileConfigurationDialog::makeUniqueProfileId(
        const QString& baseId) const
    {
        QString candidate = baseId;
        int suffix = 2;
        const auto exists = [this](const QString& id) {
            return std::any_of(m_profiles.cbegin(), m_profiles.cend(),
                [&id](const AvailableProfile& profile) {
                    return profile.profile.id == id;
                });
        };
        while(exists(candidate)) {
            candidate = QStringLiteral("%1-%2").arg(baseId).arg(suffix++);
        }
        return candidate;
    }

    RobotQtViewerProductProfileConfigurationDialog::AvailableProfile*
    RobotQtViewerProductProfileConfigurationDialog::selectedProfile()
    {
        const int index = m_loadedProfileIndex;
        return index >= 0 && index < m_profiles.size() ? &m_profiles[index] : nullptr;
    }

    const RobotQtViewerProductProfileConfigurationDialog::AvailableProfile*
    RobotQtViewerProductProfileConfigurationDialog::selectedProfile() const
    {
        const int index = m_loadedProfileIndex;
        return index >= 0 && index < m_profiles.size() ? &m_profiles[index] : nullptr;
    }
}
