#include "RobotQtViewerLocalization.h"
#include "RobotQtViewerWorkbenchPackageRegistry.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
    using namespace robot_qt_viewer;

    void require(bool condition, const char* message)
    {
        if(!condition) {
            throw std::runtime_error(message);
        }
    }

    void writeCatalog(const QString& path, const QByteArray& json)
    {
        QFile file(path);
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "cannot create localization catalog");
        require(file.write(json) == json.size(), "cannot write localization catalog");
    }

    QByteArray englishCatalog()
    {
        return R"json({
  "schema": "smrobot.localization.catalog",
  "version": 1,
  "locale": "en-US",
  "nativeName": "English",
  "fallbackLocale": "",
  "messages": { "shell.menu.file": "File" },
  "legacy": {
    "Apply": "Apply",
    "Selected robot: %1": "Selected robot: %1",
    "Scene Explorer": "Scene Explorer"
  }
})json";
    }

    QByteArray chineseCatalog()
    {
        return R"json({
  "schema": "smrobot.localization.catalog",
  "version": 1,
  "locale": "zh-CN",
  "nativeName": "\u7b80\u4f53\u4e2d\u6587",
  "fallbackLocale": "en-US",
  "messages": { "shell.menu.file": "\u6587\u4ef6" },
  "legacy": {
    "Apply": "\u5e94\u7528",
    "Selected robot: %1": "\u5df2\u9009\u62e9\u673a\u5668\u4eba\uff1a%1",
    "Scene Explorer": "\u573a\u666f\u6d4f\u89c8\u5668"
  }
})json";
    }

    void bindAllLanguages(
        RobotQtViewerWorkbenchPackageRegistry& registry,
        IRobotQtViewerLanguageParticipant& participant)
    {
        for(const RobotQtViewerWorkbenchModeDesc& mode : registry.modes()) {
            require(
                registry.bindModeLanguageParticipant(
                    mode.descriptor.kind,
                    participant),
                "language participant binding failed");
        }
    }

    void runTest(QApplication& application)
    {
        QTemporaryDir directory;
        require(directory.isValid(), "temporary catalog directory is unavailable");
        writeCatalog(directory.filePath(QStringLiteral("base.en-US.i18n.json")), englishCatalog());
        writeCatalog(directory.filePath(QStringLiteral("base.zh-CN.i18n.json")), chineseCatalog());

        RobotQtViewerLocalizationService localization(directory.path());
        localization.installOnApplication(application);
        QString error;
        require(localization.reloadCatalogs(&error), error.toUtf8().constData());
        require(localization.availableLanguages().size() == 2, "language discovery failed");
        require(localization.setLanguage(QStringLiteral("zh-CN"), false, &error),
            error.toUtf8().constData());
        require(localization.text(QStringLiteral("shell.menu.file")) ==
                QString::fromWCharArray(L"\u6587\u4ef6"),
            "stable message translation failed");
        require(localization.text(QStringLiteral("missing"), QStringLiteral("Fallback")) ==
                QStringLiteral("Fallback"),
            "English fallback failed");
        require(localization.translateLegacyText(QStringLiteral("Selected robot: r1")) ==
                QString::fromWCharArray(
                    L"\u5df2\u9009\u62e9\u673a\u5668\u4eba\uff1ar1"),
            "template translation failed");

        QWidget root;
        auto* layout = new QVBoxLayout(&root);
        auto* label = new QLabel(QStringLiteral("Scene Explorer"), &root);
        auto* button = new QPushButton(QStringLiteral("Apply"), &root);
        auto* tree = new QTreeWidget(&root);
        tree->setHeaderLabels({ QStringLiteral("Scene Explorer") });
        layout->addWidget(label);
        layout->addWidget(button);
        layout->addWidget(tree);
        root.show();
        application.processEvents();
        localization.retranslateObjectTree(&root);
        require(label->text() ==
                QString::fromWCharArray(L"\u573a\u666f\u6d4f\u89c8\u5668"),
            "label translation failed");
        require(button->text() == QString::fromWCharArray(L"\u5e94\u7528"),
            "button translation failed");
        require(tree->headerItem()->text(0) ==
                QString::fromWCharArray(L"\u573a\u666f\u6d4f\u89c8\u5668"),
            "tree header translation failed");

        require(localization.setLanguage(QStringLiteral("en-US"), false, &error),
            error.toUtf8().constData());
        localization.retranslateObjectTree(&root);
        require(label->text() == QStringLiteral("Scene Explorer"),
            "language round trip failed");

        RobotQtViewerWorkbenchPackageRegistry missingRegistry =
            defaultRobotQtViewerWorkbenchPackageRegistry();
        require(!missingRegistry.validateEnabledModeLanguages(&error),
            "missing language participant was accepted");

        RobotQtViewerWorkbenchPackageRegistry registry =
            defaultRobotQtViewerWorkbenchPackageRegistry();
        RobotQtViewerNoOpLanguageParticipant noOp;
        bindAllLanguages(registry, noOp);
        require(registry.validateEnabledModeLanguages(&error), error.toUtf8().constData());

        RobotQtViewerWidgetLanguageParticipant shell(root);
        RobotQtViewerLanguageCoordinator coordinator(localization, registry);
        coordinator.registerShellParticipant(shell);
        require(coordinator.switchLanguage(QStringLiteral("zh-CN"), &error),
            error.toUtf8().constData());
        require(label->text() ==
                QString::fromWCharArray(L"\u573a\u666f\u6d4f\u89c8\u5668"),
            "coordinated translation failed");
        label->setText(QStringLiteral("Selected robot: r2"));
        application.processEvents();
        require(label->text() ==
                QString::fromWCharArray(
                    L"\u5df2\u9009\u62e9\u673a\u5668\u4eba\uff1ar2"),
            "dynamic text translation failed");
        QSettings().remove(QStringLiteral("ui/language"));
    }
}

int main(int argc, char* argv[])
{
    QApplication::setOrganizationName(QStringLiteral("RS2026-Test"));
    QApplication::setApplicationName(QStringLiteral("LocalizationTest"));
    QApplication application(argc, argv);
    try {
        runTest(application);
        std::cout << "RobotQtModulesShared localization regression passed.\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
