#pragma once

#include <QFileDialog>
#include <QString>

class QWidget;

namespace robot_qt_viewer
{
    enum class RobotQtViewerFileDialogBackend
    {
        Native,
        QtWidget
    };

    RobotQtViewerFileDialogBackend preferredFileDialogBackend() noexcept;

    // Compatibility helper. New callers should use a purpose-aware overload below.
    QFileDialog::Options fastFileDialogOptions(QFileDialog::Options options = QFileDialog::Options());

    QString getOpenFileName(
        const QString& purposeId,
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter = nullptr,
        QFileDialog::Options options = QFileDialog::Options());

    QString getOpenFileName(
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter = nullptr,
        QFileDialog::Options options = QFileDialog::Options());

    QString getSaveFileName(
        const QString& purposeId,
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter = nullptr,
        QFileDialog::Options options = QFileDialog::Options());

    QString getSaveFileName(
        QWidget* parent,
        const QString& caption,
        const QString& dir,
        const QString& filter,
        QString* selectedFilter = nullptr,
        QFileDialog::Options options = QFileDialog::Options());
}
