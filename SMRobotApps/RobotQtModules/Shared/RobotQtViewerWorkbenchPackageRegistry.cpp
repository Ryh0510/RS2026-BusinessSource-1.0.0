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

        RobotQtViewerWorkbenchDesc makeWorkbench(
            const QString& packageId,
            RobotQtViewerWorkbenchKind kind)
        {
            RobotQtViewerWorkbenchDesc workbench;
            workbench.packageId = packageId;
            workbench.descriptor = robotQtViewerWorkbenchDescriptor(kind);
            workbench.enabled = true;
            return workbench;
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

    bool RobotQtViewerWorkbenchPackageRegistry::registerWorkbench(
        const RobotQtViewerWorkbenchDesc& workbench)
    {
        if(workbench.packageId.isEmpty() || workbench.descriptor.id.isEmpty() ||
            !hasPackage(workbench.packageId) || registeredWorkbench(workbench.descriptor.id) != nullptr) {
            return false;
        }
        m_workbenches.push_back(workbench);
        return true;
    }

    RobotQtViewerWorkbenchPackageDesc makeRobotQtViewerWorkbenchPackage(
        const QString& id,
        const QString& displayName,
        const QString& version)
    {
        RobotQtViewerWorkbenchPackageDesc package;
        package.id = id;
        package.displayName = displayName;
        package.version = version;
        package.extensionApiVersion = QStringLiteral("1");
        return package;
    }

    RobotQtViewerWorkbenchDesc makeRobotQtViewerWorkbench(
        const QString& packageId,
        RobotQtViewerWorkbenchKind kind,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds,
        const QStringList& requiredWorkbenchIds)
    {
        return makeRobotQtViewerWorkbench(
            packageId,
            robotQtViewerWorkbenchDescriptor(kind),
            toolbarActionId,
            defaultOrder,
            featureIds,
            requiredWorkbenchIds);
    }

    RobotQtViewerWorkbenchDesc makeRobotQtViewerWorkbench(
        const QString& packageId,
        const RobotQtViewerWorkbenchDescriptor& descriptor,
        const QString& toolbarActionId,
        int defaultOrder,
        const QStringList& featureIds,
        const QStringList& requiredWorkbenchIds)
    {
        RobotQtViewerWorkbenchDesc workbench;
        workbench.packageId = packageId;
        workbench.descriptor = descriptor;
        workbench.toolbarActionId = toolbarActionId;
        workbench.defaultOrder = defaultOrder;
        workbench.featureIds = featureIds;
        workbench.requiredWorkbenchIds = requiredWorkbenchIds;
        return workbench;
    }

    RobotQtViewerWorkbenchFeatureDesc makeRobotQtViewerWorkbenchFeature(
        const QString& id,
        const QString& displayName,
        const QString& packageId,
        const QStringList& requiredWorkbenchIds)
    {
        RobotQtViewerWorkbenchFeatureDesc feature;
        feature.id = id;
        feature.displayName = displayName;
        feature.packageId = packageId;
        feature.requiredWorkbenchIds = requiredWorkbenchIds;
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

    bool RobotQtViewerWorkbenchPackageRegistry::bindWorkbenchLifecycle(
        RobotQtViewerWorkbenchKind kind,
        IRobotQtViewerWorkbenchLifecycle& lifecycle,
        const RobotQtViewerWorkbenchLifecyclePolicy& policy)
    {
        return bindWorkbenchLifecycle(robotQtViewerWorkbenchId(kind), lifecycle, policy);
    }

    bool RobotQtViewerWorkbenchPackageRegistry::bindWorkbenchLifecycle(
        const QString& workbenchId,
        IRobotQtViewerWorkbenchLifecycle& lifecycle,
        const RobotQtViewerWorkbenchLifecyclePolicy& policy)
    {
        for(RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(workbenchDesc.descriptor.id != workbenchId || workbenchDesc.lifecycle != nullptr) {
                continue;
            }
            workbenchDesc.lifecycle = &lifecycle;
            workbenchDesc.lifecyclePolicy = policy;
            return true;
        }
        return false;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::bindWorkbenchLanguageParticipant(
        RobotQtViewerWorkbenchKind kind,
        IRobotQtViewerLanguageParticipant& participant)
    {
        return bindWorkbenchLanguageParticipant(robotQtViewerWorkbenchId(kind), participant);
    }

    bool RobotQtViewerWorkbenchPackageRegistry::bindWorkbenchLanguageParticipant(
        const QString& workbenchId,
        IRobotQtViewerLanguageParticipant& participant)
    {
        for(RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(workbenchDesc.descriptor.id == workbenchId && workbenchDesc.enabled &&
                hasPackage(workbenchDesc.packageId)) {
                workbenchDesc.languageParticipant = &participant;
                return true;
            }
        }
        return false;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::clearWorkbenchRuntimeBindings(
        const QString& workbenchId)
    {
        for(RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(workbenchDesc.descriptor.id != workbenchId) {
                continue;
            }
            workbenchDesc.lifecycle = nullptr;
            workbenchDesc.lifecyclePolicy = {};
            workbenchDesc.languageParticipant = nullptr;
            return true;
        }
        return false;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::hasPackage(const QString& packageId) const
    {
        return package(packageId) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::hasWorkbench(RobotQtViewerWorkbenchKind kind) const
    {
        return workbench(kind) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::hasWorkbench(const QString& workbenchId) const
    {
        return workbench(workbenchId) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isWorkbenchReady(
        RobotQtViewerWorkbenchKind kind) const
    {
        return isWorkbenchReady(robotQtViewerWorkbenchId(kind));
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isWorkbenchReady(
        const QString& workbenchId) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(workbenchId);
        return workbenchDesc != nullptr && workbenchDesc->lifecycle != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isWorkbenchLanguageReady(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(kind);
        return workbenchDesc != nullptr && workbenchDesc->languageParticipant != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isWorkbenchEnabled(
        RobotQtViewerWorkbenchKind kind) const
    {
        return isWorkbenchEnabled(robotQtViewerWorkbenchId(kind));
    }

    bool RobotQtViewerWorkbenchPackageRegistry::isWorkbenchEnabled(
        const QString& workbenchId) const
    {
        return workbench(workbenchId) != nullptr;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::setEnabledWorkbenchIds(
        const QStringList& enabledWorkbenchIds,
        QString* errorMessage)
    {
        for(const QString& workbenchId : enabledWorkbenchIds) {
            if(registeredWorkbench(workbenchId) == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench is not in the build catalog: %1")
                        .arg(workbenchId);
                }
                return false;
            }
        }
        for(RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            workbenchDesc.enabled = enabledWorkbenchIds.contains(workbenchDesc.descriptor.id);
            if(!workbenchDesc.enabled) {
                workbenchDesc.lifecycle = nullptr;
                workbenchDesc.languageParticipant = nullptr;
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::validateEnabledWorkbenches(
        QString* errorMessage) const
    {
        for(const RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(!workbenchDesc.enabled || !hasPackage(workbenchDesc.packageId)) {
                continue;
            }
            if(workbenchDesc.lifecycle == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench has no lifecycle: %1 (%2)")
                        .arg(workbenchDesc.descriptor.id, workbenchDesc.packageId);
                }
                return false;
            }
            if(workbenchDesc.languageParticipant == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench has no language participant: %1 (%2)")
                        .arg(workbenchDesc.descriptor.id, workbenchDesc.packageId);
                }
                return false;
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerWorkbenchPackageRegistry::validateEnabledWorkbenchLanguages(
        QString* errorMessage) const
    {
        for(const RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(!workbenchDesc.enabled || !hasPackage(workbenchDesc.packageId)) {
                continue;
            }
            if(workbenchDesc.languageParticipant == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench has no language participant: %1 (%2)")
                        .arg(workbenchDesc.descriptor.id, workbenchDesc.packageId);
                }
                return false;
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    QString RobotQtViewerWorkbenchPackageRegistry::packageIdForWorkbench(
        RobotQtViewerWorkbenchKind kind) const
    {
        return packageIdForWorkbench(robotQtViewerWorkbenchId(kind));
    }

    QString RobotQtViewerWorkbenchPackageRegistry::packageIdForWorkbench(
        const QString& workbenchId) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(workbenchId);
        return workbenchDesc == nullptr ? QString() : workbenchDesc->packageId;
    }

    IRobotQtViewerWorkbenchLifecycle* RobotQtViewerWorkbenchPackageRegistry::lifecycle(
        RobotQtViewerWorkbenchKind kind) const
    {
        return lifecycle(robotQtViewerWorkbenchId(kind));
    }

    IRobotQtViewerWorkbenchLifecycle* RobotQtViewerWorkbenchPackageRegistry::lifecycle(
        const QString& workbenchId) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(workbenchId);
        return workbenchDesc == nullptr ? nullptr : workbenchDesc->lifecycle;
    }

    IRobotQtViewerLanguageParticipant*
    RobotQtViewerWorkbenchPackageRegistry::languageParticipant(
        RobotQtViewerWorkbenchKind kind) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(kind);
        return workbenchDesc == nullptr ? nullptr : workbenchDesc->languageParticipant;
    }

    const RobotQtViewerWorkbenchLifecyclePolicy*
    RobotQtViewerWorkbenchPackageRegistry::lifecyclePolicy(
        RobotQtViewerWorkbenchKind kind) const
    {
        return lifecyclePolicy(robotQtViewerWorkbenchId(kind));
    }

    const RobotQtViewerWorkbenchLifecyclePolicy*
    RobotQtViewerWorkbenchPackageRegistry::lifecyclePolicy(
        const QString& workbenchId) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(workbenchId);
        return workbenchDesc == nullptr || workbenchDesc->lifecycle == nullptr
            ? nullptr
            : &workbenchDesc->lifecyclePolicy;
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

    const RobotQtViewerWorkbenchDesc* RobotQtViewerWorkbenchPackageRegistry::workbench(
        RobotQtViewerWorkbenchKind kind) const
    {
        for(const RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(workbenchDesc.descriptor.kind == kind && workbenchDesc.enabled &&
                hasPackage(workbenchDesc.packageId)) {
                return &workbenchDesc;
            }
        }
        return nullptr;
    }

    const RobotQtViewerWorkbenchDesc* RobotQtViewerWorkbenchPackageRegistry::workbench(
        const QString& workbenchId) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = registeredWorkbench(workbenchId);
        return workbenchDesc != nullptr && workbenchDesc->enabled && hasPackage(workbenchDesc->packageId)
            ? workbenchDesc
            : nullptr;
    }

    const RobotQtViewerWorkbenchDesc*
    RobotQtViewerWorkbenchPackageRegistry::registeredWorkbench(const QString& workbenchId) const
    {
        for(const RobotQtViewerWorkbenchDesc& workbenchDesc : m_workbenches) {
            if(workbenchDesc.descriptor.id == workbenchId) {
                return &workbenchDesc;
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
        return descriptor(robotQtViewerWorkbenchId(kind));
    }

    const RobotQtViewerWorkbenchDescriptor* RobotQtViewerWorkbenchPackageRegistry::descriptor(
        const QString& workbenchId) const
    {
        const RobotQtViewerWorkbenchDesc* workbenchDesc = workbench(workbenchId);
        return workbenchDesc == nullptr ? nullptr : &workbenchDesc->descriptor;
    }

    const QVector<RobotQtViewerWorkbenchPackageDesc>&
    RobotQtViewerWorkbenchPackageRegistry::packages() const
    {
        return m_packages;
    }

    const QVector<RobotQtViewerWorkbenchDesc>&
    RobotQtViewerWorkbenchPackageRegistry::workbenches() const
    {
        return m_workbenches;
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

        registry.registerWorkbench(makeWorkbench(projectAssemblyPackageId, RobotQtViewerWorkbenchKind::Browse));
        registry.registerWorkbench(makeWorkbench(robotRunPackageId, RobotQtViewerWorkbenchKind::Motion));
        registry.registerWorkbench(makeWorkbench(projectAssemblyPackageId, RobotQtViewerWorkbenchKind::ToolSetup));
        registry.registerWorkbench(makeWorkbench(collisionConfigPackageId, RobotQtViewerWorkbenchKind::Collision));
        registry.registerWorkbench(makeWorkbench(motionPlanningPackageId, RobotQtViewerWorkbenchKind::TrajectoryPlanning));
        registry.registerWorkbench(makeWorkbench(sprayProcessPackageId, RobotQtViewerWorkbenchKind::SprayProcess));
        registry.registerWorkbench(makeWorkbench(paintingAnalysisPackageId, RobotQtViewerWorkbenchKind::CoatingAnalysis));
        registry.registerWorkbench(makeWorkbench(digitalTwinPackageId, RobotQtViewerWorkbenchKind::DigitalTwin));

        return registry;
    }
}
