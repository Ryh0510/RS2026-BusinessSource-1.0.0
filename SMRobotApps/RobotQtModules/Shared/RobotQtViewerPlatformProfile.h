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
        int version = 2;
        QString id;
        QString displayName;
        QStringList workbenchIds;
        QString defaultWorkbenchId;
    };

    struct RobotQtViewerPlatformSelection
    {
        QString schema = QStringLiteral("smrobot.platform-selection");
        int version = 1;
        QString profileId = QStringLiteral("base-robot");
    };

    struct RobotQtViewerResolvedPlatformComposition
    {
        QString profileId;
        QString displayName;
        QString defaultWorkbenchId;
        QStringList enabledWorkbenchIds;
        QVector<RobotQtViewerPlatformDiagnostic> diagnostics;

        bool succeeded() const;
        bool containsWorkbench(const QString& workbenchId) const;
        bool containsWorkbench(RobotQtViewerWorkbenchKind kind) const;
        QString diagnosticText() const;
    };

    class RobotQtViewerPlatformProfileResolver final
    {
    public:
        static RobotQtViewerResolvedPlatformComposition resolve(
            const RobotQtViewerWorkbenchPackageRegistry& catalog,
            const RobotQtViewerPlatformProfile& profile);
    };

    RobotQtViewerPlatformProfile makeRobotQtViewerBuiltInBaseProfile(
        const RobotQtViewerWorkbenchPackageRegistry& catalog);

    class RobotQtViewerPlatformProfileIo final
    {
    public:
        static bool loadProfile(
            const std::filesystem::path& path,
            RobotQtViewerPlatformProfile* profile,
            QString* errorMessage = nullptr);
        static bool saveProfile(
            const std::filesystem::path& path,
            const RobotQtViewerPlatformProfile& profile,
            QString* errorMessage = nullptr);
        static bool loadSelection(
            const std::filesystem::path& path,
            RobotQtViewerPlatformSelection* selection,
            QString* errorMessage = nullptr);
        static bool saveSelection(
            const std::filesystem::path& path,
            const RobotQtViewerPlatformSelection& selection,
            QString* errorMessage = nullptr);
        static QVector<std::filesystem::path> discoverProfiles(
            const std::filesystem::path& profilesDirectory);
    };
}
