#include "RobotQtViewerWorkbenchContribution.h"

#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <algorithm>
#include <set>
#include <utility>

namespace robot_qt_viewer
{
    IWorkbenchEditSession* RobotQtViewerWorkbenchRuntimeContribution::editSession() const
    {
        return dynamic_cast<IWorkbenchEditSession*>(lifecycle());
    }

    QString RobotQtViewerWorkbenchRuntimeContribution::resolvePanelTitle(
        const QString& defaultTitle) const
    {
        return defaultTitle;
    }

    bool RobotQtViewerWorkbenchRuntimeContribution::catalogOnly() const
    {
        return false;
    }

    RobotQtViewerBasicWorkbenchRuntimeContribution::
        RobotQtViewerBasicWorkbenchRuntimeContribution(
            QString workbenchId,
            PanelResolver panelResolver,
            std::unique_ptr<IRobotQtViewerWorkbenchLifecycle> lifecycle,
            RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy,
            std::unique_ptr<IRobotQtViewerLanguageParticipant> languageParticipant,
            bool catalogOnly,
            PanelTitleResolver panelTitleResolver,
            Teardown teardown)
        : m_workbenchId(std::move(workbenchId))
        , m_panelResolver(std::move(panelResolver))
        , m_panelTitleResolver(std::move(panelTitleResolver))
        , m_ownedLifecycle(std::move(lifecycle))
        , m_ownedLanguageParticipant(std::move(languageParticipant))
        , m_lifecycle(m_ownedLifecycle.get())
        , m_languageParticipant(m_ownedLanguageParticipant.get())
        , m_lifecyclePolicy(lifecyclePolicy)
        , m_catalogOnly(catalogOnly)
        , m_teardown(std::move(teardown))
    {
    }

    RobotQtViewerBasicWorkbenchRuntimeContribution::
        RobotQtViewerBasicWorkbenchRuntimeContribution(
            QString workbenchId,
            PanelResolver panelResolver,
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy,
            IRobotQtViewerLanguageParticipant& languageParticipant,
            bool catalogOnly,
            PanelTitleResolver panelTitleResolver,
            Teardown teardown)
        : m_workbenchId(std::move(workbenchId))
        , m_panelResolver(std::move(panelResolver))
        , m_panelTitleResolver(std::move(panelTitleResolver))
        , m_lifecycle(&lifecycle)
        , m_languageParticipant(&languageParticipant)
        , m_lifecyclePolicy(lifecyclePolicy)
        , m_catalogOnly(catalogOnly)
        , m_teardown(std::move(teardown))
    {
    }

    RobotQtViewerBasicWorkbenchRuntimeContribution::~RobotQtViewerBasicWorkbenchRuntimeContribution()
    {
        if(m_teardown) {
            m_teardown();
        }
    }

    QString RobotQtViewerBasicWorkbenchRuntimeContribution::workbenchId() const
    {
        return m_workbenchId;
    }

    QWidget* RobotQtViewerBasicWorkbenchRuntimeContribution::resolvePanel() const
    {
        return m_panelResolver ? m_panelResolver() : nullptr;
    }

    QString RobotQtViewerBasicWorkbenchRuntimeContribution::resolvePanelTitle(
        const QString& defaultTitle) const
    {
        return m_panelTitleResolver ? m_panelTitleResolver(defaultTitle) : defaultTitle;
    }

    IRobotQtViewerWorkbenchLifecycle*
    RobotQtViewerBasicWorkbenchRuntimeContribution::lifecycle() const
    {
        return m_lifecycle;
    }

    RobotQtViewerWorkbenchLifecyclePolicy
    RobotQtViewerBasicWorkbenchRuntimeContribution::lifecyclePolicy() const
    {
        return m_lifecyclePolicy;
    }

    IRobotQtViewerLanguageParticipant*
    RobotQtViewerBasicWorkbenchRuntimeContribution::languageParticipant() const
    {
        return m_languageParticipant;
    }

    bool RobotQtViewerBasicWorkbenchRuntimeContribution::catalogOnly() const
    {
        return m_catalogOnly;
    }

    RobotQtViewerWorkbenchContributionHost::RobotQtViewerWorkbenchContributionHost(
        RobotQtViewerWorkbenchPackageRegistry& registry,
        RobotQtViewerEditSessionCoordinator& editSessionCoordinator)
        : m_registry(registry)
        , m_editSessionCoordinator(editSessionCoordinator)
    {
    }

    RobotQtViewerWorkbenchContributionHost::~RobotQtViewerWorkbenchContributionHost()
    {
        clear();
    }

    bool RobotQtViewerWorkbenchContributionHost::registerFactory(
        const RobotQtViewerWorkbenchRuntimeContributionFactoryDesc& factory,
        QString* errorMessage)
    {
        const QString workbenchId = factory.workbenchId.trimmed();
        if(workbenchId.isEmpty() || !factory.factory ||
            m_registry.registeredWorkbench(workbenchId) == nullptr || hasFactory(workbenchId)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Invalid or duplicate runtime contribution factory: %1")
                    .arg(workbenchId);
            }
            return false;
        }
        m_factories.push_back(FactoryRecord{ workbenchId, factory.factory });
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerWorkbenchContributionHost::instantiateEnabled(
        const QStringList& enabledWorkbenchIds,
        QWidget* panelParent,
        QString* errorMessage)
    {
        if(!m_contributions.empty()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Runtime contributions were already instantiated.");
            }
            return false;
        }

        std::vector<std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>> pending;
        pending.reserve(static_cast<std::size_t>(enabledWorkbenchIds.size()));
        std::set<QString> requestedWorkbenchIds;
        for(const QString& workbenchId : enabledWorkbenchIds) {
            if(!requestedWorkbenchIds.insert(workbenchId).second ||
                !m_registry.isWorkbenchEnabled(workbenchId)) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Duplicate or disabled runtime contribution: %1")
                        .arg(workbenchId);
                }
                return false;
            }
            const auto factory = std::find_if(
                m_factories.cbegin(),
                m_factories.cend(),
                [&workbenchId](const FactoryRecord& candidate) {
                    return candidate.workbenchId == workbenchId;
                });
            if(factory == m_factories.cend()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Missing runtime contribution factory: %1")
                        .arg(workbenchId);
                }
                return false;
            }
            std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution> contribution =
                factory->factory(panelParent);
            if(contribution == nullptr || contribution->workbenchId() != workbenchId ||
                contribution->lifecycle() == nullptr ||
                contribution->languageParticipant() == nullptr ||
                contribution->resolvePanel() == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Incomplete runtime contribution: %1")
                        .arg(workbenchId);
                }
                return false;
            }
            pending.push_back(std::move(contribution));
        }

        QStringList boundIds;
        std::vector<IWorkbenchEditSession*> registeredSessions;
        for(const auto& contribution : pending) {
            const QString workbenchId = contribution->workbenchId();
            if(!m_registry.bindWorkbenchLifecycle(
                    workbenchId,
                    *contribution->lifecycle(),
                    contribution->lifecyclePolicy()) ||
                !m_registry.bindWorkbenchLanguageParticipant(
                    workbenchId,
                    *contribution->languageParticipant())) {
                for(IWorkbenchEditSession* session : registeredSessions) {
                    m_editSessionCoordinator.unregisterSession(*session);
                }
                for(const QString& boundId : boundIds) {
                    m_registry.clearWorkbenchRuntimeBindings(boundId);
                }
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Failed to bind runtime contribution: %1")
                        .arg(workbenchId);
                }
                return false;
            }
            boundIds.push_back(workbenchId);
            if(IWorkbenchEditSession* session = contribution->editSession()) {
                if(!m_editSessionCoordinator.registerSession(*session)) {
                    for(IWorkbenchEditSession* registered : registeredSessions) {
                        m_editSessionCoordinator.unregisterSession(*registered);
                    }
                    for(const QString& boundId : boundIds) {
                        m_registry.clearWorkbenchRuntimeBindings(boundId);
                    }
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Failed to register edit session: %1")
                            .arg(workbenchId);
                    }
                    return false;
                }
                registeredSessions.push_back(session);
            }
        }

        m_contributions = std::move(pending);
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    QWidget* RobotQtViewerWorkbenchContributionHost::panelForWorkbench(
        const QString& workbenchId) const
    {
        for(const auto& contribution : m_contributions) {
            if(contribution->workbenchId() == workbenchId) {
                return contribution->resolvePanel();
            }
        }
        return nullptr;
    }

    QString RobotQtViewerWorkbenchContributionHost::panelTitleForWorkbench(
        const QString& workbenchId,
        const QString& defaultTitle) const
    {
        for(const auto& contribution : m_contributions) {
            if(contribution->workbenchId() == workbenchId) {
                return contribution->resolvePanelTitle(defaultTitle);
            }
        }
        return defaultTitle;
    }

    bool RobotQtViewerWorkbenchContributionHost::hasFactory(const QString& workbenchId) const
    {
        return std::any_of(
            m_factories.cbegin(),
            m_factories.cend(),
            [&workbenchId](const FactoryRecord& factory) {
                return factory.workbenchId == workbenchId;
            });
    }

    bool RobotQtViewerWorkbenchContributionHost::hasContribution(
        const QString& workbenchId) const
    {
        return std::any_of(
            m_contributions.cbegin(),
            m_contributions.cend(),
            [&workbenchId](const auto& contribution) {
                return contribution->workbenchId() == workbenchId;
            });
    }

    bool RobotQtViewerWorkbenchContributionHost::isCatalogOnly(
        const QString& workbenchId) const
    {
        for(const auto& contribution : m_contributions) {
            if(contribution->workbenchId() == workbenchId) {
                return contribution->catalogOnly();
            }
        }
        return false;
    }

    QStringList RobotQtViewerWorkbenchContributionHost::instantiatedWorkbenchIds() const
    {
        QStringList result;
        result.reserve(m_contributions.size());
        for(const auto& contribution : m_contributions) {
            result.push_back(contribution->workbenchId());
        }
        return result;
    }

    void RobotQtViewerWorkbenchContributionHost::clear() noexcept
    {
        for(auto it = m_contributions.rbegin(); it != m_contributions.rend(); ++it) {
            if(IWorkbenchEditSession* session = (*it)->editSession()) {
                m_editSessionCoordinator.unregisterSession(*session);
            }
            m_registry.clearWorkbenchRuntimeBindings((*it)->workbenchId());
        }
        m_contributions.clear();
    }
}
