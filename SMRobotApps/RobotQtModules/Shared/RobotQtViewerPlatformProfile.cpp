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
        constexpr int kProfileSchemaVersion = 1;
        const QString kWorkbenchExtensionApiVersion = QStringLiteral("1");

        QStringList stringArray(const QJsonObject& object, const QString& key, bool* valid)
        {
            QStringList values;
            const QJsonValue value = object.value(key);
            if(value.isUndefined()) {
                return values;
            }
            if(!value.isArray()) {
                *valid = false;
                return values;
            }
            for(const QJsonValue& item : value.toArray()) {
                if(!item.isString() || item.toString().trimmed().isEmpty()) {
                    *valid = false;
                    return {};
                }
                const QString text = item.toString().trimmed();
                if(!values.contains(text)) {
                    values.push_back(text);
                }
            }
            return values;
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
    }

    bool RobotQtViewerResolvedPlatformComposition::succeeded() const
    {
        return std::none_of(
            diagnostics.cbegin(),
            diagnostics.cend(),
            [](const RobotQtViewerPlatformDiagnostic& diagnostic) {
                return diagnostic.severity == RobotQtViewerPlatformDiagnosticSeverity::Error;
            });
    }

    bool RobotQtViewerResolvedPlatformComposition::containsMode(const QString& modeId) const
    {
        return enabledModeIds.contains(modeId);
    }

    bool RobotQtViewerResolvedPlatformComposition::containsMode(
        RobotQtViewerWorkbenchKind kind) const
    {
        return containsMode(robotQtViewerWorkbenchId(kind));
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
        const RobotQtViewerPlatformProfile& profile,
        const RobotQtViewerPlatformUserOverlay& overlay)
    {
        RobotQtViewerResolvedPlatformComposition result;
        result.profileId = profile.id;
        result.displayName = profile.displayName;
        result.defaultModeId = profile.defaultModeId;

        if(profile.schema != QStringLiteral("smrobot.platform-profile") ||
            profile.version != kProfileSchemaVersion) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.schema"),
                profile.id,
                QStringLiteral("Unsupported platform profile schema or version."));
        }
        if(profile.displayName.trimmed().isEmpty()) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.display_name"),
                profile.id,
                QStringLiteral("Platform profile display name is empty."));
        }
        if(!isValidStableId(profile.id)) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.profile.id"),
                profile.id,
                QStringLiteral("Platform profile has an invalid stable id: %1").arg(profile.id));
        }
        const auto validateIds = [&](const QStringList& ids, const QString& field) {
            for(const QString& id : ids) {
                if(!isValidStableId(id)) {
                    addDiagnostic(result,
                        RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.profile.reference_id"),
                        id,
                        QStringLiteral("Platform profile field %1 has an invalid stable id: %2")
                            .arg(field, id));
                }
            }
        };
        validateIds(profile.requiredFeatureIds, QStringLiteral("requiredFeatures"));
        validateIds(profile.requiredModeIds, QStringLiteral("requiredModes"));
        validateIds(profile.optionalModeIds, QStringLiteral("optionalModes"));
        validateIds(profile.defaultEnabledOptionalModeIds,
            QStringLiteral("defaultEnabledOptionalModes"));
        validateIds(profile.modeOrder, QStringLiteral("modeOrder"));
        if(!isValidStableId(profile.defaultModeId)) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.default_mode.id"),
                profile.defaultModeId,
                QStringLiteral("Platform default mode has an invalid stable id: %1")
                    .arg(profile.defaultModeId));
        }
        if(overlay.schema != QStringLiteral("smrobot.platform-profile-overlay") ||
            overlay.version != kProfileSchemaVersion) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.overlay.schema"),
                overlay.profileId,
                QStringLiteral("Unsupported platform profile overlay schema or version."));
        }
        if(!overlay.profileId.isEmpty() && overlay.profileId != profile.id) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.overlay.profile_mismatch"),
                overlay.profileId,
                QStringLiteral("The user overlay belongs to another platform profile."));
        }
        validateIds(overlay.enabledOptionalModeIds,
            QStringLiteral("overlay.enabledOptionalModes"));
        validateIds(overlay.disabledOptionalModeIds,
            QStringLiteral("overlay.disabledOptionalModes"));

        std::set<QString> requiredModes;
        std::set<QString> optionalModes;
        std::set<QString> enabledModes;
        for(const QString& modeId : profile.requiredModeIds) {
            requiredModes.insert(modeId);
        }
        for(const QString& featureId : profile.requiredFeatureIds) {
            const RobotQtViewerWorkbenchFeatureDesc* featureDesc = catalog.feature(featureId);
            if(featureDesc == nullptr) {
                addDiagnostic(result,
                    RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.feature.missing"),
                    featureId,
                    QStringLiteral("Required feature is not in the build catalog: %1").arg(featureId));
                continue;
            }
            for(const QString& modeId : featureDesc->requiredModeIds) {
                requiredModes.insert(modeId);
            }
        }
        for(const QString& modeId : profile.optionalModeIds) {
            optionalModes.insert(modeId);
            if(requiredModes.find(modeId) != requiredModes.end()) {
                addDiagnostic(result,
                    RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.mode.required_and_optional"),
                    modeId,
                    QStringLiteral("Workbench mode cannot be both required and optional: %1")
                        .arg(modeId));
            }
            if(catalog.registeredMode(modeId) == nullptr) {
                addDiagnostic(result,
                    RobotQtViewerPlatformDiagnosticSeverity::Warning,
                    QStringLiteral("platform.mode.optional_missing"),
                    modeId,
                    QStringLiteral("Optional Workbench mode is not in the build catalog: %1")
                        .arg(modeId));
            }
        }
        enabledModes = requiredModes;
        for(const QString& modeId : profile.defaultEnabledOptionalModeIds) {
            if(optionalModes.find(modeId) == optionalModes.end()) {
                addDiagnostic(result,
                    RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.optional.default_not_allowed"),
                    modeId,
                    QStringLiteral("Default-enabled mode is not declared optional: %1").arg(modeId));
            } else {
                enabledModes.insert(modeId);
            }
        }

        if(!overlay.enabledOptionalModeIds.isEmpty() ||
            !overlay.disabledOptionalModeIds.isEmpty()) {
            if(!profile.allowUserOverrides) {
                addDiagnostic(result,
                    RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.overlay.not_allowed"),
                    profile.id,
                    QStringLiteral("This platform profile does not allow user overrides."));
            }
            for(const QString& modeId : overlay.enabledOptionalModeIds) {
                if(overlay.disabledOptionalModeIds.contains(modeId)) {
                    addDiagnostic(result,
                        RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.overlay.contradictory_mode"),
                        modeId,
                        QStringLiteral("The overlay both enables and disables this mode: %1")
                            .arg(modeId));
                    continue;
                }
                if(optionalModes.find(modeId) == optionalModes.end()) {
                    addDiagnostic(result,
                        RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.overlay.mode_not_allowed"),
                        modeId,
                        QStringLiteral("The overlay cannot enable this mode: %1").arg(modeId));
                } else {
                    enabledModes.insert(modeId);
                }
            }
            for(const QString& modeId : overlay.disabledOptionalModeIds) {
                if(requiredModes.find(modeId) != requiredModes.end()) {
                    addDiagnostic(result,
                        RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.overlay.required_mode"),
                        modeId,
                        QStringLiteral("The overlay cannot disable a required mode: %1").arg(modeId));
                } else if(optionalModes.find(modeId) == optionalModes.end()) {
                    addDiagnostic(result,
                        RobotQtViewerPlatformDiagnosticSeverity::Warning,
                        QStringLiteral("platform.overlay.unknown_mode"),
                        modeId,
                        QStringLiteral("The overlay references an unknown optional mode: %1").arg(modeId));
                } else {
                    enabledModes.erase(modeId);
                }
            }
        }

        std::set<QString> expanded;
        std::set<QString> visiting;
        std::function<bool(const QString&, bool)> includeMode;
        includeMode = [&](const QString& modeId, bool required) {
            if(visiting.find(modeId) != visiting.end()) {
                addDiagnostic(result,
                    RobotQtViewerPlatformDiagnosticSeverity::Error,
                    QStringLiteral("platform.mode.dependency_cycle"),
                    modeId,
                    QStringLiteral("Workbench mode dependency cycle contains: %1").arg(modeId));
                return false;
            }
            if(expanded.find(modeId) != expanded.end()) {
                if(required) {
                    requiredModes.insert(modeId);
                }
                return true;
            }
            visiting.insert(modeId);
            const RobotQtViewerWorkbenchModeDesc* modeDesc = catalog.registeredMode(modeId);
            if(modeDesc == nullptr) {
                if(required || optionalModes.find(modeId) == optionalModes.end()) {
                    addDiagnostic(result,
                        required
                            ? RobotQtViewerPlatformDiagnosticSeverity::Error
                            : RobotQtViewerPlatformDiagnosticSeverity::Warning,
                        required
                            ? QStringLiteral("platform.mode.required_missing")
                            : QStringLiteral("platform.mode.dependency_missing"),
                        modeId,
                        QStringLiteral("Workbench mode is not in the build catalog: %1")
                            .arg(modeId));
                }
                enabledModes.erase(modeId);
                visiting.erase(modeId);
                return false;
            }
            const RobotQtViewerWorkbenchPackageDesc* packageDesc =
                catalog.package(modeDesc->packageId);
            if(packageDesc == nullptr ||
                packageDesc->extensionApiVersion != kWorkbenchExtensionApiVersion) {
                addDiagnostic(result,
                    required
                        ? RobotQtViewerPlatformDiagnosticSeverity::Error
                        : RobotQtViewerPlatformDiagnosticSeverity::Warning,
                    QStringLiteral("platform.package.unavailable"),
                    modeDesc->packageId,
                    QStringLiteral("Workbench package is missing or uses an incompatible extension API: %1")
                        .arg(modeDesc->packageId));
                enabledModes.erase(modeId);
                visiting.erase(modeId);
                return false;
            }
            for(const QString& packageDependencyId : packageDesc->requiredPackageIds) {
                if(!catalog.hasPackage(packageDependencyId)) {
                    addDiagnostic(result,
                        required
                            ? RobotQtViewerPlatformDiagnosticSeverity::Error
                            : RobotQtViewerPlatformDiagnosticSeverity::Warning,
                        QStringLiteral("platform.package.dependency_missing"),
                        packageDependencyId,
                        QStringLiteral("Workbench package dependency is missing: %1 requires %2")
                            .arg(packageDesc->id, packageDependencyId));
                    enabledModes.erase(modeId);
                    visiting.erase(modeId);
                    return false;
                }
            }
            if(required) {
                requiredModes.insert(modeId);
            }
            for(const QString& dependencyId : modeDesc->requiredModeIds) {
                enabledModes.insert(dependencyId);
                if(!includeMode(dependencyId, required)) {
                    enabledModes.erase(modeId);
                    visiting.erase(modeId);
                    return false;
                }
            }
            visiting.erase(modeId);
            expanded.insert(modeId);
            return true;
        };
        const std::set<QString> requestedModes = enabledModes;
        for(const QString& modeId : requestedModes) {
            if(!includeMode(modeId, requiredModes.find(modeId) != requiredModes.end())) {
                enabledModes.erase(modeId);
            }
        }

        for(const QString& modeId : enabledModes) {
            const RobotQtViewerWorkbenchModeDesc* modeDesc = catalog.registeredMode(modeId);
            if(modeDesc == nullptr) {
                continue;
            }
            for(const QString& conflictId : modeDesc->conflictsWithModeIds) {
                if(enabledModes.find(conflictId) != enabledModes.end()) {
                    addDiagnostic(result,
                        RobotQtViewerPlatformDiagnosticSeverity::Error,
                        QStringLiteral("platform.mode.conflict"),
                        modeId,
                        QStringLiteral("Workbench modes conflict: %1 and %2")
                            .arg(modeId, conflictId));
                }
            }
        }

        if(enabledModes.find(profile.defaultModeId) == enabledModes.end() ||
            catalog.registeredMode(profile.defaultModeId) == nullptr) {
            addDiagnostic(result,
                RobotQtViewerPlatformDiagnosticSeverity::Error,
                QStringLiteral("platform.default_mode.unavailable"),
                profile.defaultModeId,
                QStringLiteral("Default mode is not enabled and available: %1")
                    .arg(profile.defaultModeId));
        }

        for(const QString& modeId : profile.modeOrder) {
            if(enabledModes.find(modeId) != enabledModes.end() &&
                !result.enabledModeIds.contains(modeId)) {
                result.enabledModeIds.push_back(modeId);
            }
        }
        QVector<const RobotQtViewerWorkbenchModeDesc*> remaining;
        for(const QString& modeId : enabledModes) {
            if(result.enabledModeIds.contains(modeId)) {
                continue;
            }
            if(const RobotQtViewerWorkbenchModeDesc* modeDesc = catalog.registeredMode(modeId)) {
                remaining.push_back(modeDesc);
            }
        }
        std::sort(remaining.begin(), remaining.end(),
            [](const RobotQtViewerWorkbenchModeDesc* lhs,
               const RobotQtViewerWorkbenchModeDesc* rhs) {
                if(lhs->defaultOrder != rhs->defaultOrder) {
                    return lhs->defaultOrder < rhs->defaultOrder;
                }
                return lhs->descriptor.id < rhs->descriptor.id;
            });
        for(const RobotQtViewerWorkbenchModeDesc* modeDesc : remaining) {
            result.enabledModeIds.push_back(modeDesc->descriptor.id);
        }
        for(const QString& modeId : result.enabledModeIds) {
            if(requiredModes.find(modeId) != requiredModes.end()) {
                result.requiredModeIds.push_back(modeId);
            }
        }
        return result;
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
        bool valid = true;
        RobotQtViewerPlatformProfile loaded;
        loaded.schema = object.value(QStringLiteral("schema")).toString();
        loaded.version = object.value(QStringLiteral("version")).toInt(-1);
        loaded.id = object.value(QStringLiteral("id")).toString().trimmed();
        loaded.displayName = object.value(QStringLiteral("displayName")).toString().trimmed();
        loaded.requiredFeatureIds = stringArray(object, QStringLiteral("requiredFeatures"), &valid);
        loaded.requiredModeIds = stringArray(object, QStringLiteral("requiredModes"), &valid);
        loaded.optionalModeIds = stringArray(object, QStringLiteral("optionalModes"), &valid);
        loaded.defaultEnabledOptionalModeIds =
            stringArray(object, QStringLiteral("defaultEnabledOptionalModes"), &valid);
        loaded.defaultModeId = object.value(QStringLiteral("defaultMode")).toString().trimmed();
        loaded.modeOrder = stringArray(object, QStringLiteral("modeOrder"), &valid);
        loaded.allowUserOverrides = object.value(QStringLiteral("allowUserOverrides")).toBool(true);
        if(!valid || loaded.schema.isEmpty() || loaded.id.isEmpty() ||
            loaded.displayName.isEmpty() || loaded.defaultModeId.isEmpty()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Platform profile has missing or invalid fields: %1")
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

    bool RobotQtViewerPlatformProfileIo::loadOverlay(
        const std::filesystem::path& path,
        RobotQtViewerPlatformUserOverlay* overlay,
        QString* errorMessage)
    {
        if(overlay == nullptr) {
            return false;
        }
        std::error_code existsError;
        if(!std::filesystem::exists(path, existsError)) {
            *overlay = {};
            if(errorMessage != nullptr) {
                errorMessage->clear();
            }
            return true;
        }
        QJsonObject object;
        if(!readJsonObject(path, &object, errorMessage)) {
            return false;
        }
        bool valid = true;
        RobotQtViewerPlatformUserOverlay loaded;
        loaded.schema = object.value(QStringLiteral("schema")).toString();
        loaded.version = object.value(QStringLiteral("version")).toInt(-1);
        loaded.profileId = object.value(QStringLiteral("profileId")).toString().trimmed();
        loaded.enabledOptionalModeIds =
            stringArray(object, QStringLiteral("enabledOptionalModes"), &valid);
        loaded.disabledOptionalModeIds =
            stringArray(object, QStringLiteral("disabledOptionalModes"), &valid);
        if(!valid || loaded.schema != QStringLiteral("smrobot.platform-profile-overlay") ||
            loaded.version != kProfileSchemaVersion || loaded.profileId.isEmpty()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Invalid platform profile overlay: %1")
                    .arg(QString::fromStdWString(path.wstring()));
            }
            return false;
        }
        *overlay = loaded;
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
    }

    bool RobotQtViewerPlatformProfileIo::saveOverlay(
        const std::filesystem::path& path,
        const RobotQtViewerPlatformUserOverlay& overlay,
        QString* errorMessage)
    {
        std::error_code directoryError;
        std::filesystem::create_directories(path.parent_path(), directoryError);
        if(directoryError) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Cannot create platform overlay directory: %1")
                    .arg(QString::fromStdString(directoryError.message()));
            }
            return false;
        }
        QJsonObject object;
        object.insert(QStringLiteral("schema"), overlay.schema);
        object.insert(QStringLiteral("version"), overlay.version);
        object.insert(QStringLiteral("profileId"), overlay.profileId);
        object.insert(QStringLiteral("enabledOptionalModes"),
            toJsonArray(overlay.enabledOptionalModeIds));
        object.insert(QStringLiteral("disabledOptionalModes"),
            toJsonArray(overlay.disabledOptionalModeIds));
        QSaveFile file(QString::fromStdWString(path.wstring()));
        if(!file.open(QIODevice::WriteOnly) ||
            file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0 ||
            !file.commit()) {
            if(errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Cannot save platform profile overlay: %1")
                    .arg(file.fileName());
            }
            return false;
        }
        if(errorMessage != nullptr) {
            errorMessage->clear();
        }
        return true;
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
