#include "RobotQtViewerProductProfile.h"

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

        bool readStringList(
            const QJsonObject& object,
            const QString& key,
            QStringList* values,
            QString* errorMessage,
            bool required)
        {
            const QJsonValue value = object.value(key);
            if(value.isUndefined() || value.isNull()) {
                if(required && errorMessage != nullptr) {
                    *errorMessage = QStringLiteral("Product Profile field '%1' is missing.")
                        .arg(key);
                }
                if(values != nullptr) {
                    values->clear();
                }
                return !required;
            }
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
                            "Product Profile field '%1' contains an invalid stable ID.")
                            .arg(key);
                    }
                    return false;
                }
                if(!loaded.contains(id)) {
                    loaded.push_back(id);
                }
            }
            if(values != nullptr) {
                *values = loaded;
            }
            return true;
        }

        void appendUnique(QStringList* target, const QStringList& values)
        {
            if(target == nullptr) {
                return;
            }
            for(const QString& value : values) {
                if(!target->contains(value)) {
                    target->push_back(value);
                }
            }
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
            RobotQtViewerResolvedProductComposition& result,
            RobotQtViewerProductProfileDiagnosticSeverity severity,
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
            if(!path.parent_path().empty()) {
                std::filesystem::create_directories(path.parent_path(), directoryError);
                if(directoryError) {
                    if(errorMessage != nullptr) {
                        *errorMessage = QStringLiteral("Cannot create configuration directory: %1")
                            .arg(QString::fromStdString(directoryError.message()));
                    }
                    return false;
                }
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

    bool RobotQtViewerResolvedProductComposition::succeeded() const
    {
        return std::none_of(
            diagnostics.cbegin(),
            diagnostics.cend(),
            [](const RobotQtViewerProductProfileDiagnostic& diagnostic) {
                return diagnostic.severity == RobotQtViewerProductProfileDiagnosticSeverity::Error;
            });
    }

    bool RobotQtViewerResolvedProductComposition::containsWorkbench(const QString& workbenchId) const
    {
        return enabledWorkbenchIds.contains(workbenchId);
    }

    bool RobotQtViewerResolvedProductComposition::containsWorkbench(
        RobotQtViewerWorkbenchKind kind) const
    {
        return containsWorkbench(robotQtViewerWorkbenchId(kind));
    }

    QString RobotQtViewerResolvedProductComposition::diagnosticText() const
    {
        QStringList messages;
        for(const RobotQtViewerProductProfileDiagnostic& diagnostic : diagnostics) {
            messages.push_back(QStringLiteral("[%1] %2")
                .arg(diagnostic.code, diagnostic.message));
        }
        return messages.join(QLatin1Char('\n'));
    }

    RobotQtViewerResolvedProductComposition RobotQtViewerProductProfileResolver::resolve(
        const RobotQtViewerWorkbenchPackageRegistry& catalog,
        const RobotQtViewerProductProfile& profile)
    {
        RobotQtViewerResolvedProductComposition result;
        result.profileId = profile.id;
        result.displayName = profile.displayName;
        result.defaultWorkbenchId = profile.defaultWorkbenchId;

        if(profile.schema != QStringLiteral("smrobot.platform-profile") ||
            profile.version != kProfileSchemaVersion) {
            addDiagnostic(result,
                RobotQtViewerProductProfileDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.schema"),
                profile.id,
                QStringLiteral("Unsupported Product Profile schema or version."));
        }
        if(profile.displayName.trimmed().isEmpty()) {
            addDiagnostic(result,
                RobotQtViewerProductProfileDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.display_name"),
                profile.id,
                QStringLiteral("Product Profile display name is empty."));
        }
        if(!isValidStableId(profile.id)) {
            addDiagnostic(result,
                RobotQtViewerProductProfileDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.id"),
                profile.id,
                QStringLiteral("Product Profile has an invalid stable ID: %1").arg(profile.id));
        }
        if(!isValidStableId(profile.defaultWorkbenchId)) {
            addDiagnostic(result,
                RobotQtViewerProductProfileDiagnosticSeverity::Error,
                QStringLiteral("platform.default_workbench.id"),
                profile.defaultWorkbenchId,
                QStringLiteral("Default Workbench has an invalid stable ID: %1")
                    .arg(profile.defaultWorkbenchId));
        }

        std::set<QString> enabledWorkbenchIds;
        std::set<QString> explicitWorkbenchIds;
        QStringList orderedExplicitWorkbenchIds;
        for(const QString& workbenchId : profile.workbenchIds) {
            if(!isValidStableId(workbenchId)) {
                addDiagnostic(result,
                    RobotQtViewerProductProfileDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.id"),
                    workbenchId,
                    QStringLiteral("Product Profile contains an invalid Workbench ID: %1")
                        .arg(workbenchId));
                continue;
            }
            if(!explicitWorkbenchIds.insert(workbenchId).second) {
                addDiagnostic(result,
                    RobotQtViewerProductProfileDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.duplicate"),
                    workbenchId,
                    QStringLiteral("Product Profile contains a duplicate Workbench: %1")
                        .arg(workbenchId));
                continue;
            }
            orderedExplicitWorkbenchIds.push_back(workbenchId);
            enabledWorkbenchIds.insert(workbenchId);
        }
        if(orderedExplicitWorkbenchIds.isEmpty()) {
            addDiagnostic(result,
                RobotQtViewerProductProfileDiagnosticSeverity::Error,
                QStringLiteral("platform.workbench.empty"),
                profile.id,
                QStringLiteral("Product Profile must include at least one Workbench."));
        }

        for(const QString& featureId : profile.requiredFeatureIds) {
            const RobotQtViewerWorkbenchFeatureDesc* feature = catalog.feature(featureId);
            if(feature == nullptr) {
                addDiagnostic(result,
                    RobotQtViewerProductProfileDiagnosticSeverity::Error,
                    QStringLiteral("platform.feature.missing"),
                    featureId,
                    QStringLiteral("Required feature is not in the build catalog: %1")
                        .arg(featureId));
                continue;
            }
            for(const QString& workbenchId : feature->requiredModeIds) {
                if(isValidStableId(workbenchId)) {
                    enabledWorkbenchIds.insert(workbenchId);
                }
            }
        }

        std::set<QString> visiting;
        std::set<QString> expanded;
        std::function<bool(const QString&)> includeWorkbench;
        includeWorkbench = [&](const QString& workbenchId) {
            if(visiting.find(workbenchId) != visiting.end()) {
                addDiagnostic(result,
                    RobotQtViewerProductProfileDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.dependency_cycle"),
                    workbenchId,
                    QStringLiteral("Workbench dependency cycle contains: %1").arg(workbenchId));
                return false;
            }
            if(expanded.find(workbenchId) != expanded.end()) {
                return true;
            }
            visiting.insert(workbenchId);
            const RobotQtViewerWorkbenchDesc* workbench = catalog.registeredWorkbench(workbenchId);
            if(workbench == nullptr) {
                addDiagnostic(result,
                    RobotQtViewerProductProfileDiagnosticSeverity::Error,
                    QStringLiteral("platform.workbench.missing"),
                    workbenchId,
                    QStringLiteral("Workbench is not available in this build: %1")
                        .arg(workbenchId));
                visiting.erase(workbenchId);
                return false;
            }
            const RobotQtViewerWorkbenchPackageDesc* package = catalog.package(workbench->packageId);
            if(package == nullptr || !package->enabled ||
                package->extensionApiVersion != kWorkbenchExtensionApiVersion) {
                addDiagnostic(result,
                    RobotQtViewerProductProfileDiagnosticSeverity::Error,
                    QStringLiteral("platform.package.unavailable"),
                    workbench->packageId,
                    QStringLiteral("Workbench package is unavailable or incompatible: %1")
                        .arg(workbench->packageId));
                visiting.erase(workbenchId);
                return false;
            }
            for(const QString& packageDependencyId : package->requiredPackageIds) {
                if(!catalog.hasPackage(packageDependencyId)) {
                    addDiagnostic(result,
                        RobotQtViewerProductProfileDiagnosticSeverity::Error,
                        QStringLiteral("platform.package.dependency_missing"),
                        packageDependencyId,
                        QStringLiteral("Workbench package dependency is missing: %1 requires %2")
                            .arg(package->id, packageDependencyId));
                    visiting.erase(workbenchId);
                    return false;
                }
            }
            for(const QString& dependencyId : workbench->requiredWorkbenchIds) {
                enabledWorkbenchIds.insert(dependencyId);
                if(!includeWorkbench(dependencyId)) {
                    visiting.erase(workbenchId);
                    return false;
                }
            }
            visiting.erase(workbenchId);
            expanded.insert(workbenchId);
            return true;
        };

        const std::set<QString> requestedWorkbenchIds = enabledWorkbenchIds;
        for(const QString& workbenchId : requestedWorkbenchIds) {
            if(!includeWorkbench(workbenchId)) {
                enabledWorkbenchIds.erase(workbenchId);
            }
        }

        for(const QString& workbenchId : enabledWorkbenchIds) {
            const RobotQtViewerWorkbenchDesc* workbench = catalog.registeredWorkbench(workbenchId);
            if(workbench == nullptr) {
                continue;
            }
            for(const QString& conflictId : workbench->conflictsWithWorkbenchIds) {
                if(enabledWorkbenchIds.find(conflictId) != enabledWorkbenchIds.end() &&
                    workbenchId < conflictId) {
                    addDiagnostic(result,
                        RobotQtViewerProductProfileDiagnosticSeverity::Error,
                        QStringLiteral("platform.workbench.conflict"),
                        workbenchId,
                        QStringLiteral("Workbenches conflict: %1 and %2")
                            .arg(workbenchId, conflictId));
                }
            }
        }

        if(enabledWorkbenchIds.find(profile.defaultWorkbenchId) == enabledWorkbenchIds.end() ||
            catalog.registeredWorkbench(profile.defaultWorkbenchId) == nullptr) {
            addDiagnostic(result,
                RobotQtViewerProductProfileDiagnosticSeverity::Error,
                QStringLiteral("platform.default_workbench.unavailable"),
                profile.defaultWorkbenchId,
                QStringLiteral("Default Workbench must be included and available: %1")
                    .arg(profile.defaultWorkbenchId));
        }

        for(const QString& workbenchId : orderedExplicitWorkbenchIds) {
            if(enabledWorkbenchIds.find(workbenchId) != enabledWorkbenchIds.end()) {
                result.enabledWorkbenchIds.push_back(workbenchId);
            }
        }
        QVector<const RobotQtViewerWorkbenchDesc*> remaining;
        for(const QString& workbenchId : enabledWorkbenchIds) {
            if(result.enabledWorkbenchIds.contains(workbenchId)) {
                continue;
            }
            if(const RobotQtViewerWorkbenchDesc* workbench = catalog.registeredWorkbench(workbenchId)) {
                remaining.push_back(workbench);
            }
        }
        std::sort(remaining.begin(), remaining.end(),
            [](const RobotQtViewerWorkbenchDesc* lhs,
               const RobotQtViewerWorkbenchDesc* rhs) {
                if(lhs->defaultOrder != rhs->defaultOrder) {
                    return lhs->defaultOrder < rhs->defaultOrder;
                }
                return lhs->descriptor.id < rhs->descriptor.id;
            });
        for(const RobotQtViewerWorkbenchDesc* workbench : remaining) {
            result.enabledWorkbenchIds.push_back(workbench->descriptor.id);
        }

        return result;
    }

    RobotQtViewerProductProfile makeRobotQtViewerBuiltInBaseProductProfile(
        const RobotQtViewerWorkbenchPackageRegistry& catalog)
    {
        RobotQtViewerProductProfile profile;
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

    bool RobotQtViewerProductProfileIo::loadProfile(
        const std::filesystem::path& path,
        RobotQtViewerProductProfile* profile,
        QString* errorMessage)
    {
        if(profile == nullptr) {
            return false;
        }
        QJsonObject object;
        if(!readJsonObject(path, &object, errorMessage)) {
            return false;
        }
        const QString schema = object.value(QStringLiteral("schema")).toString();
        const int version = object.value(QStringLiteral("version")).toInt(-1);
        if(schema != QStringLiteral("smrobot.platform-profile") ||
            (version != 1 && version != kProfileSchemaVersion)) {
            if(errorMessage != nullptr) {
                *errorMessage = version == 1
                    ? QStringLiteral("Product Profile v1 must be migrated to v2: %1")
                        .arg(QString::fromStdWString(path.wstring()))
                    : QStringLiteral("Unsupported Product Profile schema or version: %1")
                        .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }

        RobotQtViewerProductProfile loaded;
        loaded.schema = schema;
        loaded.version = kProfileSchemaVersion;
        loaded.id = object.value(QStringLiteral("id")).toString().trimmed();
        loaded.displayName = object.value(QStringLiteral("displayName")).toString().trimmed();
        if(!readStringList(
               object, QStringLiteral("requiredFeatures"), &loaded.requiredFeatureIds,
               errorMessage, false)) {
            return false;
        }

        QStringList workbenchIds;
        QString defaultWorkbenchId;
        if(version == kProfileSchemaVersion) {
            if(!readStringList(
                   object, QStringLiteral("workbenches"), &workbenchIds, errorMessage, false)) {
                return false;
            }
            if(workbenchIds.isEmpty()) {
                if(!readStringList(
                       object, QStringLiteral("modeOrder"), &workbenchIds, errorMessage, false)) {
                    return false;
                }
            }
            defaultWorkbenchId =
                object.value(QStringLiteral("defaultWorkbench")).toString().trimmed();
            if(defaultWorkbenchId.isEmpty()) {
                defaultWorkbenchId = object.value(QStringLiteral("defaultMode")).toString().trimmed();
            }
        } else {
            QStringList requiredModes;
            QStringList defaultEnabledOptionalModes;
            QStringList modeOrder;
            if(!readStringList(
                   object, QStringLiteral("requiredModes"), &requiredModes, errorMessage, false) ||
                !readStringList(
                    object, QStringLiteral("defaultEnabledOptionalModes"),
                    &defaultEnabledOptionalModes, errorMessage, false) ||
                !readStringList(object, QStringLiteral("modeOrder"), &modeOrder, errorMessage, false)) {
                return false;
            }
            std::set<QString> selectedIds;
            for(const QString& workbenchId : requiredModes) {
                selectedIds.insert(workbenchId);
            }
            for(const QString& workbenchId : defaultEnabledOptionalModes) {
                selectedIds.insert(workbenchId);
            }
            defaultWorkbenchId = object.value(QStringLiteral("defaultMode")).toString().trimmed();
            if(isValidStableId(defaultWorkbenchId)) {
                selectedIds.insert(defaultWorkbenchId);
            }
            for(const QString& workbenchId : modeOrder) {
                if(selectedIds.find(workbenchId) != selectedIds.end()) {
                    workbenchIds.push_back(workbenchId);
                }
            }
            for(const QString& workbenchId : requiredModes) {
                if(!workbenchIds.contains(workbenchId)) {
                    workbenchIds.push_back(workbenchId);
                }
            }
            for(const QString& workbenchId : defaultEnabledOptionalModes) {
                if(!workbenchIds.contains(workbenchId)) {
                    workbenchIds.push_back(workbenchId);
                }
            }
            if(!workbenchIds.contains(defaultWorkbenchId) && isValidStableId(defaultWorkbenchId)) {
                workbenchIds.push_back(defaultWorkbenchId);
            }
        }

        if(loaded.schema.isEmpty() || !isValidStableId(loaded.id) ||
            loaded.displayName.isEmpty() || !isValidStableId(defaultWorkbenchId)) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Product Profile has missing or invalid fields: %1")
                    .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }
        loaded.workbenchIds = workbenchIds;
        if(loaded.workbenchIds.isEmpty()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Product Profile must include at least one Workbench: %1")
                    .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }
        if(!loaded.workbenchIds.contains(defaultWorkbenchId)) {
            loaded.workbenchIds.push_back(defaultWorkbenchId);
        }
        loaded.defaultWorkbenchId = defaultWorkbenchId;
        *profile = loaded;
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerProductProfileIo::saveProfile(
        const std::filesystem::path& path,
        const RobotQtViewerProductProfile& profile,
        QString* errorMessage)
    {
        std::set<QString> uniqueIds;
        bool validIds = !profile.workbenchIds.isEmpty();
        for(const QString& workbenchId : profile.workbenchIds) {
            validIds = validIds && isValidStableId(workbenchId) &&
                uniqueIds.insert(workbenchId).second;
        }
        std::set<QString> uniqueFeatures;
        for(const QString& featureId : profile.requiredFeatureIds) {
            validIds = validIds && isValidStableId(featureId) &&
                uniqueFeatures.insert(featureId).second;
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
        if(!profile.requiredFeatureIds.isEmpty()) {
            object.insert(QStringLiteral("requiredFeatures"), toJsonArray(profile.requiredFeatureIds));
        }
        return writeJsonObject(path, object, QStringLiteral("Product Profile"), errorMessage);
    }

    bool RobotQtViewerProductProfileIo::loadSelection(
        const std::filesystem::path& path,
        RobotQtViewerProductSelection* selection,
        QString* errorMessage)
    {
        if(selection == nullptr) {
            return false;
        }
        QJsonObject object;
        if(!readJsonObject(path, &object, errorMessage)) {
            return false;
        }
        RobotQtViewerProductSelection loaded;
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

    bool RobotQtViewerProductProfileIo::saveSelection(
        const std::filesystem::path& path,
        const RobotQtViewerProductSelection& selection,
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

    QVector<std::filesystem::path> RobotQtViewerProductProfileIo::discoverProfiles(
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
            const std::string name = entry.path().filename().generic_u8string();
            constexpr std::size_t suffixLength = 14;
            if(name.size() >= suffixLength &&
                name.compare(name.size() - suffixLength, suffixLength, ".platform.json") == 0) {
                profiles.push_back(entry.path());
            }
        }
        std::sort(profiles.begin(), profiles.end());
        return profiles;
    }
}
