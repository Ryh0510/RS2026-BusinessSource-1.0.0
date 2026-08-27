#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <filesystem>

namespace robot_qt_viewer
{
    enum class RobotQtViewerPlatformDiagnosticSeverity
    {
        Warning,
        Error
    };

    struct RobotQtViewerPlatformDiagnostic
    {
        RobotQtViewerPlatformDiagnosticSeverity severity =
            RobotQtViewerPlatformDiagnosticSeverity::Error;
        QString code;
        QString subjectId;
        QString message;
    };

    struct RobotQtViewerPlatformProfile
    {
        QString schema = QStringLiteral("smrobot.platform-profile");
        int version = 1;
        QString id;
        QString displayName;
        QStringList requiredFeatureIds;
        QStringList requiredModeIds;
        QStringList optionalModeIds;
        QStringList defaultEnabledOptionalModeIds;
        QString defaultModeId;
        QStringList modeOrder;
        bool allowUserOverrides = true;
    };

    struct RobotQtViewerPlatformUserOverlay
    {
        QString schema = QStringLiteral("smrobot.platform-profile-overlay");
        int version = 1;
        QString profileId;
        QStringList enabledOptionalModeIds;
        QStringList disabledOptionalModeIds;
    };

    struct RobotQtViewerResolvedPlatformComposition
    {
        QString profileId;
        QString displayName;
        QString defaultModeId;
        QStringList enabledModeIds;
        QStringList requiredModeIds;
        QVector<RobotQtViewerPlatformDiagnostic> diagnostics;

        bool succeeded() const;
        bool containsMode(const QString& modeId) const;
        bool containsMode(RobotQtViewerWorkbenchKind kind) const;
        QString diagnosticText() const;
    };

    class RobotQtViewerPlatformProfileResolver final
    {
    public:
        static RobotQtViewerResolvedPlatformComposition resolve(
            const RobotQtViewerWorkbenchPackageRegistry& catalog,
            const RobotQtViewerPlatformProfile& profile,
            const RobotQtViewerPlatformUserOverlay& overlay = {});
    };

    class RobotQtViewerPlatformProfileIo final
    {
    public:
        static bool loadProfile(
            const std::filesystem::path& path,
            RobotQtViewerPlatformProfile* profile,
            QString* errorMessage = nullptr);
        static bool loadOverlay(
            const std::filesystem::path& path,
            RobotQtViewerPlatformUserOverlay* overlay,
            QString* errorMessage = nullptr);
        static bool saveOverlay(
            const std::filesystem::path& path,
            const RobotQtViewerPlatformUserOverlay& overlay,
            QString* errorMessage = nullptr);
        static QVector<std::filesystem::path> discoverProfiles(
            const std::filesystem::path& profilesDirectory);
    };
}
