#include "RobotQtViewerPlatformProfile.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>
#include <functional>
#include <set>

namespace robot_qt_viewer
{
    namespace
    {
        constexpr int kProfileSchemaVersion = 2;
        constexpr int kSelectionSchemaVersion = 1;
        const QString kWorkbenchExtensionApiVersion = QStringLiteral("1");

        bool isValidStableId(const QString& id)
        {
            if(id.isEmpty()) {
                return false;
            }
            for(const QChar ch : id) {
                if(!(ch.isLower() || ch.isDigit() || ch == QLatin1Char('.') ||
                     ch == QLatin1Char('-') || ch == QLatin1Char('_'))) {
                    return false;
                }
            }
            return true;
        }

        bool readJsonObject(
            const std::filesystem::path& path,
            QJsonObject* object,
            QString* errorMessage)
        {
            QFile file(QString::fromStdWString(path.wstring()));
            if(!file.open(QIODevice::ReadOnly)) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Cannot open configuration file: %1")
                        .arg(file.fileName());
                }
                return false;
            }
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            if(parseError.error != QJsonParseError::NoError || !document.isObject()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Invalid JSON in %1: %2")
                        .arg(file.fileName(), parseError.errorString());
                }
                return false;
            }
            *object = document.object();
            return true;
        }

        bool readUniqueStringArray(
            const QJsonObject& object,
            const QString& key,
            QStringList* values,
            QString* errorMessage)
        {
            const QJsonValue value = object.value(key);
            if(!value.isArray()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Product Profile field '%1' must be an array.")
                        .arg(key);
                }
                return false;
            }
            QStringList loaded;
            for(const QJsonValue& item : value.toArray()) {
                const QString id = item.isString() ? item.toString().trimmed() : QString();
                if(!isValidStableId(id)) {
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral(
                            "Product Profile field '%1' contains an invalid Workbench ID.")
                            .arg(key);
                    }
                    return false;
                }
                if(loaded.contains(id)) {
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral(
                            "Product Profile field '%1' contains a duplicate Workbench ID: %2")
                            .arg(key, id);
                    }
                    return false;
                }
                loaded.push_back(id);
            }
            *values = loaded;
            return true;
        }

        QJsonArray toJsonArray(const QStringList& values)
        {
            QJsonArray array;
            for(const QString& value : values) {
                array.push_back(value);
            }
            return array;
        }

        void addDiagnostic(
            RobotQtViewerResolvedPlatformComposition& result,
            RobotQtViewerPlatformDiagnosticSeverity severity,
            const QString& code,
            const QString& subjectId,
            const QString& message)
        {
            result.diagnostics.push_back({ severity, code, subjectId, message });
        }

        bool writeJsonObject(
            const std::filesystem::path& path,
            const QJsonObject& object,
            const QString& description,
            QString* errorMessage)
        {
            std::error_code directoryError;
            std::filesystem::create_directories(path.parent_path(), directoryError);
            if(directoryError) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Cannot create configuration directory: %1")
                        .arg(QString::fromStdString(directoryError.message()));
                }
                return false;
            }
            QSaveFile file(QString::fromStdWString(path.wstring()));
            if(!file.open(QIODevice::WriteOnly) ||
                file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0 ||
                !file.commit()) {
                if(errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Cannot save %1: %2")
                        .arg(description, file.fileName());
                }
                return false;
            }
            if(errorMessage != nullptr) {
                errorMessage->clear();
            }
            return true;
        }
    }

    bool RobotQtViewerResolvedPlatformComposition::succeeded() const
    {
        return std::none_of(
            diagnostics.cbegin(), diagnostics.cend(),
            [](const RobotQtViewerPlatformDiagnostic& diagnostic) {
                return diagnostic.severity == RobotQtViewerPlatformDiagnosticSeverity::Error;
            });
    }

    bool RobotQtViewerResolvedPlatformComposition::containsWorkbench(const QString& workbenchId) const
    {
        return enabledWorkbenchIds.contains(workbenchId);
    }

    bool RobotQtViewerResolvedPlatformComposition::containsWorkbench(
        RobotQtViewerWorkbenchKind kind) const
    {
        return containsWorkbench(robotQtViewerWorkbenchId(kind));
    }

    QString RobotQtViewerResolvedPlatformComposition::diagnosticText() const
    {
        QStringList messages;
        for(const RobotQtViewerPlatformDiagnostic& diagnostic : diagnostics) {
            messages.push_back(QStringLiteral("[%1] %2")
                .arg(diagnostic.code, diagnostic.message));
        }
        return messages.join(QLatin1Char('\n'));
    }

    RobotQtViewerResolvedPlatformComposition RobotQtViewerPlatformProfileResolver::resolve(
        const RobotQtViewerWorkbenchPackageRegistry& catalog,
        const RobotQtViewerPlatformProfile& profile)
    {
        RobotQtViewerResolvedPlatformComposition result;
        result.profileId = profile.id;
        result.displayName = profile.displayName;
        result.defaultWorkbenchId = profile.defaultWorkbenchId;
        result.enabledWorkbenchIds = profile.workbenchIds;

        if(profile.schema != QStringLiteral("smrobot.platform-profile") ||
            profile.version != kProfileSchemaVersion) {
            addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.schema"), profile.id,
                QStringLiteral("Unsupported Product Profile schema. Version 2 is required."));
        }
        if(!isValidStableId(profile.id)) {
            addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.id"), profile.id,
                QStringLiteral("Product Profile has an invalid stable ID: %1").arg(profile.id));
        }
        if(profile.displayName.trimmed().isEmpty()) {
            addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.display_name"), profile.id,
                QStringLiteral("Product Profile display name is empty."));
        }
        if(profile.workbenchIds.isEmpty()) {
            addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.workbench.empty"), profile.id,
                QStringLiteral("Product Profile must include at least one Workbench."));
        }

        std::set<QString> declared;
        for(const QString& workbenchId : profile.workbenchIds) {
            if(!isValidStableId(workbenchId)) {
                addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.id"), workbenchId,
                    QStringLiteral("Product Profile contains an invalid Workbench ID: %1")
                        .arg(workbenchId));
                continue;
            }
            if(!declared.insert(workbenchId).second) {
                addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.duplicate"), workbenchId,
                    QStringLiteral("Product Profile contains a duplicate Workbench: %1")
                        .arg(workbenchId));
                continue;
            }
            const RobotQtViewerWorkbenchDesc* workbench = catalog.registeredWorkbench(workbenchId);
            if(workbench == nullptr) {
                addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.missing"), workbenchId,
                    QStringLiteral("Workbench is not available in this build: %1")
                        .arg(workbenchId));
                continue;
            }
            const RobotQtViewerWorkbenchPackageDesc* package = catalog.package(workbench->packageId);
            if(package == nullptr || !package->enabled ||
                package->extensionApiVersion != kWorkbenchExtensionApiVersion) {
                addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.package.unavailable"), workbench->packageId,
                    QStringLiteral("Workbench package is unavailable or incompatible: %1")
                        .arg(workbench->packageId));
                continue;
            }
            for(const QString& packageDependencyId : package->requiredPackageIds) {
                if(!catalog.hasPackage(packageDependencyId)) {
                    addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.package.dependency_missing"),
                        packageDependencyId,
                        QStringLiteral("Workbench package dependency is missing: %1 requires %2")
                            .arg(package->id, packageDependencyId));
                }
            }
        }

        for(const QString& workbenchId : profile.workbenchIds) {
            const RobotQtViewerWorkbenchDesc* workbench = catalog.registeredWorkbench(workbenchId);
            if(workbench == nullptr) {
                continue;
            }
            for(const QString& dependencyId : workbench->requiredWorkbenchIds) {
                if(!declared.count(dependencyId)) {
                    addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.workbench.dependency_not_declared"), workbenchId,
                        QStringLiteral("Workbench dependency must be listed explicitly: %1 requires %2")
                            .arg(workbenchId, dependencyId));
                }
            }
            for(const QString& conflictId : workbench->conflictsWithWorkbenchIds) {
                if(declared.count(conflictId) && workbenchId < conflictId) {
                    addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.workbench.conflict"), workbenchId,
                        QStringLiteral("Workbenches conflict: %1 and %2")
                            .arg(workbenchId, conflictId));
                }
            }
        }

        std::set<QString> visiting;
        std::set<QString> visited;
        std::function<void(const QString&)> visit = [&](const QString& workbenchId) {
            if(visited.count(workbenchId)) {
                return;
            }
            if(!visiting.insert(workbenchId).second) {
                addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.dependency_cycle"), workbenchId,
                    QStringLiteral("Workbench dependency cycle contains: %1").arg(workbenchId));
                return;
            }
            const RobotQtViewerWorkbenchDesc* workbench = catalog.registeredWorkbench(workbenchId);
            if(workbench != nullptr) {
                for(const QString& dependencyId : workbench->requiredWorkbenchIds) {
                    if(declared.count(dependencyId)) {
                        visit(dependencyId);
                    }
                }
            }
            visiting.erase(workbenchId);
            visited.insert(workbenchId);
        };
        for(const QString& workbenchId : profile.workbenchIds) {
            visit(workbenchId);
        }

        if(!isValidStableId(profile.defaultWorkbenchId) ||
            !declared.count(profile.defaultWorkbenchId) ||
            catalog.registeredWorkbench(profile.defaultWorkbenchId) == nullptr) {
            addDiagnostic(result, RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.default_workbench.unavailable"),
                profile.defaultWorkbenchId,
                QStringLiteral("Default Workbench must be included and available: %1")
                    .arg(profile.defaultWorkbenchId));
        }
        return result;
    }

    RobotQtViewerPlatformProfile makeRobotQtViewerBuiltInBaseProfile(
        const RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        RobotQtViewerPlatformProfile profile;
        profile.id = QStringLiteral("base-robot");
        profile.displayName = QStringLiteral(
            "General Robot Simulation Platform (Built-in Fallback)");
        profile.defaultWorkbenchId = robotQtViewerWorkbenchId(RobotQtViewerWorkbenchKind::Browse);

        QVector<const RobotQtViewerWorkbenchDesc*> workbenches;
        for(const RobotQtViewerWorkbenchDesc& workbench : catalog.workbenches()) {
            const RobotQtViewerWorkbenchPackageDesc* package = catalog.package(workbench.packageId);
            if(package != nullptr && package->enabled && !package->dynamicallyLoadable) {
                workbenches.push_back(&workbench);
            }
        }
        std::sort(workbenches.begin(), workbenches.end(),
            [](const RobotQtViewerWorkbenchDesc* left,
               const RobotQtViewerWorkbenchDesc* right) {
                if(left->defaultOrder != right->defaultOrder) {
                    return left->defaultOrder < right->defaultOrder;
                }
                return left->descriptor.id < right->descriptor.id;
            });
        for(const RobotQtViewerWorkbenchDesc* workbench : workbenches) {
            profile.workbenchIds.push_back(workbench->descriptor.id);
        }
        if(!profile.workbenchIds.contains(profile.defaultWorkbenchId) &&
            !profile.workbenchIds.isEmpty()) {
            profile.defaultWorkbenchId = profile.workbenchIds.front();
        }
        return profile;
    }

    bool RobotQtViewerPlatformProfileIo::loadProfile(
        const std::filesystem::path& path,
        RobotQtViewerPlatformProfile* profile,
        QString* errorMessage)
    {
        if(profile == nullptr) {
            return false;
        }
        QJsonObject object;
        if(!readJsonObject(path, &object, errorMessage)) {
            return false;
        }
        const int version = object.value(QStringLiteral("version")).toInt(-1);
        if(object.value(QStringLiteral("schema")).toString() !=
               QStringLiteral("smrobot.platform-profile") ||
            version != kProfileSchemaVersion) {
            if(errorMessage != nullptr) {
                *errorMessage = version == 1
                    ? QStringLiteral("Product Profile v1 must be migrated to v2: %1")
                        .arg(QString::fromStdWString(path.wstring()))
                    : QStringLiteral("Unsupported Product Profile schema or version: %1")
                        .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }

        RobotQtViewerPlatformProfile loaded;
        loaded.schema = object.value(QStringLiteral("schema")).toString();
        loaded.version = version;
        loaded.id = object.value(QStringLiteral("id")).toString().trimmed();
        loaded.displayName = object.value(QStringLiteral("displayName")).toString().trimmed();
        loaded.defaultWorkbenchId =
            object.value(QStringLiteral("defaultWorkbench")).toString().trimmed();
        if(!readUniqueStringArray(
               object, QStringLiteral("workbenches"), &loaded.workbenchIds, errorMessage) ||
            !isValidStableId(loaded.id) || loaded.displayName.isEmpty() ||
            !isValidStableId(loaded.defaultWorkbenchId) ||
            !loaded.workbenchIds.contains(loaded.defaultWorkbenchId)) {
            if(errorMessage != nullptr && errorMessage->isEmpty()) {
                *errorMessage = QStringLiteral("Product Profile has missing or invalid fields: %1")
                    .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }
        *profile = loaded;
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerPlatformProfileIo::saveProfile(
        const std::filesystem::path& path,
        const RobotQtViewerPlatformProfile& profile,
        QString* errorMessage)
    {
        std::set<QString> uniqueIds;
        bool validIds = !profile.workbenchIds.isEmpty();
        for(const QString& workbenchId : profile.workbenchIds) {
            validIds = validIds && isValidStableId(workbenchId) &&
                uniqueIds.insert(workbenchId).second;
        }
        if(profile.schema != QStringLiteral("smrobot.platform-profile") ||
            profile.version != kProfileSchemaVersion || !isValidStableId(profile.id) ||
            profile.displayName.trimmed().isEmpty() || !validIds ||
            !isValidStableId(profile.defaultWorkbenchId) ||
            !profile.workbenchIds.contains(profile.defaultWorkbenchId)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Cannot save an invalid Product Profile.");
            }
            return false;
        }
        QJsonObject object;
        object.insert(QStringLiteral("schema"), profile.schema);
        object.insert(QStringLiteral("version"), profile.version);
        object.insert(QStringLiteral("id"), profile.id);
        object.insert(QStringLiteral("displayName"), profile.displayName.trimmed());
        object.insert(QStringLiteral("workbenches"), toJsonArray(profile.workbenchIds));
        object.insert(QStringLiteral("defaultWorkbench"), profile.defaultWorkbenchId);
        return writeJsonObject(path, object, QStringLiteral("Product Profile"), errorMessage);
    }

    bool RobotQtViewerPlatformProfileIo::loadSelection(
        const std::filesystem::path& path,
        RobotQtViewerPlatformSelection* selection,
        QString* errorMessage)
    {
        if(selection == nullptr) {
            return false;
        }
        QJsonObject object;
        if(!readJsonObject(path, &object, errorMessage)) {
            return false;
        }
        RobotQtViewerPlatformSelection loaded;
        loaded.schema = object.value(QStringLiteral("schema")).toString();
        loaded.version = object.value(QStringLiteral("version")).toInt(-1);
        loaded.profileId = object.value(QStringLiteral("profileId")).toString().trimmed();
        if(loaded.schema != QStringLiteral("smrobot.platform-selection") ||
            loaded.version != kSelectionSchemaVersion || !isValidStableId(loaded.profileId)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Invalid simulation platform selection: %1")
                    .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }
        *selection = loaded;
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerPlatformProfileIo::saveSelection(
        const std::filesystem::path& path,
        const RobotQtViewerPlatformSelection& selection,
        QString* errorMessage)
    {
        if(selection.schema != QStringLiteral("smrobot.platform-selection") ||
            selection.version != kSelectionSchemaVersion || !isValidStableId(selection.profileId)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral(
                    "Cannot save an invalid simulation platform selection.");
            }
            return false;
        }
        QJsonObject object;
        object.insert(QStringLiteral("schema"), selection.schema);
        object.insert(QStringLiteral("version"), selection.version);
        object.insert(QStringLiteral("profileId"), selection.profileId);
        return writeJsonObject(
            path, object, QStringLiteral("simulation platform selection"), errorMessage);
    }

    QVector<std::filesystem::path> RobotQtViewerPlatformProfileIo::discoverProfiles(
        const std::filesystem::path& profilesDirectory)
    {
        QVector<std::filesystem::path> profiles;
        std::error_code error;
        if(!std::filesystem::is_directory(profilesDirectory, error)) {
            return profiles;
        }
        for(const std::filesystem::directory_entry& entry :
            std::filesystem::directory_iterator(profilesDirectory, error)) {
            if(error || !entry.is_regular_file()) {
                continue;
            }
            const std::string fileName = entry.path().filename().u8string();
            const std::string suffix = ".platform.json";
            if(fileName.size() >= suffix.size() &&
                fileName.compare(fileName.size() - suffix.size(), suffix.size(), suffix) == 0) {
                profiles.push_back(entry.path());
            }
        }
        std::sort(profiles.begin(), profiles.end());
        return profiles;
    }
}
