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
        int defaultOrder = 0;
        bool enabled = true;
        IRobotQtViewerWorkbenchLifecycle* lifecycle = nullptr;
        RobotQtViewerWorkbenchLifecyclePolicy lifecyclePolicy;
        IRobotQtViewerLanguageParticipant* languageParticipant = nullptr;
    };

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
    RobotQtViewerWorkbenchModeDesc makeRobotQtViewerWorkbenchMode(
        const QString& packageId,
        RobotQtViewerWorkbenchKind kind,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds = {},
        const QStringList& requiredModeIds = {});
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
        bool registerFeature(const RobotQtViewerWorkbenchFeatureDesc& feature);
        bool bindModeLifecycle(
            RobotQtViewerWorkbenchKind kind,
            IRobotQtViewerWorkbenchLifecycle& lifecycle,
            const RobotQtViewerWorkbenchLifecyclePolicy& policy);
        bool bindModeLanguageParticipant(
            RobotQtViewerWorkbenchKind kind,
            IRobotQtViewerLanguageParticipant& participant);

        bool hasPackage(const QString& packageId) const;
        bool hasMode(RobotQtViewerWorkbenchKind kind) const;
        bool hasMode(const QString& modeId) const;
        bool isModeReady(RobotQtViewerWorkbenchKind kind) const;
        bool isModeLanguageReady(RobotQtViewerWorkbenchKind kind) const;
        bool isModeEnabled(RobotQtViewerWorkbenchKind kind) const;
        bool validateEnabledModes(QString* errorMessage = nullptr) const;
        bool validateEnabledModeLanguages(QString* errorMessage = nullptr) const;
        bool setEnabledModeIds(
            const QStringList& enabledModeIds,
            QString* errorMessage = nullptr);
        QString packageIdForMode(RobotQtViewerWorkbenchKind kind) const;
        IRobotQtViewerWorkbenchLifecycle* lifecycle(RobotQtViewerWorkbenchKind kind) const;
        IRobotQtViewerLanguageParticipant* languageParticipant(
            RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchLifecyclePolicy* lifecyclePolicy(
            RobotQtViewerWorkbenchKind kind) const;

        const RobotQtViewerWorkbenchPackageDesc* package(const QString& packageId) const;
        const RobotQtViewerWorkbenchModeDesc* mode(RobotQtViewerWorkbenchKind kind) const;
        const RobotQtViewerWorkbenchModeDesc* mode(const QString& modeId) const;
        const RobotQtViewerWorkbenchModeDesc* registeredMode(const QString& modeId) const;
        const RobotQtViewerWorkbenchFeatureDesc* feature(const QString& featureId) const;
        const RobotQtViewerWorkbenchDescriptor* descriptor(RobotQtViewerWorkbenchKind kind) const;

        const QVector<RobotQtViewerWorkbenchPackageDesc>& packages() const;
        const QVector<RobotQtViewerWorkbenchModeDesc>& modes() const;
        const QVector<RobotQtViewerWorkbenchFeatureDesc>& features() const;

    private:
        QVector<RobotQtViewerWorkbenchPackageDesc> m_packages;
        QVector<RobotQtViewerWorkbenchModeDesc> m_modes;
        QVector<RobotQtViewerWorkbenchFeatureDesc> m_features;
    };

    RobotQtViewerWorkbenchPackageRegistry defaultRobotQtViewerWorkbenchPackageRegistry();
}
