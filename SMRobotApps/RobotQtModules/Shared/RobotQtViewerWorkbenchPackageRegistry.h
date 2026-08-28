#pragma once

#include "RobotQtViewerWorkbench.h"
#include "RobotQtViewerWorkbenchLifecycle.h"
#include "RobotQtViewerLocalization.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace robot_qt_viewer
{
    enum class RobotQtViewerWorkbenchPackageSource
    {
        BuiltInSource,
        Prebuilt
    };

    struct RobotQtViewerWorkbenchPackageDesc
    {
        QString id;
        QString displayName;
        QString version;
        QString extensionApiVersion = QStringLiteral("1");
        RobotQtViewerWorkbenchPackageSource source =
            RobotQtViewerWorkbenchPackageSource::BuiltInSource;
        QStringList providedCapabilities;
        QStringList requiredPackageIds;
        bool dynamicallyLoadable = false;
        bool enabled = true;
    };

    struct RobotQtViewerWorkbenchModeDesc
    {
        QString packageId;
        RobotQtViewerWorkbenchDescriptor descriptor;
        QString toolbarActionId;
        QStringList featureIds;
        QStringList requiredModeIds;
        QStringList conflictsWithModeIds;
        QStringList requiredWorkbenchIds;
        QStringList conflictsWithWorkbenchIds;
        int defaultOrder = 0;
        bool enabled = true;
        IRobotQtViewerWorkbenchLifecycle* lifecycle = nullptr;
        RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy;
        IRobotQtViewerLanguageParticipant* languageParticipant = nullptr;
    };

    using RobotQtViewerWorkbenchDesc = RobotQtViewerWorkbenchModeDesc;

    struct RobotQtViewerWorkbenchFeatureDesc
    {
        QString id;
        QString displayName;
        QString packageId;
        QStringList requiredModeIds;
    };

    RobotQtViewerWorkbenchPackageDesc makeRobotQtViewerWorkbenchPackage(
        const QString& id,
        const QString& displayName,
        RobotQtViewerWorkbenchPackageSource source,
        const QString& version = QStringLiteral("0.1"));
    RobotQtViewerWorkbenchPackageDesc makeRobotQtViewerWorkbenchPackage(
        const QString& id,
        const QString& displayName,
        const QString& version = QStringLiteral("0.1"));
    RobotQtViewerWorkbenchModeDesc makeRobotQtViewerWorkbenchMode(
        const QString& packageId,
        RobotQtViewerWorkbenchKind kind,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds = {},
        const QStringList& requiredModeIds = {});
    RobotQtViewerWorkbenchDesc makeRobotQtViewerWorkbench(
        const QString& packageId,
        RobotQtViewerWorkbenchKind kind,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds = {},
        const QStringList& requiredWorkbenchIds = {});
    RobotQtViewerWorkbenchDesc makeRobotQtViewerWorkbench(
        const QString& packageId,
        const RobotQtViewerWorkbenchDescriptor& descriptor,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds = {},
        const QStringList& requiredWorkbenchIds = {});
    RobotQtViewerWorkbenchFeatureDesc makeRobotQtViewerWorkbenchFeature(
        const QString& id,
        const QString& displayName,
        const QString& packageId,
        const QStringList& requiredModeIds);

    class RobotQtViewerWorkbenchPackageRegistry
    {
    public:
        bool registerPackage(const RobotQtViewerWorkbenchPackageDesc& package);
        bool registerMode(const RobotQtViewerWorkbenchModeDesc& mode);
        bool registerWorkbench(const RobotQtViewerWorkbenchDesc& workbench);
        bool registerFeature(const RobotQtViewerWorkbenchFeatureDesc& feature);
        bool bindModeLifecycle(
            RobotQtViewerWorkbenchKind kind,
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchLifecyclePolicy& policy);
        bool bindWorkbenchLifecycle(
            RobotQtViewerWorkbenchKind kind,
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchLifecyclePolicy& policy);
        bool bindWorkbenchLifecycle(
            const QString& workbenchId,
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchLifecyclePolicy& policy);
        bool bindModeLanguageParticipant(
            RobotQtViewerWorkbenchKind kind,
            IRobotQtViewerLanguageParticipant& participant);
        bool bindWorkbenchLanguageParticipant(
            RobotQtViewerWorkbenchKind kind,
            IRobotQtViewerLanguageParticipant& participant);
        bool bindWorkbenchLanguageParticipant(
            const QString& workbenchId,
            IRobotQtViewerLanguageParticipant& participant);

        bool hasPackage(const QString& packageId) const;
        bool hasMode(RobotQtViewerWorkbenchKind kind) const;
        bool hasMode(const QString& modeId) const;
        bool hasWorkbench(RobotQtViewerWorkbenchKind kind) const;
        bool hasWorkbench(const QString& workbenchId) const;
        bool isModeReady(RobotQtViewerWorkbenchKind kind) const;
        bool isWorkbenchReady(RobotQtViewerWorkbenchKind kind) const;
        bool isWorkbenchReady(const QString& workbenchId) const;
        bool isModeLanguageReady(RobotQtViewerWorkbenchKind kind) const;
        bool isModeEnabled(RobotQtViewerWorkbenchKind kind) const;
        bool isWorkbenchEnabled(RobotQtViewerWorkbenchKind kind) const;
        bool isWorkbenchEnabled(const QString& workbenchId) const;
        bool validateEnabledModes(QString* errorMessage = nullptr) const;
        bool validateEnabledWorkbenches(QString* errorMessage = nullptr) const;
        bool validateEnabledModeLanguages(QString* errorMessage = nullptr) const;
        bool setEnabledModeIds(
            const QStringList& enabledModeIds,
            QString* errorMessage = nullptr);
        bool setEnabledWorkbenchIds(
            const QStringList& enabledWorkbenchIds,
            QString* errorMessage = nullptr);
        QString packageIdForMode(RobotQtViewerWorkbenchKind kind) const;
        QString packageIdForWorkbench(RobotQtViewerWorkbenchKind kind) const;
        QString packageIdForWorkbench(const QString& workbenchId) const;
        IRobotQtViewerWorkbenchLifecycle* lifecycle(RobotQtViewerWorkbenchKind kind) const;
        IRobotQtViewerWorkbenchLifecycle* lifecycle(const QString& workbenchId) const;
        IRobotQtViewerLanguageParticipant* languageParticipant(
            RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchLifecyclePolicy* lifecyclePolicy(
            RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchLifecyclePolicy* lifecyclePolicy(
            const QString& workbenchId) const;

        const RobotQtViewerWorkbenchPackageDesc* package(const QString& packageId) const;
        const RobotQtViewerWorkbenchModeDesc* mode(RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchModeDesc* mode(const QString& modeId) const;
        const RobotQtViewerWorkbenchModeDesc* registeredMode(const QString& modeId) const;
        const RobotQtViewerWorkbenchDesc* workbench(RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchDesc* workbench(const QString& workbenchId) const;
        const RobotQtViewerWorkbenchDesc* registeredWorkbench(const QString& workbenchId) const;
        const RobotQtViewerWorkbenchFeatureDesc* feature(const QString& featureId) const;
        const RobotQtViewerWorkbenchDescriptor* descriptor(RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchDescriptor* descriptor(const QString& workbenchId) const;

        const QVector<RobotQtViewerWorkbenchPackageDesc>& packages() const;
        const QVector<RobotQtViewerWorkbenchModeDesc>& modes() const;
        const QVector<RobotQtViewerWorkbenchDesc>& workbenches() const;
        const QVector<RobotQtViewerWorkbenchFeatureDesc>& features() const;

    private:
        QVector<RobotQtViewerWorkbenchPackageDesc> m_packages;
        QVector<RobotQtViewerWorkbenchModeDesc> m_modes;
        QVector<RobotQtViewerWorkbenchFeatureDesc> m_features;
    };

    RobotQtViewerWorkbenchPackageRegistry defaultRobotQtViewerWorkbenchPackageRegistry();
}
