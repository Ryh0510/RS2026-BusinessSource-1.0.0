#include "MainWindow.h"
#include "RobotQtViewerBuiltWorkbenchCatalog.h"
#include "RobotQtViewerTheme.h"
#include "RobotQtViewerWorkbenchPlugin.h"

#include <CustomLog/CustomLog.h>
#include <SimulationProject/RuntimePaths.h>

#include <QApplication>
#include <QByteArray>
#include <QIcon>
#include <QMessageBox>
#include <QSurfaceFormat>
#include <QTimer>

#ifdef Q_OS_WIN
#include "RobotQtViewerResource.h"

#include <windows.h>
#endif

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    struct PlatformStartupContext
    {
        robot_qt_viewer::RobotQtViewerWorkbenchPackageRegistry catalog;
        robot_qt_viewer::RobotQtViewerPlatformProfile profile;
        robot_qt_viewer::RobotQtViewerResolvedPlatformComposition composition;
        std::filesystem::path profilesDirectory;
        std::filesystem::path selectionPath;
        std::unique_ptr<robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader> pluginLoader;
    };

    void initializeLogging()
    {
        std::map<std::string, std::pair<bool, bool>> logInfo = {
            {"rs2026", {true, true}},
            {"SMRobot", {true, true}},
            {"RobotIO", {true, true}},
            {"AssetCore", {true, true}},
            {"asset.model", {true, true}},
            {"render.geometry", {true, true}},
            {"collision.geometry", {true, true}},
        };

        CustomLog::init(logInfo);
        CustomLog::set_level(CustomLog::Level::debug);
        CustomLog::rotate_all();
    }

    std::string argumentValue(int argc, char* argv[], const std::string& name)
    {
        for(int i = 1; i + 1 < argc; ++i) {
            if(argv[i] != nullptr && name == argv[i]) {
                return argv[i + 1] != nullptr ? argv[i + 1] : std::string();
            }
        }
        return std::string();
    }

    int intArgumentValue(int argc, char* argv[], const std::string& name, int fallback)
    {
        const std::string value = argumentValue(argc, argv, name);
        if(value.empty()) {
            return fallback;
        }
        try {
            return std::stoi(value);
        } catch(...) {
            return fallback;
        }
    }

    bool hasArgument(int argc, char* argv[], const std::string& name)
    {
        for(int i = 1; i < argc; ++i) {
            if(argv[i] != nullptr && name == argv[i]) {
                return true;
            }
        }
        return false;
    }

    std::filesystem::path utf8Path(const QString& value)
    {
        const QByteArray utf8 = value.toUtf8();
        return std::filesystem::u8path(utf8.constData());
    }

    PlatformStartupContext loadPlatformStartupContext(int argc, char* argv[])
    {
        PlatformStartupContext context;
        QString catalogError;
        context.catalog = robot_qt_viewer::makeRobotQtViewerBuiltWorkbenchCatalog(&catalogError);
        if(!catalogError.isEmpty()) {
            throw std::runtime_error(catalogError.toStdString());
        }

        context.pluginLoader =
            std::make_unique<robot_qt_viewer::RobotQtViewerWorkbenchPluginLoader>();
        QStringList pluginDiagnostics;
        context.pluginLoader->discover(
            simulation_project::RuntimePaths::applicationRoot() / "workbenches",
            context.catalog,
            &pluginDiagnostics);
        for(const QString& diagnostic : pluginDiagnostics) {
            LOG_WARNING("rs2026") << diagnostic.toStdString();
        }

        context.profilesDirectory =
            simulation_project::RuntimePaths::configRoot() / "platforms";
        context.selectionPath =
            simulation_project::RuntimePaths::configRoot() / "simulation-platform.json";

        const std::string requestedArgument =
            argumentValue(argc, argv, "--platform-profile");
        const bool hasExplicitProfile = !requestedArgument.empty();
        QString requested = QString::fromUtf8(requestedArgument.c_str());
        if(!hasExplicitProfile) {
            robot_qt_viewer::RobotQtViewerPlatformSelection selection;
            QString selectionError;
            if(robot_qt_viewer::RobotQtViewerPlatformProfileIo::loadSelection(
                   context.selectionPath, &selection, &selectionError)) {
                requested = selection.profileId;
            } else {
                requested = QStringLiteral("base-robot");
                LOG_WARNING("rs2026")
                    << "Simulation platform selection is unavailable; using base-robot. "
                    << selectionError.toStdString();
            }
        }
        const auto resolveProfilePath = [&](const QString& profileReference) {
            if(profileReference.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive) ||
                profileReference.contains(QLatin1Char('/')) ||
                profileReference.contains(QLatin1Char('\\'))) {
                std::filesystem::path path = utf8Path(profileReference);
                if(path.is_relative()) {
                    path = std::filesystem::current_path() / path;
                }
                return path;
            }
            return context.profilesDirectory /
                utf8Path(profileReference + QStringLiteral(".platform.json"));
        };

        std::filesystem::path profilePath = resolveProfilePath(requested);
        QString profileError;
        if(!robot_qt_viewer::RobotQtViewerPlatformProfileIo::loadProfile(
               profilePath, &context.profile, &profileError)) {
            if(hasExplicitProfile) {
                throw std::runtime_error(profileError.toStdString());
            }
            LOG_WARNING("rs2026") << "Selected Product Profile is unavailable; falling back to base-robot. "
                << profileError.toStdString();
            if(requested != QStringLiteral("base-robot")) {
                profilePath = resolveProfilePath(QStringLiteral("base-robot"));
                robot_qt_viewer::RobotQtViewerPlatformProfileIo::loadProfile(
                    profilePath, &context.profile, &profileError);
            }
            if(context.profile.id.isEmpty()) {
                context.profile = robot_qt_viewer::makeRobotQtViewerBuiltInBaseProfile(
                    context.catalog);
                LOG_WARNING("rs2026")
                    << "Shipped base-robot Product Profile is unavailable; using the built-in fallback. "
                    << profileError.toStdString();
            }
        }
        context.composition = robot_qt_viewer::RobotQtViewerPlatformProfileResolver::resolve(
            context.catalog, context.profile);
        if(!context.composition.succeeded()) {
            if(hasExplicitProfile) {
                throw std::runtime_error(context.composition.diagnosticText().toStdString());
            }
            LOG_WARNING("rs2026")
                << "Selected Product Profile cannot be composed; using the built-in fallback. "
                << context.composition.diagnosticText().toStdString();
            context.profile =
                robot_qt_viewer::makeRobotQtViewerBuiltInBaseProfile(context.catalog);
            context.composition = robot_qt_viewer::RobotQtViewerPlatformProfileResolver::resolve(
                context.catalog, context.profile);
            if(!context.composition.succeeded()) {
                throw std::runtime_error(context.composition.diagnosticText().toStdString());
            }
        }
        QString applyError;
        if(!context.catalog.setEnabledWorkbenchIds(
               context.composition.enabledWorkbenchIds, &applyError)) {
            throw std::runtime_error(applyError.toStdString());
        }
        QString pluginError;
        if(!context.pluginLoader->loadEnabled(
               context.composition.enabledWorkbenchIds, context.catalog, &pluginError)) {
            if(hasExplicitProfile) {
                throw std::runtime_error(pluginError.toStdString());
            }
            LOG_WARNING("rs2026")
                << "Selected Product Profile plugin load failed; using the built-in fallback. "
                << pluginError.toStdString();
            context.profile =
                robot_qt_viewer::makeRobotQtViewerBuiltInBaseProfile(context.catalog);
            context.composition = robot_qt_viewer::RobotQtViewerPlatformProfileResolver::resolve(
                context.catalog, context.profile);
            if(!context.composition.succeeded() ||
                !context.catalog.setEnabledWorkbenchIds(
                    context.composition.enabledWorkbenchIds, &applyError)) {
                throw std::runtime_error(
                    context.composition.succeeded()
                        ? applyError.toStdString()
                        : context.composition.diagnosticText().toStdString());
            }
        }
        return context;
    }

    bool isAuthorizationFailure(const std::exception& error)
    {
        return std::string(error.what()).find("authorization failed") != std::string::npos;
    }

    void reportStartupFailure(const std::exception& error, bool showDialog)
    {
        const bool authorizationFailure = isAuthorizationFailure(error);
        const QString message = authorizationFailure
            ? QStringLiteral("This computer is not authorized.")
            : QStringLiteral("RobotQtViewer failed to start.");

        LOG_ERROR("rs2026") << message.toStdString() << " " << error.what();
        std::cerr << message.toStdString() << " " << error.what() << '\n';
        if(showDialog) {
            QMessageBox::critical(
                nullptr,
                authorizationFailure
                    ? QStringLiteral("Authorization Failed")
                    : QStringLiteral("Startup Failed"),
                authorizationFailure
                    ? message
                    : QStringLiteral("%1\n\n%2")
                        .arg(message, QString::fromUtf8(error.what())));
        }
    }

#ifdef Q_OS_WIN
    void applyWindowsTaskbarIcon(QWidget& window)
    {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        const HWND handle = reinterpret_cast<HWND>(window.winId());
        if(instance == nullptr || handle == nullptr) {
            return;
        }

        const auto loadIcon = [instance](int width, int height) {
            return static_cast<HICON>(LoadImageW(
                instance,
                MAKEINTRESOURCEW(IDI_ROBOT_QT_VIEWER),
                IMAGE_ICON,
                width,
                height,
                LR_DEFAULTCOLOR | LR_SHARED));
        };

        const HICON largeIcon = loadIcon(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
        const HICON smallIcon = loadIcon(GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        if(largeIcon != nullptr) {
            SendMessageW(handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
        }
        if(smallIcon != nullptr) {
            SendMessageW(handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        }
    }
#endif
}

int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QSurfaceFormat format;
    format.setVersion(4, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setOrganizationName("RS2026");
    QApplication::setApplicationName("RobotQtViewer");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/RobotQtViewer/icons/app/robot_qt_viewer.png")));
    robot_qt_viewer::ThemeManager::applySaved(app);
    initializeLogging();

#if defined(_DEBUG)
    constexpr const char* buildConfiguration = "Debug";
#else
    constexpr const char* buildConfiguration = "Release";
#endif
    LOG_INFO("rs2026") << "RobotQtViewer build identity: configuration="
        << buildConfiguration
        << ", pointerBits=" << (sizeof(void*) * 8)
        << ", compiled=" << __DATE__ << ' ' << __TIME__;

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        CustomLog::shutdown();
    });

    try {
        PlatformStartupContext platform = loadPlatformStartupContext(argc, argv);
        LOG_INFO("rs2026") << "Simulation platform profile: id="
            << platform.profile.id.toStdString()
            << ", workbenches=" << platform.composition.enabledWorkbenchIds.join(',').toStdString();
        MainWindow window(
            std::move(platform.catalog),
            std::move(platform.profile),
            std::move(platform.composition),
            std::move(platform.profilesDirectory),
            std::move(platform.selectionPath),
            platform.pluginLoader.get(),
            QString::fromUtf8(argumentValue(argc, argv, "--language").c_str()));
        window.resize(1760, 920);
        window.setWindowIcon(QApplication::windowIcon());
        window.show();
#ifdef Q_OS_WIN
        applyWindowsTaskbarIcon(window);
#endif
        robot_qt_viewer::ThemeManager::applyNativeWindowFrame(
            window,
            robot_qt_viewer::ThemeManager::savedTheme());

        const int smokeExitMs = intArgumentValue(argc, argv, "--smoke-exit-ms", 0);
        if(smokeExitMs > 0) {
            QTimer::singleShot(smokeExitMs, &app, &QCoreApplication::quit);
        }

        const std::string profileProject = argumentValue(argc, argv, "--profile-project");
        if(!profileProject.empty()) {
            const int exitDelayMs = intArgumentValue(argc, argv, "--profile-exit-ms", 1500);
            const int repeatCount = intArgumentValue(argc, argv, "--profile-repeat", 1);
            const bool enableCollisionAfterLoad = hasArgument(argc, argv, "--profile-enable-collision");
            const std::string profileGenerateObjectCoacd =
                argumentValue(argc, argv, "--profile-generate-object-coacd");
            const bool profileSetGeneratedCurrent =
                hasArgument(argc, argv, "--profile-set-generated-current");
            const std::string profileSaveProject =
                argumentValue(argc, argv, "--profile-save-project");
            QTimer::singleShot(
                0,
                &window,
                [&window,
                 profileProject,
                 exitDelayMs,
                 repeatCount,
                 enableCollisionAfterLoad,
                 profileGenerateObjectCoacd,
                 profileSetGeneratedCurrent,
                 profileSaveProject]() {
                window.openProjectPathForProfiling(
                    std::filesystem::path(profileProject),
                    exitDelayMs,
                    repeatCount,
                    enableCollisionAfterLoad,
                    QString::fromStdString(profileGenerateObjectCoacd),
                    profileSetGeneratedCurrent,
                    std::filesystem::path(profileSaveProject));
            });
        }

        return app.exec();
    } catch(const std::exception& error) {
        reportStartupFailure(error, !hasArgument(argc, argv, "--smoke-exit-ms"));
    }

    CustomLog::shutdown();
    return EXIT_FAILURE;
}
