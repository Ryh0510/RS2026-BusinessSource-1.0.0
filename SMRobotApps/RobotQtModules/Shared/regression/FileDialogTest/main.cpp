#include <RobotQtViewerFileDialog.h>
#include <RobotQtViewerLocalization.h>

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QListView>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

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

    QString normalizedDirectory(const QString& path)
    {
        const QString canonical = QDir(path).canonicalPath();
        return canonical.isEmpty() ? QDir(path).absolutePath() : canonical;
    }

    class DialogObserver final : public QObject
    {
    public:
        void prepare(
            const QString& selectedPath,
            bool accept,
            const QString& expectedDirectory,
            const QString& expectedTitle,
            const QString& expectedFilter)
        {
            m_selectedPath = selectedPath;
            m_accept = accept;
            m_expectedDirectory = expectedDirectory;
            m_expectedTitle = expectedTitle;
            m_expectedFilter = expectedFilter;
            m_seen = false;
            m_valid = false;
        }

        bool seen() const noexcept
        {
            return m_seen;
        }

        bool valid() const noexcept
        {
            return m_valid;
        }

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if(event->type() != QEvent::Show) {
                return QObject::eventFilter(watched, event);
            }
            QFileDialog* dialog = qobject_cast<QFileDialog*>(watched);
            if(dialog == nullptr) {
                return QObject::eventFilter(watched, event);
            }

            m_seen = true;
            bool buttonsAreUsable = true;
            const QList<QToolButton*> buttons = dialog->findChildren<QToolButton*>();
            for(const QToolButton* button : buttons) {
                buttonsAreUsable = buttonsAreUsable &&
                    button->minimumWidth() >= 36 && button->minimumHeight() >= 36;
            }
            const QListView* sidebar =
                dialog->findChild<QListView*>(QStringLiteral("sidebar"));
            const bool directoryMatches = m_expectedDirectory.isEmpty() ||
                normalizedDirectory(dialog->directory().absolutePath()) ==
                    normalizedDirectory(m_expectedDirectory);
            m_valid = dialog->options().testFlag(QFileDialog::DontUseNativeDialog) &&
                dialog->viewMode() == QFileDialog::Detail &&
                dialog->minimumWidth() >= 760 && dialog->minimumHeight() >= 520 &&
                dialog->sidebarUrls().size() >= 2 &&
                sidebar != nullptr && sidebar->minimumWidth() >= 190 &&
                !buttons.isEmpty() && buttonsAreUsable && directoryMatches &&
                dialog->windowTitle() == m_expectedTitle &&
                dialog->nameFilters().contains(m_expectedFilter);

            QTimer::singleShot(0, dialog, [this, dialog]() {
                if(m_accept) {
                    dialog->selectFile(m_selectedPath);
                    static_cast<QDialog*>(dialog)->accept();
                } else {
                    dialog->reject();
                }
            });
            return QObject::eventFilter(watched, event);
        }

    private:
        QString m_selectedPath;
        QString m_expectedDirectory;
        QString m_expectedTitle;
        QString m_expectedFilter;
        bool m_accept = false;
        bool m_seen = false;
        bool m_valid = false;
    };

    void writeCatalog(const QString& path)
    {
        QFile file(path);
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate),
            "Failed to create localization catalog.");
        const QByteArray json = R"JSON({
  "schema": "smrobot.localization.catalog",
  "version": 1,
  "catalogId": "test.fileDialog",
  "locale": "en-US",
  "nativeName": "English",
  "fallbackLocale": "",
  "messages": {
    "fileDialog.test.localized.title": "Localized open title",
    "fileDialog.test.localized.filter": "Localized JSON (*.json)"
  },
  "legacy": {}
})JSON";
        require(file.write(json) == json.size(), "Failed to write localization catalog.");
    }

    void runTest(QApplication& application)
    {
        QTemporaryDir temporaryDirectory;
        require(temporaryDirectory.isValid(), "Failed to create temporary directory.");

        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(
            QSettings::IniFormat,
            QSettings::UserScope,
            temporaryDirectory.path());
        QSettings().clear();

        qunsetenv("SMROBOT_FILE_DIALOG_BACKEND");
#ifdef Q_OS_WIN
        require(
            robot_qt_viewer::preferredFileDialogBackend() ==
                robot_qt_viewer::RobotQtViewerFileDialogBackend::Native,
            "Windows must default to the native file dialog.");
        require(
            !robot_qt_viewer::fastFileDialogOptions().testFlag(
                QFileDialog::DontUseNativeDialog),
            "Windows default options must preserve the native file dialog.");
#else
        require(
            robot_qt_viewer::preferredFileDialogBackend() ==
                robot_qt_viewer::RobotQtViewerFileDialogBackend::QtWidget,
            "Non-Windows platforms must default to the Qt Widget file dialog.");
#endif

        qputenv("SMROBOT_FILE_DIALOG_BACKEND", QByteArrayLiteral("qt"));
        const QFileDialog::Options qtOptions =
            robot_qt_viewer::fastFileDialogOptions();
        require(qtOptions.testFlag(QFileDialog::DontUseNativeDialog),
            "Qt backend must disable the native file dialog.");
        require(qtOptions.testFlag(QFileDialog::DontUseCustomDirectoryIcons),
            "Qt backend must disable custom directory icons.");

        const QString catalogPath =
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("base.i18n.json"));
        writeCatalog(catalogPath);
        robot_qt_viewer::RobotQtViewerLocalizationService localization(
            temporaryDirectory.path());
        QString error;
        require(localization.reloadCatalogs(&error), "Failed to load test catalog.");
        require(localization.setLanguage(QStringLiteral("en-US"), false, &error),
            "Failed to select test language.");
        localization.installOnApplication(application);

        const QString selectedPath =
            QDir(temporaryDirectory.path()).filePath(QStringLiteral("selected.json"));
        QFile selectedFile(selectedPath);
        require(selectedFile.open(QIODevice::WriteOnly), "Failed to create selected file.");
        selectedFile.close();

        DialogObserver observer;
        application.installEventFilter(&observer);
        observer.prepare(
            selectedPath,
            true,
            temporaryDirectory.path(),
            QStringLiteral("Localized open title"),
            QStringLiteral("Localized JSON (*.json)"));
        const QString result = robot_qt_viewer::getOpenFileName(
            QStringLiteral("test.localized"),
            nullptr,
            QStringLiteral("Fallback title"),
            temporaryDirectory.path(),
            QStringLiteral("Fallback (*.txt)"));
        require(observer.seen() && observer.valid(),
            "Enhanced Qt file dialog presentation is invalid.");
        require(QFileInfo(result).absoluteFilePath() == QFileInfo(selectedPath).absoluteFilePath(),
            "Accepted file path was not returned.");

        observer.prepare(
            QString(),
            false,
            temporaryDirectory.path(),
            QStringLiteral("Localized open title"),
            QStringLiteral("Localized JSON (*.json)"));
        const QString canceled = robot_qt_viewer::getOpenFileName(
            QStringLiteral("test.localized"),
            nullptr,
            QStringLiteral("Fallback title"),
            QString(),
            QStringLiteral("Fallback (*.txt)"));
        require(canceled.isEmpty(), "Canceled file dialog must return an empty path.");
        require(observer.seen() && observer.valid(),
            "Remembered purpose directory was not restored.");
        application.removeEventFilter(&observer);
        qunsetenv("SMROBOT_FILE_DIALOG_BACKEND");
        QSettings().clear();
    }
}

int main(int argc, char* argv[])
{
    QApplication::setOrganizationName(QStringLiteral("RS2026-Test"));
    QApplication::setApplicationName(QStringLiteral("FileDialogTest"));
    QApplication application(argc, argv);
    try {
        runTest(application);
        std::cout << "RobotQtModulesShared file dialog regression passed.\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "RobotQtModulesShared file dialog regression failed: "
                  << error.what() << '\n';
        return 1;
    }
}
