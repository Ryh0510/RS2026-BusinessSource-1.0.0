#pragma once

#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <filesystem>

namespace robot_qt_viewer
{
    enum class RobotQtViewerProductProfileDiagnosticSeverity
    {
        Warning,
        Error
    };

    struct RobotQtViewerProductProfileDiagnostic
    {
        RobotQtViewerProductProfileDiagnosticSeverity severity =
            RobotQtViewerProductProfileDiagnosticSeverity::Error;
        QString code;
        QString subjectId;
        QString message;
    };

    struct RobotQtViewerProductProfile
    {
        QString schema = QStringLiteral("smrobot.platform-profile");
        int version = 2;
        QString id;
        QString displayName;
        QStringList requiredFeatureIds;
        QStringList workbenchIds;
        QString defaultWorkbenchId;
    };

    struct RobotQtViewerProductSelection
    {
        QString schema = QStringLiteral("smrobot.platform-selection");
        int version = 1;
        QString profileId = QStringLiteral("base-robot");
    };

    struct RobotQtViewerResolvedProductComposition
    {
        QString profileId;
        QString displayName;
        QString defaultWorkbenchId;
        QStringList enabledWorkbenchIds;
        QVector<RobotQtViewerProductProfileDiagnostic> diagnostics;

        bool succeeded() const;
        bool containsWorkbench(const QString& workbenchId) const;
        bool containsWorkbench(RobotQtViewerWorkbenchKind kind) const;
        QString diagnosticText() const;
    };

    class RobotQtViewerProductProfileResolver final
    {
    public:
        static RobotQtViewerResolvedProductComposition resolve(
            const RobotQtViewerWorkbenchPackageRegistry& catalog,
            const RobotQtViewerProductProfile& profile);
    };

    RobotQtViewerProductProfile makeRobotQtViewerBuiltInBaseProductProfile(
        const RobotQtViewerWorkbenchPackageRegistry& catalog);

    class RobotQtViewerProductProfileIo final
    {
    public:
        static bool loadProfile(
            const std::filesystem::path& path,
            RobotQtViewerProductProfile* profile,
            QString* errorMessage = nullptr);
        static bool saveProfile(
            const std::filesystem::path& path,
            const RobotQtViewerProductProfile& profile,
            QString* errorMessage = nullptr);
        static bool loadSelection(
            const std::filesystem::path& path,
            RobotQtViewerProductSelection* selection,
            QString* errorMessage = nullptr);
        static bool saveSelection(
            const std::filesystem::path& path,
            const RobotQtViewerProductSelection& selection,
            QString* errorMessage = nullptr);
        static QVector<std::filesystem::path> discoverProfiles(
            const std::filesystem::path& profilesDirectory);
    };
}
