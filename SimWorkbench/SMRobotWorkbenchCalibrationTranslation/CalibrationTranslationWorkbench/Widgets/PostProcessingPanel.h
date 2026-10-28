#pragma once

#include <Models/CalibrationTranslationViewModel.h>

#include <QWidget>

class QLabel;
class QGroupBox;
class QPushButton;
class QTextEdit;
class QComboBox;
class QLineEdit;

namespace smrobot::workbench::spray::translation
{
    class PostProcessingPanel final : public QWidget
    {
        Q_OBJECT

    public:
        explicit PostProcessingPanel(QWidget* parent = nullptr);

        void setLanguageCode(const QString& languageCode);
        QString languageCode() const;
        void setViewModel(const CalibrationTranslationViewModel& viewModel);

    signals:
        void loadCurrentTrajectoryRequested(const QString& robotId);
        void generateRequested(
            const QString& templateName,
            const QString& robotId,
            const QString& programName,
            const QString& outputDirectory);

    private:
        void retranslate();
        void browseOutputDirectory();
        void updateActionState();

        QString m_languageCode{ QStringLiteral("en") };
        CalibrationTranslationViewModel m_viewModel;
        QGroupBox* m_settingsGroup{ nullptr };
        QGroupBox* m_codeGroup{ nullptr };
        QLabel* m_descriptionLabel{ nullptr };
        QLabel* m_statusLabel{ nullptr };
        QLabel* m_outputLabel{ nullptr };
        QLabel* m_warningLabel{ nullptr };
        QTextEdit* m_programEditor{ nullptr };
        QComboBox* m_robotCombo{ nullptr };
        QComboBox* m_templateCombo{ nullptr };
        QLineEdit* m_programNameEdit{ nullptr };
        QLineEdit* m_outputDirectoryEdit{ nullptr };
        QPushButton* m_browseButton{ nullptr };
        QPushButton* m_loadButton{ nullptr };
        QPushButton* m_generateButton{ nullptr };
    };
}
