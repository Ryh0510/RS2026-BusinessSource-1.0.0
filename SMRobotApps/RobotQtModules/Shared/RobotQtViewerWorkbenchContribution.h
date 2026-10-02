#pragma once

#include "RobotQtViewerEditSession.h"
#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerWorkbenchLifecycle.h"

#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <vector>

class QWidget;

namespace robot_qt_viewer
{
    class RobotQtViewerWorkbenchPackageRegistry;

    class RobotQtViewerWorkbenchRuntimeContribution
    {
    public:
        using PanelResolver = std::function<QWidget*()>;
        using PanelTitleResolver = std::function<QString(const QString&)>;

        virtual ~RobotQtViewerWorkbenchRuntimeContribution() = default;

        virtual QString workbenchId() const = 0;
        virtual QWidget* resolvePanel() const = 0;
        virtual QString resolvePanelTitle(const QString& defaultTitle) const;
        virtual IRobotQtViewerWorkbenchLifecycle* lifecycle() const = 0;
        virtual RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy() const = 0;
        virtual IRobotQtViewerLanguageParticipant* languageParticipant() const = 0;
        virtual IWorkbenchEditSession* editSession() const;
        virtual bool catalogOnly() const;
    };

    using RobotQtViewerWorkbenchRuntimeContributionFactory =
        std::function<std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>(
            QWidget* panelParent)>;

    struct RobotQtViewerWorkbenchRuntimeContributionFactoryDesc
    {
        QString workbenchId;
        RobotQtViewerWorkbenchRuntimeContributionFactory factory;
    };

    class RobotQtViewerBasicWorkbenchRuntimeContribution final
        : public RobotQtViewerWorkbenchRuntimeContribution
    {
    public:
        using Teardown = std::function<void()>;

        RobotQtViewerBasicWorkbenchRuntimeContribution(
            QString workbenchId,
            PanelResolver panelResolver,
            std::unique_ptr<IRobotQtViewerWorkbenchLifecycle> lifecycle,
            RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy,
            std::unique_ptr<IRobotQtViewerLanguageParticipant> languageParticipant,
            bool catalogOnly = false,
            PanelTitleResolver panelTitleResolver = {},
            Teardown teardown = {});
        RobotQtViewerBasicWorkbenchRuntimeContribution(
            QString workbenchId,
            PanelResolver panelResolver,
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy,
            IRobotQtViewerLanguageParticipant& languageParticipant,
            bool catalogOnly = false,
            PanelTitleResolver panelTitleResolver = {},
            Teardown teardown = {});
        ~RobotQtViewerBasicWorkbenchRuntimeContribution() override;

        QString workbenchId() const override;
        QWidget* resolvePanel() const override;
        QString resolvePanelTitle(const QString& defaultTitle) const override;
        IRobotQtViewerWorkbenchLifecycle* lifecycle() const override;
        RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy() const override;
        IRobotQtViewerLanguageParticipant* languageParticipant() const override;
        bool catalogOnly() const override;

    private:
        QString m_workbenchId;
        PanelResolver m_panelResolver;
        PanelTitleResolver m_panelTitleResolver;
        std::unique_ptr<IRobotQtViewerWorkbenchLifecycle> m_ownedLifecycle;
        std::unique_ptr<IRobotQtViewerLanguageParticipant> m_ownedLanguageParticipant;
        IRobotQtViewerWorkbenchLifecycle* m_lifecycle = nullptr;
        IRobotQtViewerLanguageParticipant* m_languageParticipant = nullptr;
        RobotQtViewerWorkbenchLifecyclePolicy m_lifecyclePolicy;
        bool m_catalogOnly = false;
        Teardown m_teardown;
    };

    class RobotQtViewerWorkbenchContributionHost
    {
    public:
        RobotQtViewerWorkbenchContributionHost(
            RobotQtViewerWorkbenchPackageRegistry& registry,
            RobotQtViewerEditSessionCoordinator& editSessionCoordinator);
        ~RobotQtViewerWorkbenchContributionHost();

        RobotQtViewerWorkbenchContributionHost(
            const RobotQtViewerWorkbenchContributionHost&) = delete;
        RobotQtViewerWorkbenchContributionHost& operator=(
            const RobotQtViewerWorkbenchContributionHost&) = delete;

        bool registerFactory(
            const RobotQtViewerWorkbenchRuntimeContributionFactoryDesc& factory,
            QString* errorMessage = nullptr);
        bool instantiateEnabled(
            const QStringList& enabledWorkbenchIds,
            QWidget* panelParent,
            QString* errorMessage = nullptr);
        QWidget* panelForWorkbench(const QString& workbenchId) const;
        QString panelTitleForWorkbench(
            const QString& workbenchId,
            const QString& defaultTitle) const;
        bool hasFactory(const QString& workbenchId) const;
        bool hasContribution(const QString& workbenchId) const;
        bool isCatalogOnly(const QString& workbenchId) const;
        QStringList instantiatedWorkbenchIds() const;
        void clear() noexcept;

    private:
        struct FactoryRecord
        {
            QString workbenchId;
            RobotQtViewerWorkbenchRuntimeContributionFactory factory;
        };

        RobotQtViewerWorkbenchPackageRegistry& m_registry;
        RobotQtViewerEditSessionCoordinator& m_editSessionCoordinator;
        std::vector<FactoryRecord> m_factories;
        std::vector<std::unique_ptr<RobotQtViewerWorkbenchRuntimeContribution>> m_contributions;
    };
}
