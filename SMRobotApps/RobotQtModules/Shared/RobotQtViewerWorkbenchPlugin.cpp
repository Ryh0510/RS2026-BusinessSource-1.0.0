#include "RobotQtViewerWorkbenchPlugin.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>
#include <QtGlobal>

#include <algorithm>
#include <functional>
#include <map>
#include <set>

#ifndef SMROBOT_WORKBENCH_BUILD_CONFIGURATION
#define SMROBOT_WORKBENCH_BUILD_CONFIGURATION "Unknown"
#endif

#ifndef SMROBOT_WORKBENCH_MSVC_TOOLSET_VERSION
#define SMROBOT_WORKBENCH_MSVC_TOOLSET_VERSION ""
#endif

namespace robot_qt_viewer
{
    namespace
    {
        struct RuntimeFile
        {
            QString file;
            QString sha256;
        };

        struct Manifest
        {
            RobotQtViewerWorkbenchPackageDesc package;
            QVector<RobotQtViewerWorkbenchPluginModeDesc> modes;
            QString path;
            QString packageDirectory;
            QString binaryFile;
            QString binarySha256;
            QString buildConfiguration;
            QString msvcToolset;
            int qtMajor = 0;
            int qtMinor = 0;
            int pointerBits = 0;
            QVector<RuntimeFile> runtimeFiles;
        };

        QStringList stringList(const QJsonValue& value)
        {
            QStringList result;
            for(const QJsonValue& item : value.toArray()) {
                if(item.isString()) {
                    result.push_back(item.toString());
                }
            }
            return result;
        }

        bool isStableId(const QString& value)
        {
            if(value.isEmpty() || value.startsWith(QLatin1Char('.')) ||
                value.endsWith(QLatin1Char('.'))) {
                return false;
            }
            for(const QChar character : value) {
                if(!character.isLetterOrNumber() && character != QLatin1Char('.') &&
                    character != QLatin1Char('-') && character != QLatin1Char('_')) {
                    return false;
                }
            }
            return true;
        }

        bool secureRelativeFile(
            const QString& packageDirectory,
            const QString& relativeFile,
            QString* absolutePath)
        {
            if(relativeFile.isEmpty() || QDir::isAbsolutePath(relativeFile)) {
                return false;
            }
            const QString root = QFileInfo(packageDirectory).canonicalFilePath();
            const QString candidate = QFileInfo(
                QDir(packageDirectory).absoluteFilePath(relativeFile)).canonicalFilePath();
            if(root.isEmpty() || candidate.isEmpty()) {
                return false;
            }
#ifdef Q_OS_WIN
            const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
            const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
            const QString rootPrefix = QDir::cleanPath(root) + QLatin1Char('/');
            if(!QDir::cleanPath(candidate).startsWith(rootPrefix, sensitivity)) {
                return false;
            }
            if(absolutePath != nullptr) {
                *absolutePath = candidate;
            }
            return true;
        }

        QString fileSha256(const QString& path)
        {
            QFile file(path);
            if(!file.open(QIODevice::ReadOnly)) {
                return {};
            }
            QCryptographicHash hash(QCryptographicHash::Sha256);
            if(!hash.addData(&file)) {
                return {};
            }
            return QString::fromLatin1(hash.result().toHex());
        }

        bool verifyFile(
            const QString& packageDirectory,
            const QString& relativeFile,
            const QString& expectedSha256,
            QString* errorMessage)
        {
            QString absolutePath;
            if(expectedSha256.size() != 64 ||
                !secureRelativeFile(packageDirectory, relativeFile, &absolutePath)) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Unsafe or incomplete Workbench file declaration: %1")
                        .arg(relativeFile);
                }
                return false;
            }
            const QString actual = fileSha256(absolutePath);
            if(actual.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench file integrity check failed: %1")
                        .arg(relativeFile);
                }
                return false;
            }
            return true;
        }

        bool parseManifest(const QString& path, Manifest* manifest, QString* errorMessage)
        {
            QFile file(path);
            if(!file.open(QIODevice::ReadOnly)) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Cannot open Workbench manifest: %1").arg(path);
                }
                return false;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if(parseError.error != QJsonParseError::NoError || !document.isObject()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Invalid Workbench manifest JSON: %1 (%2)")
                        .arg(path, parseError.errorString());
                }
                return false;
            }
            const QJsonObject root = document.object();
            if(root.value(QStringLiteral("schema")).toString() !=
                    QStringLiteral("smrobot.workbench-package") ||
                root.value(QStringLiteral("version")).toInt() != 1) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Unsupported Workbench manifest schema: %1").arg(path);
                }
                return false;
            }

            Manifest parsed;
            parsed.path = QFileInfo(path).absoluteFilePath();
            parsed.packageDirectory = QFileInfo(path).absolutePath();
            const QJsonObject package = root.value(QStringLiteral("package")).toObject();
            parsed.package.id = package.value(QStringLiteral("id")).toString();
            parsed.package.displayName = package.value(QStringLiteral("displayName")).toString();
            parsed.package.version = package.value(QStringLiteral("version")).toString();
            parsed.package.extensionApiVersion =
                root.value(QStringLiteral("extensionApiVersion")).toString();
            parsed.package.requiredPackageIds =
                stringList(package.value(QStringLiteral("requiredPackageIds")));
            parsed.package.providedCapabilities =
                stringList(package.value(QStringLiteral("providedCapabilities")));
            parsed.package.dynamicallyLoadable = true;
            const QJsonObject binary = root.value(QStringLiteral("binary")).toObject();
            parsed.binaryFile = binary.value(QStringLiteral("file")).toString();
            parsed.binarySha256 = binary.value(QStringLiteral("sha256")).toString();
            const QJsonObject compatibility =
                root.value(QStringLiteral("compatibility")).toObject();
            parsed.qtMajor = compatibility.value(QStringLiteral("qtMajor")).toInt();
            parsed.qtMinor = compatibility.value(QStringLiteral("qtMinor")).toInt();
            parsed.pointerBits = compatibility.value(QStringLiteral("pointerBits")).toInt();
            parsed.buildConfiguration =
                compatibility.value(QStringLiteral("buildConfiguration")).toString();
            parsed.msvcToolset = compatibility.value(QStringLiteral("msvcToolset")).toString();

            for(const QJsonValue& value : root.value(QStringLiteral("modes")).toArray()) {
                const QJsonObject object = value.toObject();
                RobotQtViewerWorkbenchPluginModeDesc mode;
                mode.id = object.value(QStringLiteral("id")).toString();
                mode.displayName = object.value(QStringLiteral("displayName")).toString();
                mode.rightPanelTitle = object.value(QStringLiteral("rightPanelTitle")).toString();
                mode.toolbarActionId = object.value(QStringLiteral("toolbarActionId")).toString();
                mode.featureIds = stringList(object.value(QStringLiteral("featureIds")));
                mode.requiredModeIds = stringList(object.value(QStringLiteral("requiredModeIds")));
                mode.conflictsWithModeIds =
                    stringList(object.value(QStringLiteral("conflictsWithModeIds")));
                mode.defaultOrder = object.value(QStringLiteral("defaultOrder")).toInt();
                parsed.modes.push_back(mode);
            }
            for(const QJsonValue& value : root.value(QStringLiteral("runtimeFiles")).toArray()) {
                const QJsonObject object = value.toObject();
                parsed.runtimeFiles.push_back(RuntimeFile{
                    object.value(QStringLiteral("file")).toString(),
                    object.value(QStringLiteral("sha256")).toString() });
            }

            if(!isStableId(parsed.package.id) || parsed.package.displayName.isEmpty() ||
                parsed.package.version.isEmpty() ||
                parsed.package.extensionApiVersion != QStringLiteral("1") ||
                parsed.modes.isEmpty() || parsed.binaryFile.isEmpty() ||
                parsed.binarySha256.size() != 64) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench manifest is incomplete: %1").arg(path);
                }
                return false;
            }
            QStringList modeIds;
            for(const RobotQtViewerWorkbenchPluginModeDesc& mode : parsed.modes) {
                if(!isStableId(mode.id) || mode.displayName.isEmpty() || modeIds.contains(mode.id)) {
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Workbench manifest has an invalid or duplicate Workbench ID: %1")
                            .arg(mode.id);
                    }
                    return false;
                }
                modeIds.push_back(mode.id);
            }
            if(parsed.qtMajor != QT_VERSION_MAJOR || parsed.qtMinor != QT_VERSION_MINOR ||
                parsed.pointerBits != static_cast<int>(sizeof(void*) * 8) ||
                parsed.buildConfiguration !=
                    QString::fromLatin1(SMROBOT_WORKBENCH_BUILD_CONFIGURATION) ||
                (!parsed.msvcToolset.isEmpty() && parsed.msvcToolset !=
                    QString::fromLatin1(SMROBOT_WORKBENCH_MSVC_TOOLSET_VERSION))) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench binary compatibility mismatch: %1").arg(path);
                }
                return false;
            }
            *manifest = parsed;
            return true;
        }

        RobotQtViewerWorkbenchDescriptor registryDescriptor(
            const RobotQtViewerWorkbenchPluginModeDesc& mode)
        {
            RobotQtViewerWorkbenchDescriptor descriptor;
            descriptor.kind = RobotQtViewerWorkbenchKind::Browse;
            descriptor.domain = RobotQtViewerWorkbenchDomain::ProjectAssembly;
            descriptor.rightPanel = RobotQtViewerRightPanelKind::Status;
            descriptor.defaultViewportMode = RobotQtViewerViewportInteractionMode::Browse;
            descriptor.id = mode.id;
            descriptor.displayName = mode.displayName;
            descriptor.rightPanelTitle = mode.rightPanelTitle.isEmpty()
                ? mode.displayName
                : mode.rightPanelTitle;
            descriptor.projectAssemblyTreeProjection = false;
            descriptor.collisionTreeProjection = false;
            descriptor.projectAssemblyActions = false;
            descriptor.collisionActions = false;
            return descriptor;
        }

        bool sameMode(
            const RobotQtViewerWorkbenchPluginModeDesc& left,
            const RobotQtViewerWorkbenchPluginModeDesc& right)
        {
            return left.id == right.id && left.displayName == right.displayName &&
                left.rightPanelTitle == right.rightPanelTitle &&
                left.toolbarActionId == right.toolbarActionId &&
                left.featureIds == right.featureIds &&
                left.requiredModeIds == right.requiredModeIds &&
                left.conflictsWithModeIds == right.conflictsWithModeIds &&
                left.defaultOrder == right.defaultOrder;
        }
    }

    struct RobotQtViewerWorkbenchPluginLoader::Record
    {
        Manifest manifest;
        std::unique_ptr<QPluginLoader> loader;
        IRobotQtViewerWorkbenchPlugin* plugin = nullptr;
    };

    RobotQtViewerWorkbenchPluginLoader::RobotQtViewerWorkbenchPluginLoader() = default;
    RobotQtViewerWorkbenchPluginLoader::~RobotQtViewerWorkbenchPluginLoader() = default;

    bool RobotQtViewerWorkbenchPluginLoader::discover(
        const std::filesystem::path& workbenchesDirectory,
        RobotQtViewerWorkbenchPackageRegistry& registry,
        QStringList* diagnostics)
    {
        m_records.clear();
        const QString root = QString::fromStdWString(workbenchesDirectory.wstring());
        if(!QFileInfo::exists(root)) {
            return true;
        }
        QStringList manifests;
        QDirIterator iterator(
            root,
            QStringList{ QStringLiteral("*.workbench.json") },
            QDir::Files,
            QDirIterator::Subdirectories);
        while(iterator.hasNext()) {
            manifests.push_back(iterator.next());
        }
        std::sort(manifests.begin(), manifests.end());

        bool success = true;
        for(const QString& path : manifests) {
            Manifest manifest;
            QString error;
            if(!parseManifest(path, &manifest, &error)) {
                success = false;
                if(diagnostics != nullptr) {
                    diagnostics->push_back(error);
                }
                continue;
            }
            bool duplicate = registry.hasPackage(manifest.package.id);
            for(const RobotQtViewerWorkbenchPluginModeDesc& mode : manifest.modes) {
                duplicate = duplicate || registry.registeredWorkbench(mode.id) != nullptr;
            }
            if(duplicate || !registry.registerPackage(manifest.package)) {
                success = false;
                if(diagnostics != nullptr) {
                    diagnostics->push_back(QStringLiteral("Duplicate Workbench package or Workbench ID: %1")
                        .arg(manifest.package.id));
                }
                continue;
            }
            bool registered = true;
            for(const RobotQtViewerWorkbenchPluginModeDesc& mode : manifest.modes) {
                RobotQtViewerWorkbenchDesc registryWorkbench = makeRobotQtViewerWorkbench(
                    manifest.package.id,
                    registryDescriptor(mode),
                    mode.toolbarActionId,
                    mode.defaultOrder,
                    mode.featureIds,
                    mode.requiredModeIds);
                registryWorkbench.conflictsWithModeIds = mode.conflictsWithModeIds;
                registryWorkbench.conflictsWithWorkbenchIds = mode.conflictsWithModeIds;
                registered = registry.registerWorkbench(registryWorkbench) && registered;
            }
            if(!registered) {
                success = false;
                if(diagnostics != nullptr) {
                    diagnostics->push_back(QStringLiteral("Cannot register Workbench manifest: %1")
                        .arg(path));
                }
                continue;
            }
            auto record = std::make_unique<Record>();
            record->manifest = manifest;
            m_records.push_back(std::move(record));
        }
        return success;
    }

    bool RobotQtViewerWorkbenchPluginLoader::loadEnabled(
        const QStringList& enabledWorkbenchIds,
        RobotQtViewerWorkbenchPackageRegistry& registry,
        QString* errorMessage)
    {
        std::map<QString, Record*> recordsByPackage;
        for(const std::unique_ptr<Record>& record : m_records) {
            recordsByPackage.emplace(record->manifest.package.id, record.get());
        }

        std::set<QString> visiting;
        std::set<QString> visited;
        std::vector<Record*> loadOrder;
        std::function<bool(const QString&)> includePackage;
        includePackage = [&](const QString& packageId) {
            if(visited.find(packageId) != visited.end()) {
                return true;
            }
            if(visiting.find(packageId) != visiting.end()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench package dependency cycle contains: %1")
                        .arg(packageId);
                }
                return false;
            }
            const RobotQtViewerWorkbenchPackageDesc* package = registry.package(packageId);
            if(package == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench package dependency is missing: %1")
                        .arg(packageId);
                }
                return false;
            }
            visiting.insert(packageId);
            for(const QString& dependencyId : package->requiredPackageIds) {
                if(!includePackage(dependencyId)) {
                    return false;
                }
            }
            visiting.erase(packageId);
            visited.insert(packageId);
            const auto record = recordsByPackage.find(packageId);
            if(record != recordsByPackage.end()) {
                loadOrder.push_back(record->second);
            }
            return true;
        };

        for(const std::unique_ptr<Record>& record : m_records) {
            const bool hasEnabledMode = std::any_of(
                record->manifest.modes.cbegin(),
                record->manifest.modes.cend(),
                [&enabledWorkbenchIds](const RobotQtViewerWorkbenchPluginModeDesc& mode) {
                    return enabledWorkbenchIds.contains(mode.id);
                });
            if(hasEnabledMode && !includePackage(record->manifest.package.id)) {
                return false;
            }
        }

        for(Record* record : loadOrder) {
            QString error;
            if(!verifyFile(record->manifest.packageDirectory,
                   record->manifest.binaryFile,
                   record->manifest.binarySha256,
                   &error)) {
                if(errorMessage != nullptr) {
                    *errorMessage = error;
                }
                return false;
            }
            for(const RuntimeFile& file : record->manifest.runtimeFiles) {
                if(!verifyFile(record->manifest.packageDirectory, file.file, file.sha256, &error)) {
                    if(errorMessage != nullptr) {
                        *errorMessage = error;
                    }
                    return false;
                }
            }
            QString binaryPath;
            if(!secureRelativeFile(record->manifest.packageDirectory,
                   record->manifest.binaryFile, &binaryPath)) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Unsafe Workbench binary path.");
                }
                return false;
            }
            record->loader = std::make_unique<QPluginLoader>(binaryPath);
            QObject* instance = record->loader->instance();
            record->plugin = qobject_cast<IRobotQtViewerWorkbenchPlugin*>(instance);
            if(record->plugin == nullptr) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench plugin load failed: %1")
                        .arg(record->loader->errorString());
                }
                return false;
            }
            const RobotQtViewerWorkbenchPackageDesc pluginPackage =
                record->plugin->packageDescriptor();
            if(record->plugin->extensionApiVersion() != QStringLiteral("1") ||
                pluginPackage.id != record->manifest.package.id ||
                pluginPackage.displayName != record->manifest.package.displayName ||
                pluginPackage.version != record->manifest.package.version ||
                pluginPackage.extensionApiVersion !=
                    record->manifest.package.extensionApiVersion ||
                pluginPackage.providedCapabilities !=
                    record->manifest.package.providedCapabilities ||
                pluginPackage.requiredPackageIds !=
                    record->manifest.package.requiredPackageIds) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench plugin descriptor does not match its manifest: %1")
                        .arg(record->manifest.package.id);
                }
                return false;
            }
            const QVector<RobotQtViewerWorkbenchPluginModeDesc> pluginModes =
                record->plugin->modeDescriptors();
            if(pluginModes.size() != record->manifest.modes.size()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Workbench plugin Workbench list does not match its manifest: %1")
                        .arg(record->manifest.package.id);
                }
                return false;
            }
            for(const RobotQtViewerWorkbenchPluginModeDesc& manifestMode : record->manifest.modes) {
                const auto pluginMode = std::find_if(
                    pluginModes.cbegin(), pluginModes.cend(),
                    [&manifestMode](const RobotQtViewerWorkbenchPluginModeDesc& candidate) {
                        return candidate.id == manifestMode.id;
                    });
                if(pluginMode == pluginModes.cend() || !sameMode(manifestMode, *pluginMode)) {
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Workbench plugin Workbench descriptor mismatch: %1")
                            .arg(manifestMode.id);
                    }
                    return false;
                }
                if(!enabledWorkbenchIds.contains(manifestMode.id)) {
                    continue;
                }
                IRobotQtViewerWorkbenchLifecycle* lifecycle =
                    record->plugin->lifecycle(manifestMode.id);
                IRobotQtViewerLanguageParticipant* language =
                    record->plugin->languageParticipant(manifestMode.id);
                const RobotQtViewerWorkbenchLifecyclePolicy policy{
                    RobotQtViewerWorkbenchExecutionPolicy::MustQuiesce,
                    RobotQtViewerWorkbenchReactivationPolicy::PackageDefined };
                if(lifecycle == nullptr || language == nullptr ||
                    !registry.bindWorkbenchLifecycle(manifestMode.id, *lifecycle, policy) ||
                    !registry.bindWorkbenchLanguageParticipant(manifestMode.id, *language)) {
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Workbench plugin contribution is incomplete: %1")
                            .arg(manifestMode.id);
                    }
                    return false;
                }
            }
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    QStringList RobotQtViewerWorkbenchPluginLoader::discoveredWorkbenchIds() const
    {
        QStringList result;
        for(const std::unique_ptr<Record>& record : m_records) {
            for(const RobotQtViewerWorkbenchPluginModeDesc& mode : record->manifest.modes) {
                result.push_back(mode.id);
            }
        }
        return result;
    }

    QStringList RobotQtViewerWorkbenchPluginLoader::loadedWorkbenchIds() const
    {
        QStringList result;
        for(const std::unique_ptr<Record>& record : m_records) {
            if(record->plugin == nullptr) {
                continue;
            }
            for(const RobotQtViewerWorkbenchPluginModeDesc& mode : record->manifest.modes) {
                result.push_back(mode.id);
            }
        }
        return result;
    }

    QWidget* RobotQtViewerWorkbenchPluginLoader::createPanel(
        const QString& modeId,
        QWidget* parent) const
    {
        for(const std::unique_ptr<Record>& record : m_records) {
            if(record->plugin == nullptr) {
                continue;
            }
            const auto mode = std::find_if(
                record->manifest.modes.cbegin(), record->manifest.modes.cend(),
                [&modeId](const RobotQtViewerWorkbenchPluginModeDesc& candidate) {
                    return candidate.id == modeId;
                });
            if(mode != record->manifest.modes.cend()) {
                return record->plugin->createPanel(modeId, parent);
            }
        }
        return nullptr;
    }

    bool RobotQtViewerWorkbenchPluginLoader::ownsWorkbench(const QString& modeId) const
    {
        return discoveredWorkbenchIds().contains(modeId);
    }
}
