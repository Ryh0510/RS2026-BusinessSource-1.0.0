#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerPlatformProfile.h"
#include "RobotQtViewerWorkbenchPlugin.h"
#include "RobotQtViewerWorkbenchTransitionCoordinator.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QWidget>

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if(!condition) {
            throw std::runtime_error(message);
        }
    }

    std::filesystem::path copyStagingDirectory(
        const std::filesystem::path& source,
        QTemporaryDir& temporaryDirectory)
    {
        const std::filesystem::path target =
            std::filesystem::u8path(temporaryDirectory.path().toUtf8().constData()) /
            "staging";
        std::filesystem::copy(
            source,
            target,
            std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing);
        return target;
    }

    std::filesystem::path findManifest(const std::filesystem::path& stagingDirectory)
    {
        for(const auto& entry : std::filesystem::recursive_directory_iterator(
                stagingDirectory)) {
            if(entry.is_regular_file() &&
                entry.path().filename().string().find(".workbench.json") !=
                    std::string::npos) {
                return entry.path();
            }
        }
        throw std::runtime_error("staged Workbench manifest is missing");
    }

    QJsonObject readManifest(const std::filesystem::path& path)
    {
        QFile file(QString::fromStdWString(path.wstring()));
        require(file.open(QIODevice::ReadOnly), "Workbench manifest could not be opened");
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        require(error.error == QJsonParseError::NoError && document.isObject(),
            "Workbench manifest fixture is invalid");
        return document.object();
    }

    void updateManifest(
        const std::filesystem::path& path,
        const std::function<void(QJsonObject&)>& update)
    {
        QJsonObject root = readManifest(path);
        update(root);
        QFile file(QString::fromStdWString(path.wstring()));
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "Workbench manifest fixture could not be updated");
        require(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) > 0,
            "Workbench manifest fixture update failed");
    }

    void testIntegrityFailure(const std::filesystem::path& stagingDirectory)
    {
        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "integrity test directory is invalid");
        const std::filesystem::path testRoot =
            copyStagingDirectory(stagingDirectory, temporaryDirectory);
        const std::filesystem::path manifestPath = findManifest(testRoot);
        const QJsonObject manifest = readManifest(manifestPath);
        const QString binaryFile = manifest.value(QStringLiteral("binary"))
            .toObject().value(QStringLiteral("file")).toString();
        QFile binary(QString::fromStdWString(
            (manifestPath.parent_path() /
                std::filesystem::u8path(binaryFile.toUtf8().constData())).wstring()));
        require(binary.open(QIODevice::Append), "Workbench binary fixture could not be changed");
        require(binary.write("x", 1) == 1, "Workbench binary fixture change failed");
        binary.close();

        robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry registry;
        robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader loader;
        QStringList diagnostics;
        require(loader.discover(testRoot, registry, &diagnostics),
            "tampered Workbench manifest discovery failed unexpectedly");
        const QString workbenchId = QStringLiteral("smrobot.mode.example-inspection");
        require(registry.setEnabledWorkbenchIds(QStringList{ workbenchId }),
            "tampered Workbench could not be selected for validation");
        QString error;
        require(!loader.loadEnabled(QStringList{ workbenchId }, registry, &error) &&
                error.contains(QStringLiteral("integrity"), Qt::CaseInsensitive),
            "tampered Workbench binary was not rejected by SHA256 validation");
    }

    void testDuplicateManifestFailure(const std::filesystem::path& stagingDirectory)
    {
        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "duplicate test directory is invalid");
        const std::filesystem::path testRoot =
            copyStagingDirectory(stagingDirectory, temporaryDirectory);
        const std::filesystem::path manifestPath = findManifest(testRoot);
        require(QFile::copy(
                    QString::fromStdWString(manifestPath.wstring()),
                    QString::fromStdWString(
                        (manifestPath.parent_path() / "duplicate.workbench.json").wstring())),
            "duplicate Workbench manifest fixture could not be created");

        robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry registry;
        robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader loader;
        QStringList diagnostics;
        require(!loader.discover(testRoot, registry, &diagnostics) &&
                !diagnostics.isEmpty(),
            "duplicate Workbench package and Workbench IDs were accepted");
    }

    void testCompatibilityFailure(const std::filesystem::path& stagingDirectory)
    {
        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "compatibility test directory is invalid");
        const std::filesystem::path testRoot =
            copyStagingDirectory(stagingDirectory, temporaryDirectory);
        const std::filesystem::path manifestPath = findManifest(testRoot);
        updateManifest(manifestPath, [](QJsonObject& root) {
            QJsonObject compatibility =
                root.value(QStringLiteral("compatibility")).toObject();
            compatibility.insert(
                QStringLiteral("qtMinor"),
                compatibility.value(QStringLiteral("qtMinor")).toInt() + 1);
            root.insert(QStringLiteral("compatibility"), compatibility);
        });

        robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry registry;
        robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader loader;
        QStringList diagnostics;
        require(!loader.discover(testRoot, registry, &diagnostics) &&
                !diagnostics.isEmpty(),
            "ABI-incompatible Workbench manifest was accepted");
    }

    void testMissingPackageDependency(const std::filesystem::path& stagingDirectory)
    {
        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "dependency test directory is invalid");
        const std::filesystem::path testRoot =
            copyStagingDirectory(stagingDirectory, temporaryDirectory);
        const std::filesystem::path manifestPath = findManifest(testRoot);
        updateManifest(manifestPath, [](QJsonObject& root) {
            QJsonObject package = root.value(QStringLiteral("package")).toObject();
            package.insert(
                QStringLiteral("requiredPackageIds"),
                QJsonArray{ QStringLiteral("smrobot.package.not-deployed") });
            root.insert(QStringLiteral("package"), package);
        });

        robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry registry;
        robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader loader;
        QStringList diagnostics;
        require(loader.discover(testRoot, registry, &diagnostics),
            "Workbench dependency manifest could not be discovered");
        robot_qt_viewer::RobotQtViewerPlatformProfile profile;
        profile.id = QStringLiteral("dependency-test");
        profile.displayName = QStringLiteral("Dependency Test");
        profile.workbenchIds = QStringList{
            QStringLiteral("smrobot.mode.example-inspection") };
        profile.defaultWorkbenchId = profile.workbenchIds.front();
        const robot_qt_viewer::RobotQtViewerResolvedPlatformComposition resolved =
            robot_qt_viewer::RobotQtViewerPlatformProfileResolver::resolve(
                registry, profile);
        bool foundDependencyDiagnostic = false;
        for(const robot_qt_viewer::RobotQtViewerPlatformDiagnostic& diagnostic :
                resolved.diagnostics) {
            foundDependencyDiagnostic = foundDependencyDiagnostic ||
                diagnostic.code == QStringLiteral("platform.package.dependency_missing");
        }
        require(!resolved.succeeded() && foundDependencyDiagnostic,
            "missing Workbench package dependency did not invalidate the Product Profile");
    }

    void testPackageDependencyCycle(const std::filesystem::path& stagingDirectory)
    {
        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "dependency cycle test directory is invalid");
        const std::filesystem::path testRoot =
            copyStagingDirectory(stagingDirectory, temporaryDirectory);
        const std::filesystem::path manifestPath = findManifest(testRoot);
        updateManifest(manifestPath, [](QJsonObject& root) {
            QJsonObject package = root.value(QStringLiteral("package")).toObject();
            package.insert(
                QStringLiteral("requiredPackageIds"),
                QJsonArray{ QStringLiteral("smrobot.example.inspection") });
            root.insert(QStringLiteral("package"), package);
        });

        robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry registry;
        robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader loader;
        QStringList diagnostics;
        require(loader.discover(testRoot, registry, &diagnostics),
            "Workbench dependency cycle fixture could not be discovered");
        const QString workbenchId = QStringLiteral("smrobot.mode.example-inspection");
        require(registry.setEnabledWorkbenchIds(QStringList{ workbenchId }),
            "Workbench dependency cycle Mode could not be selected");
        QString error;
        require(!loader.loadEnabled(QStringList{ workbenchId }, registry, &error) &&
                error.contains(QStringLiteral("cycle"), Qt::CaseInsensitive),
            "Workbench package dependency cycle was not rejected");
    }
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    require(argc == 2, "Workbench staging directory argument is missing");
    const std::filesystem::path stagingDirectory(argv[1]);

    robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry registry;
    robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader loader;
    QStringList diagnostics;
    require(loader.discover(stagingDirectory, registry, &diagnostics),
        "Workbench plugin discovery failed");
    require(diagnostics.isEmpty(), "Workbench plugin discovery reported diagnostics");

    const QString workbenchId = QStringLiteral("smrobot.mode.example-inspection");
    require(registry.registeredWorkbench(workbenchId) != nullptr,
        "manifest Mode was not added to the available catalog");
    require(registry.setEnabledWorkbenchIds(QStringList{ workbenchId }),
        "plugin Mode could not be enabled");
    QString error;
    require(loader.loadEnabled(QStringList{ workbenchId }, registry, &error),
        error.toUtf8().constData());
    require(!registry.isWorkbenchReady(workbenchId),
        "plugin loader bypassed the runtime contribution host");

    QWidget owner;
    robot_qt_viewer::RobotQtViewerEditSessionCoordinator editSessions;
    robot_qt_viewer::RobotQtViewerWorkbenchContributionHost contributionHost(
        registry, editSessions);
    const auto pluginFactories = loader.runtimeContributionFactories();
    require(pluginFactories.size() == 1,
        "plugin loader did not expose exactly one enabled contribution factory");
    require(contributionHost.registerFactory(pluginFactories.front(), &error),
        error.toUtf8().constData());
    require(contributionHost.instantiateEnabled(
                QStringList{ workbenchId }, &owner, &error),
        error.toUtf8().constData());
    require(registry.isWorkbenchReady(workbenchId),
        "plugin lifecycle was not bound through the runtime contribution host");

    const robot_qt_viewer::RobotQtViewerWorkbenchDescriptor* descriptor =
        registry.descriptor(workbenchId);
    require(descriptor != nullptr, "plugin descriptor is missing");
    robot_qt_viewer::RobotQtViewerWorkbenchManager manager;
    manager.setInitialWorkbench(workbenchId, *descriptor);
    robot_qt_viewer::RobotQtViewerEventHub events;
    robot_qt_viewer::RobotQtViewerWorkbenchTransitionCoordinator coordinator(
        registry, manager, events);
    require(coordinator.initializeActiveWorkbench(QStringLiteral("plugin-test")).succeeded(),
        "plugin Mode activation failed");
    require(manager.activeWorkbenchId() == workbenchId,
        "stable plugin Workbench ID was not committed");

    QWidget* panel = contributionHost.panelForWorkbench(workbenchId);
    require(panel != nullptr &&
            panel->objectName() == QStringLiteral("exampleInspectionWorkbenchPanel"),
        "plugin UI contribution was not created");
    coordinator.shutdownAll(QStringLiteral("plugin-test"));
    contributionHost.clear();
    require(!registry.isWorkbenchReady(workbenchId),
        "plugin runtime contribution bindings survived host teardown");

    testIntegrityFailure(stagingDirectory);
    testDuplicateManifestFailure(stagingDirectory);
    testCompatibilityFailure(stagingDirectory);
    testMissingPackageDependency(stagingDirectory);
    testPackageDependencyCycle(stagingDirectory);

    std::cout << "Workbench plugin test passed.\n";
    return 0;
}
