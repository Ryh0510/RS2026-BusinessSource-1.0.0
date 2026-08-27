#include "RobotQtViewerWorkbenchPackageRegistry.h"

namespace robot_qt_viewer
{
    namespace
    {
        RobotQtViewerWorkbenchPackageDesc makePackage(
            const QString& packageId,
            const QString& displayName)
        {
            RobotQtViewerWorkbenchPackageDesc package;
            package.id = packageId;
            package.displayName = displayName;
            package.version = QStringLiteral("0.1");
            package.enabled = true;
            return package;
        }

        RobotQtViewerWorkbenchModeDesc makeMode(
            const QString& packageId,
            RobotQtViewerWorkbenchKind kind)
        {
            RobotQtViewerWorkbenchModeDesc mode;
            mode.packageId = packageId;
            mode.descriptor = robotQtViewerWorkbenchDescriptor(kind);
            mode.enabled = true;
            return mode;
        }
    }

    bool RobotQtViewerWorkbenchPackageRegistry::registerPackage(
        const RobotQtViewerWorkbenchPackageDesc& package)
    {
        if(package.id.isEmpty() || hasPackage(package.id)) {
            return false;
        }
        m_packages.push_back(package);
        return true;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::registerMode(
        const RobotQtViewerWorkbenchModeDesc& mode)
    {
        if(mode.packageId.isEmpty() || mode.descriptor.id.isEmpty() ||
            !hasPackage(mode.packageId) || registeredMode(mode.descriptor.id) != nullptr) {
            return false;
        }
        for(const RobotQtViewerWorkbenchModeDesc& registered : m_modes) {
            if(registered.descriptor.kind == mode.descriptor.kind) {
                return false;
            }
        }
        m_modes.push_back(mode);
        return true;
    }

    RobotQtViewerWorkbenchPackageDesc makeRobotQtViewerWorkbenchPackage(
        const QString& id,
        const QString& displayName,
        RobotQtViewerWorkbenchPackageSource source,
        const QString& version)
    {
        RobotQtViewerWorkbenchPackageDesc package;
        package.id = id;
        package.displayName = displayName;
        package.version = version;
        package.extensionApiVersion = QStringLiteral("1");
        package.source = source;
        return package;
    }

    RobotQtViewerWorkbenchModeDesc makeRobotQtViewerWorkbenchMode(
        const QString& packageId,
        RobotQtViewerWorkbenchKind kind,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds,
        const QStringList& requiredModeIds)
    {
        RobotQtViewerWorkbenchModeDesc mode;
        mode.packageId = packageId;
        mode.descriptor = robotQtViewerWorkbenchDescriptor(kind);
        mode.toolbarActionId = toolbarActionId;
        mode.defaultOrder = defaultOrder;
        mode.featureIds = featureIds;
        mode.requiredModeIds = requiredModeIds;
        return mode;
    }

    RobotQtViewerWorkbenchFeatureDesc makeRobotQtViewerWorkbenchFeature(
        const QString& id,
        const QString& displayName,
        const QString& packageId,
        const QStringList& requiredModeIds)
    {
        RobotQtViewerWorkbenchFeatureDesc feature;
        feature.id = id;
        feature.displayName = displayName;
        feature.packageId = packageId;
        feature.requiredModeIds = requiredModeIds;
        return feature;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::registerFeature(
        const RobotQtViewerWorkbenchFeatureDesc& featureDesc)
    {
        if(featureDesc.id.isEmpty() || featureDesc.packageId.isEmpty() ||
            !hasPackage(featureDesc.packageId) || feature(featureDesc.id) != nullptr) {
            return false;
        }
        m_features.push_back(featureDesc);
        return true;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::bindModeLifecycle(
        RobotQtViewerWorkbenchKind kind,
        IRobotQtViewerWorkbenchLifecycle& lifecycle,
        const RobotQtViewerWorkbenchLifecyclePolicy& policy)
    {
        for(RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            if(modeDesc.descriptor.kind != kind || modeDesc.lifecycle != nullptr) {
                continue;
            }
            modeDesc.lifecycle = &lifecycle;
            modeDesc.lifecyclePolicy = policy;
            return true;
        }
        return false;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::bindModeLanguageParticipant(
        RobotQtViewerWorkbenchKind kind,
        IRobotQtViewerLanguageParticipant& participant)
    {
        for(RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            if(modeDesc.descriptor.kind == kind && modeDesc.enabled &&
                hasPackage(modeDesc.packageId)) {
                modeDesc.languageParticipant = &participant;
                return true;
            }
        }
        return false;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::hasPackage(const QString& packageId) const
    {
        return package(packageId) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::hasMode(RobotQtViewerWorkbenchKind kind) const
    {
        return mode(kind) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::hasMode(const QString& modeId) const
    {
        return mode(modeId) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isModeReady(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc != nullptr && modeDesc->lifecycle != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isModeLanguageReady(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc != nullptr && modeDesc->languageParticipant != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isModeEnabled(
        RobotQtViewerWorkbenchKind kind) const
    {
        return mode(kind) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::setEnabledModeIds(
        const QStringList& enabledModeIds,
        QString* errorMessage)
    {
        for(const QString& modeId : enabledModeIds) {
            if(registeredMode(modeId) == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench mode is not in the build catalog: %1")
                        .arg(modeId);
                }
                return false;
            }
        }
        for(RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            modeDesc.enabled = enabledModeIds.contains(modeDesc.descriptor.id);
            if(!modeDesc.enabled) {
                modeDesc.lifecycle = nullptr;
                modeDesc.languageParticipant = nullptr;
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::validateEnabledModes(
        QString* errorMessage) const
    {
        for(const RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            if(!modeDesc.enabled || !hasPackage(modeDesc.packageId)) {
                continue;
            }
            if(modeDesc.lifecycle == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench mode has no lifecycle: %1 (%2)")
                        .arg(modeDesc.descriptor.id, modeDesc.packageId);
                }
                return false;
            }
            if(modeDesc.languageParticipant == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench mode has no language participant: %1 (%2)")
                        .arg(modeDesc.descriptor.id, modeDesc.packageId);
                }
                return false;
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::validateEnabledModeLanguages(
        QString* errorMessage) const
    {
        for(const RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            if(!modeDesc.enabled || !hasPackage(modeDesc.packageId)) {
                continue;
            }
            if(modeDesc.languageParticipant == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench mode has no language participant: %1 (%2)")
                        .arg(modeDesc.descriptor.id, modeDesc.packageId);
                }
                return false;
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    QString RobotQtViewerWorkbenchPackageRegistry::packageIdForMode(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc == nullptr ? QString() : modeDesc->packageId;
    }

    IRobotQtViewerWorkbenchLifecycle* RobotQtViewerWorkbenchPackageRegistry::lifecycle(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc == nullptr ? nullptr : modeDesc->lifecycle;
    }

    IRobotQtViewerLanguageParticipant*
    RobotQtViewerWorkbenchPackageRegistry::languageParticipant(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc == nullptr ? nullptr : modeDesc->languageParticipant;
    }

    const RobotQtViewerWorkbenchLifecyclePolicy*
    RobotQtViewerWorkbenchPackageRegistry::lifecyclePolicy(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc == nullptr || modeDesc->lifecycle == nullptr
            ? nullptr
            : &modeDesc->lifecyclePolicy;
    }

    const RobotQtViewerWorkbenchPackageDesc* RobotQtViewerWorkbenchPackageRegistry::package(
        const QString& packageId) const
    {
        for(const RobotQtViewerWorkbenchPackageDesc& packageDesc : m_packages) {
            if(packageDesc.id == packageId && packageDesc.enabled) {
                return &packageDesc;
            }
        }
        return nullptr;
    }

    const RobotQtViewerWorkbenchModeDesc* RobotQtViewerWorkbenchPackageRegistry::mode(
        RobotQtViewerWorkbenchKind kind) const
    {
        for(const RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            if(modeDesc.descriptor.kind == kind && modeDesc.enabled &&
                hasPackage(modeDesc.packageId)) {
                return &modeDesc;
            }
        }
        return nullptr;
    }

    const RobotQtViewerWorkbenchModeDesc* RobotQtViewerWorkbenchPackageRegistry::mode(
        const QString& modeId) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = registeredMode(modeId);
        return modeDesc != nullptr && modeDesc->enabled && hasPackage(modeDesc->packageId)
            ? modeDesc
            : nullptr;
    }

    const RobotQtViewerWorkbenchModeDesc*
    RobotQtViewerWorkbenchPackageRegistry::registeredMode(const QString& modeId) const
    {
        for(const RobotQtViewerWorkbenchModeDesc& modeDesc : m_modes) {
            if(modeDesc.descriptor.id == modeId) {
                return &modeDesc;
            }
        }
        return nullptr;
    }

    const RobotQtViewerWorkbenchFeatureDesc* RobotQtViewerWorkbenchPackageRegistry::feature(
        const QString& featureId) const
    {
        for(const RobotQtViewerWorkbenchFeatureDesc& featureDesc : m_features) {
            if(featureDesc.id == featureId) {
                return &featureDesc;
            }
        }
        return nullptr;
    }

    const RobotQtViewerWorkbenchDescriptor* RobotQtViewerWorkbenchPackageRegistry::descriptor(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchModeDesc* modeDesc = mode(kind);
        return modeDesc == nullptr ? nullptr : &modeDesc->descriptor;
    }

    const QVector<RobotQtViewerWorkbenchPackageDesc>&
    RobotQtViewerWorkbenchPackageRegistry::packages() const
    {
        return m_packages;
    }

    const QVector<RobotQtViewerWorkbenchModeDesc>&
    RobotQtViewerWorkbenchPackageRegistry::modes() const
    {
        return m_modes;
    }

    const QVector<RobotQtViewerWorkbenchFeatureDesc>&
    RobotQtViewerWorkbenchPackageRegistry::features() const
    {
        return m_features;
    }

    RobotQtViewerWorkbenchPackageRegistry defaultRobotQtViewerWorkbenchPackageRegistry()
    {
        RobotQtViewerWorkbenchPackageRegistry registry;

        const QString projectAssemblyPackageId = QStringLiteral("SMRobotWorkbenchProjectAssembly");
        const QString collisionConfigPackageId = QStringLiteral("SMRobotWorkbenchCollisionConfig");
        const QString robotRunPackageId = QStringLiteral("SMRobotWorkbenchRobotRun");
        const QString motionPlanningPackageId = QStringLiteral("SMRobotWorkbenchMotionPlanning");
        const QString sprayProcessPackageId = QStringLiteral("SMRobotWorkbenchSprayProcess");
        const QString paintingAnalysisPackageId = QStringLiteral("SMRobotWorkbenchPaintingAnalysis");
        const QString digitalTwinPackageId = QStringLiteral("SMRobotWorkbenchDigitalTwin");

        registry.registerPackage(makePackage(projectAssemblyPackageId, QStringLiteral("Project Assembly")));
        registry.registerPackage(makePackage(collisionConfigPackageId, QStringLiteral("Collision Config")));
        registry.registerPackage(makePackage(robotRunPackageId, QStringLiteral("Robot Run")));
        registry.registerPackage(makePackage(motionPlanningPackageId, QStringLiteral("Motion Planning")));
        registry.registerPackage(makePackage(sprayProcessPackageId, QStringLiteral("Spray Process")));
        registry.registerPackage(makePackage(paintingAnalysisPackageId, QStringLiteral("Painting Analysis")));
        registry.registerPackage(makePackage(digitalTwinPackageId, QStringLiteral("Digital Twin")));

        registry.registerMode(makeMode(projectAssemblyPackageId, RobotQtViewerWorkbenchKind::Browse));
        registry.registerMode(makeMode(robotRunPackageId, RobotQtViewerWorkbenchKind::Motion));
        registry.registerMode(makeMode(projectAssemblyPackageId, RobotQtViewerWorkbenchKind::ToolSetup));
        registry.registerMode(makeMode(collisionConfigPackageId, RobotQtViewerWorkbenchKind::Collision));
        registry.registerMode(makeMode(motionPlanningPackageId, RobotQtViewerWorkbenchKind::TrajectoryPlanning));
        registry.registerMode(makeMode(sprayProcessPackageId, RobotQtViewerWorkbenchKind::SprayProcess));
        registry.registerMode(makeMode(paintingAnalysisPackageId, RobotQtViewerWorkbenchKind::CoatingAnalysis));
        registry.registerMode(makeMode(digitalTwinPackageId, RobotQtViewerWorkbenchKind::DigitalTwin));

        return registry;
    }
}
