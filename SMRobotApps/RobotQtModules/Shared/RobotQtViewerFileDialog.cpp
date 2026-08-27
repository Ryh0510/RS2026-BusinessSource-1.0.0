#include "RobotQtViewerFileDialog.h"

#include "RobotQtViewerLocalization.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QListView>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>

#include <algorithm>

namespace robot_qt_viewer
{
namespace
{
    constexpr const char* kForceQtBackendEnvironment =
        "SMROBOT_FILE_DIALOG_BACKEND";
    constexpr const char* kGenericPurposeId = "generic";

    QString normalizedPurposeId(const QString& purposeId)
    {
        QString normalized = purposeId.trimmed();
        for(QChar& character : normalized) {
            const bool accepted = character.isLetterOrNumber() ||
                character == QLatin1Char('.') || character == QLatin1Char('-') ||
                character == QLatin1Char('_');
            if(!accepted) {
                character = QLatin1Char('-');
            }
        }
        return normalized.isEmpty() ? QString::fromLatin1(kGenericPurposeId) : normalized;
    }

    QString settingsKey(const QString& purposeId)
    {
        return QStringLiteral("ui/fileDialogs/%1/lastDirectory")
            .arg(normalizedPurposeId(purposeId));
    }

    QString localizeDialogText(
        const QString& purposeId,
        const QString& field,
        const QString& fallback)
    {
        if(qApp == nullptr) {
            return fallback;
        }
        RobotQtViewerLocalizationService* localization =
            RobotQtViewerLocalizationService::installedOnApplication(*qApp);
        if(localization == nullptr) {
            return fallback;
        }
        return localization->text(
            QStringLiteral("fileDialog.%1.%2")
                .arg(normalizedPurposeId(purposeId), field),
            fallback);
    }

    QString usableInitialPath(const QString& requestedPath)
    {
        if(requestedPath.trimmed().isEmpty()) {
            return QString();
        }

        const QFileInfo requested(requestedPath);
        if(requested.exists() || requested.dir().exists()) {
            return requestedPath;
        }
        return QString();
    }

    QString fallbackDirectory()
    {
        const QString documents =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if(QFileInfo(documents).isDir()) {
            return documents;
        }
        const QString home = QDir::homePath();
        return QFileInfo(home).isDir() ? home : QDir::currentPath();
    }

    QString initialPathForPurpose(
        const QString& purposeId,
        const QString& requestedPath)
    {
        const QString usableRequested = usableInitialPath(requestedPath);
        if(!usableRequested.isEmpty()) {
            return usableRequested;
        }

        const QString remembered = QSettings().value(settingsKey(purposeId)).toString();
        if(QFileInfo(remembered).isDir()) {
            return remembered;
        }
        return fallbackDirectory();
    }

    QString directoryForPath(const QString& path)
    {
        if(path.trimmed().isEmpty()) {
            return QString();
        }
        const QFileInfo info(path);
        if(info.isDir()) {
            return info.absoluteFilePath();
        }
        if(info.dir().exists()) {
            return info.absolutePath();
        }
        return QString();
    }

    void rememberSelectedDirectory(
        const QString& purposeId,
        const QString& selectedPath)
    {
        const QString directory = directoryForPath(selectedPath);
        if(!directory.isEmpty()) {
            QSettings().setValue(settingsKey(purposeId), directory);
        }
    }

    void appendLocalSidebarUrl(
        QList<QUrl>& urls,
        QSet<QString>& identities,
        const QString& path)
    {
        QString directory = directoryForPath(path);
        if(directory.isEmpty()) {
            return;
        }
        directory = QDir(directory).absolutePath();
#ifdef Q_OS_WIN
        const QString identity = directory.toCaseFolded();
#else
        const QString identity = directory;
#endif
        if(identities.contains(identity)) {
            return;
        }
        identities.insert(identity);
        urls.push_back(QUrl::fromLocalFile(directory));
    }

    QList<QUrl> sidebarUrls(
        const QString& purposeId,
        const QString& initialPath)
    {
        QList<QUrl> urls;
        QSet<QString> identities;
        appendLocalSidebarUrl(urls, identities, initialPath);
        appendLocalSidebarUrl(
            urls,
            identities,
            QSettings().value(settingsKey(purposeId)).toString());
        appendLocalSidebarUrl(urls, identities, QDir::homePath());
        appendLocalSidebarUrl(
            urls,
            identities,
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
        appendLocalSidebarUrl(
            urls,
            identities,
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        appendLocalSidebarUrl(
            urls,
            identities,
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        appendLocalSidebarUrl(urls, identities, QDir::rootPath());
        return urls;
    }

    void configureQtWidgetDialog(
        QFileDialog& dialog,
        const QString& purposeId,
        const QString& initialPath)
    {
        dialog.setViewMode(QFileDialog::Detail);
        dialog.setSupportedSchemes({ QStringLiteral("file") });
        const QList<QUrl> urls = sidebarUrls(purposeId, initialPath);
        dialog.setSidebarUrls(urls);

        QStringList history;
        history.reserve(urls.size());
        for(const QUrl& url : urls) {
            history.push_back(url.toLocalFile());
        }
        dialog.setHistory(history);
        dialog.setMinimumSize(QSize(760, 520));

        QSize desiredSize(1000, 680);
        QScreen* screen = dialog.screen();
        if(screen != nullptr) {
            const QSize available = screen->availableGeometry().size();
            desiredSize.setWidth(std::min(desiredSize.width(), available.width() - 80));
            desiredSize.setHeight(std::min(desiredSize.height(), available.height() - 80));
        }
        desiredSize = desiredSize.expandedTo(dialog.minimumSize());
        dialog.resize(desiredSize);

        if(QListView* sidebar = dialog.findChild<QListView*>(QStringLiteral("sidebar"))) {
            sidebar->setMinimumWidth(190);
            sidebar->setIconSize(QSize(22, 22));
        }
        for(QTreeView* tree : dialog.findChildren<QTreeView*>()) {
            tree->setAlternatingRowColors(true);
        }
        for(QToolButton* button : dialog.findChildren<QToolButton*>()) {
            button->setMinimumSize(QSize(36, 36));
            button->setIconSize(button->iconSize().expandedTo(QSize(22, 22)));
        }
    }

    bool canOpenDialog()
    {
        if(qApp == nullptr) {
            qWarning() << "File dialog requires a QApplication.";
            return false;
        }
        if(QThread::currentThread() != qApp->thread()) {
            qWarning() << "File dialog must be opened on the Qt GUI thread.";
            return false;
        }
        return true;
    }

    QString executeQtWidgetDialog(
        const QString& purposeId,
        QWidget* parent,
        const QString& caption,
        const QString& initialPath,
        const QString& filter,
        QString* selectedFilter,
        QFileDialog::Options options,
        QFileDialog::AcceptMode acceptMode)
    {
        QFileDialog dialog(parent, caption, initialPath, filter);
        dialog.setOptions(options);
        dialog.setAcceptMode(acceptMode);
        dialog.setFileMode(
            acceptMode == QFileDialog::AcceptOpen
                ? QFileDialog::ExistingFile
                : QFileDialog::AnyFile);
        if(selectedFilter != nullptr && !selectedFilter->isEmpty()) {
            dialog.selectNameFilter(*selectedFilter);
        }
        configureQtWidgetDialog(dialog, purposeId, initialPath);

        if(dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
            return QString();
        }
        if(selectedFilter != nullptr) {
            *selectedFilter = dialog.selectedNameFilter();
        }
        return dialog.selectedFiles().front();
    }
}

    RobotQtViewerFileDialogBackend preferredFileDialogBackend() noexcept
    {
        const QString override =
            qEnvironmentVariable(kForceQtBackendEnvironment).trimmed().toLower();
        if(override == QStringLiteral("qt") ||
            override == QStringLiteral("widget") ||
            override == QStringLiteral("non-native")) {
            return RobotQtViewerFileDialogBackend::QtWidget;
        }
#ifdef Q_OS_WIN
        return RobotQtViewerFileDialogBackend::Native;
#else
        return RobotQtViewerFileDialogBackend::QtWidget;
#endif
    }

    QFileDialog::Options fastFileDialogOptions(QFileDialog::Options options)
    {
        if(preferredFileDialogBackend() == RobotQtViewerFileDialogBackend::QtWidget ||
            options.testFlag(QFileDialog::DontUseNativeDialog)) {
            options |= QFileDialog::DontUseNativeDialog;
            options |= QFileDialog::DontUseCustomDirectoryIcons;
        }
        return options;
    }

    QString getOpenFileName(
        const QString& purposeId,
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter,
        QFileDialog::Options options)
    {
        if(!canOpenDialog()) {
            return QString();
        }
        const QString initialPath = initialPathForPurpose(purposeId, dir);
        const QString localizedCaption =
            localizeDialogText(purposeId, QStringLiteral("title"), caption);
        const QString localizedFilter =
            localizeDialogText(purposeId, QStringLiteral("filter"), filter);
        const QFileDialog::Options effectiveOptions = fastFileDialogOptions(options);

        QString selected;
        if(effectiveOptions.testFlag(QFileDialog::DontUseNativeDialog)) {
            selected = executeQtWidgetDialog(
                purposeId,
                parent,
                localizedCaption,
                initialPath,
                localizedFilter,
                selectedFilter,
                effectiveOptions,
                QFileDialog::AcceptOpen);
        } else {
            selected = QFileDialog::getOpenFileName(
                parent,
                localizedCaption,
                initialPath,
                localizedFilter,
                selectedFilter,
                effectiveOptions);
        }
        if(!selected.isEmpty()) {
            rememberSelectedDirectory(purposeId, selected);
        }
        return selected;
    }

    QString getOpenFileName(
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter,
        QFileDialog::Options options)
    {
        return getOpenFileName(
            QString::fromLatin1(kGenericPurposeId),
            parent,
            caption,
            dir,
            filter,
            selectedFilter,
            options);
    }

    QString getSaveFileName(
        const QString& purposeId,
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter,
        QFileDialog::Options options)
    {
        if(!canOpenDialog()) {
            return QString();
        }
        const QString initialPath = initialPathForPurpose(purposeId, dir);
        const QString localizedCaption =
            localizeDialogText(purposeId, QStringLiteral("title"), caption);
        const QString localizedFilter =
            localizeDialogText(purposeId, QStringLiteral("filter"), filter);
        const QFileDialog::Options effectiveOptions = fastFileDialogOptions(options);

        QString selected;
        if(effectiveOptions.testFlag(QFileDialog::DontUseNativeDialog)) {
            selected = executeQtWidgetDialog(
                purposeId,
                parent,
                localizedCaption,
                initialPath,
                localizedFilter,
                selectedFilter,
                effectiveOptions,
                QFileDialog::AcceptSave);
        } else {
            selected = QFileDialog::getSaveFileName(
                parent,
                localizedCaption,
                initialPath,
                localizedFilter,
                selectedFilter,
                effectiveOptions);
        }
        if(!selected.isEmpty()) {
            rememberSelectedDirectory(purposeId, selected);
        }
        return selected;
    }

    QString getSaveFileName(
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter,
        QFileDialog::Options options)
    {
        return getSaveFileName(
            QString::fromLatin1(kGenericPurposeId),
            parent,
            caption,
            dir,
            filter,
            selectedFilter,
            options);
    }
}
