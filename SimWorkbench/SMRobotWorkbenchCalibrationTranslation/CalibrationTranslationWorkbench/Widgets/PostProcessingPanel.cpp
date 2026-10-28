#include "PostProcessingPanel.h"

#include <RobotQtWidgetUtils.h>

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

namespace smrobot::workbench::spray::translation
{
    PostProcessingPanel::PostProcessingPanel(QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("calibrationPostProcessingPanel"));

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        m_settingsGroup = new QGroupBox(this);
        auto* settingsLayout = new QVBoxLayout(m_settingsGroup);
        settingsLayout->setContentsMargins(8, 20, 8, 8);
        settingsLayout->setSpacing(6);

        m_descriptionLabel = new QLabel(m_settingsGroup);
        m_descriptionLabel->setWordWrap(true);
        m_descriptionLabel->setObjectName(QStringLiteral("postProcessingDescription"));
        settingsLayout->addWidget(m_descriptionLabel);

        auto* form = new QFormLayout();
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(6);
        m_robotCombo = new QComboBox(m_settingsGroup);
        m_templateCombo = new QComboBox(m_settingsGroup);
        m_programNameEdit = new QLineEdit(m_settingsGroup);
        m_outputDirectoryEdit = new QLineEdit(m_settingsGroup);
        m_browseButton = new QPushButton(m_settingsGroup);
        m_robotCombo->setObjectName(QStringLiteral("postProcessingRobot"));
        m_templateCombo->setObjectName(QStringLiteral("postProcessingTemplate"));
        m_programNameEdit->setObjectName(QStringLiteral("postProcessingProgramName"));
        m_outputDirectoryEdit->setObjectName(QStringLiteral("postProcessingOutputDirectory"));
        m_browseButton->setObjectName(QStringLiteral("postProcessingBrowseOutput"));
        m_programNameEdit->setText(QStringLiteral("RotationBodyPath"));
        auto* outputRow = new QWidget(m_settingsGroup);
        auto* outputLayout = new QHBoxLayout(outputRow);
        outputLayout->setContentsMargins(0, 0, 0, 0);
        outputLayout->setSpacing(6);
        outputLayout->addWidget(m_outputDirectoryEdit, 1);
        outputLayout->addWidget(m_browseButton);
        form->addRow(QString(), m_robotCombo);
        form->addRow(QString(), m_templateCombo);
        form->addRow(QString(), m_programNameEdit);
        form->addRow(QString(), outputRow);
        settingsLayout->addLayout(form);

        auto* actionRow = new QHBoxLayout();
        actionRow->setContentsMargins(0, 0, 0, 0);
        actionRow->setSpacing(6);
        m_loadButton = new QPushButton(m_settingsGroup);
        m_generateButton = new QPushButton(m_settingsGroup);
        m_loadButton->setObjectName(QStringLiteral("postProcessingLoad"));
        m_generateButton->setObjectName(QStringLiteral("postProcessingGenerate"));
        robot_qt_viewer::configureInspectorButton(m_loadButton);
        robot_qt_viewer::configureActionButton(
            m_generateButton, robot_qt_viewer::UiActionRole::Primary);
        actionRow->addWidget(m_loadButton, 1);
        actionRow->addWidget(m_generateButton, 1);
        settingsLayout->addLayout(actionRow);

        m_statusLabel = new QLabel(m_settingsGroup);
        m_statusLabel->setWordWrap(true);
        m_statusLabel->setObjectName(QStringLiteral("postProcessingStatus"));
        m_statusLabel->setProperty("planningStatus", true);
        settingsLayout->addWidget(m_statusLabel);
        m_outputLabel = new QLabel(m_settingsGroup);
        m_outputLabel->setWordWrap(true);
        m_outputLabel->setObjectName(QStringLiteral("postProcessingOutput"));
        settingsLayout->addWidget(m_outputLabel);
        layout->addWidget(m_settingsGroup);

        m_codeGroup = new QGroupBox(this);
        auto* codeLayout = new QVBoxLayout(m_codeGroup);
        codeLayout->setContentsMargins(8, 20, 8, 8);
        m_programEditor = new QTextEdit(m_codeGroup);
        m_programEditor->setObjectName(QStringLiteral("postProcessingProgram"));
        m_programEditor->setReadOnly(true);
        m_programEditor->setAcceptRichText(false);
        m_programEditor->setLineWrapMode(QTextEdit::NoWrap);
        m_programEditor->setMinimumHeight(360);
        codeLayout->addWidget(m_programEditor);
        m_warningLabel = new QLabel(m_codeGroup);
        m_warningLabel->setWordWrap(true);
        m_warningLabel->setObjectName(QStringLiteral("postProcessingWarnings"));
        m_warningLabel->setProperty("planningStatus", true);
        codeLayout->addWidget(m_warningLabel);
        layout->addWidget(m_codeGroup, 1);

        connect(m_browseButton, &QPushButton::clicked,
            this, &PostProcessingPanel::browseOutputDirectory);
        connect(m_robotCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this]() { updateActionState(); });
        connect(m_templateCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this]() { updateActionState(); });
        connect(m_programNameEdit, &QLineEdit::textChanged,
            this, [this]() { updateActionState(); });
        connect(m_outputDirectoryEdit, &QLineEdit::textChanged,
            this, [this]() { updateActionState(); });
        connect(m_loadButton, &QPushButton::clicked, this, [this]() {
            emit loadCurrentTrajectoryRequested(
                m_robotCombo->currentData().toString());
        });
        connect(m_generateButton, &QPushButton::clicked, this, [this]() {
            emit generateRequested(
                m_templateCombo->currentData().toString(),
                m_robotCombo->currentData().toString(),
                m_programNameEdit->text().trimmed(),
                m_outputDirectoryEdit->text().trimmed());
        });

        retranslate();
    }

    void PostProcessingPanel::setLanguageCode(const QString& languageCode)
    {
        const QString normalized = languageCode.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)
            ? QStringLiteral("zh")
            : QStringLiteral("en");
        if(m_languageCode == normalized) return;
        m_languageCode = normalized;
        retranslate();
    }

    QString PostProcessingPanel::languageCode() const
    {
        return m_languageCode;
    }

    void PostProcessingPanel::setViewModel(const CalibrationTranslationViewModel& viewModel)
    {
        const QString selectedRobot = m_robotCombo->currentData().toString();
        const QString selectedTemplate = m_templateCombo->currentData().toString();
        const QSignalBlocker robotBlocker(m_robotCombo);
        const QSignalBlocker templateBlocker(m_templateCombo);
        m_robotCombo->clear();
        for(const PostProcessingRobotOption& robot : viewModel.postProcessingRobots) {
            m_robotCombo->addItem(
                QString::fromUtf8(robot.name.c_str()),
                QString::fromUtf8(robot.id.c_str()));
        }
        QString robotToSelect = selectedRobot;
        if(robotToSelect.isEmpty() && !viewModel.postProcessingRobotId.empty()) {
            robotToSelect = QString::fromUtf8(viewModel.postProcessingRobotId.c_str());
        }
        const int robotIndex = m_robotCombo->findData(robotToSelect);
        if(robotIndex >= 0) m_robotCombo->setCurrentIndex(robotIndex);

        m_templateCombo->clear();
        for(const std::string& name : viewModel.postProcessingTemplates) {
            m_templateCombo->addItem(
                QString::fromUtf8(name.c_str()),
                QString::fromUtf8(name.c_str()));
        }
        const int templateIndex = m_templateCombo->findData(selectedTemplate);
        if(templateIndex >= 0) {
            m_templateCombo->setCurrentIndex(templateIndex);
        } else {
            const int defaultIndex = m_templateCombo->findData(
                QStringLiteral("ABB_RAPID_IRC5"));
            if(defaultIndex >= 0) m_templateCombo->setCurrentIndex(defaultIndex);
        }
        if(m_outputDirectoryEdit->text().trimmed().isEmpty() &&
            !viewModel.rapidSettings.outputDirectory.empty()) {
            m_outputDirectoryEdit->setText(
                QString::fromUtf8(viewModel.rapidSettings.outputDirectory.c_str()));
        }
        m_viewModel = viewModel;
        const bool hasProgram = !viewModel.postProcessingPreview.empty();
        m_programEditor->setPlainText(
            QString::fromUtf8(viewModel.postProcessingPreview.c_str()));
        m_loadButton->setEnabled(viewModel.canLoadPostProcessingTrajectory);
        updateActionState();
        const bool exported = viewModel.postProcessingExportSucceeded &&
            !viewModel.postProcessingOutputFiles.empty();
        if(exported) {
            m_statusLabel->setText(m_languageCode == QStringLiteral("zh")
                ? QStringLiteral("控制器程序已生成并写入输出目录。")
                : QStringLiteral("The controller program was generated in the output directory."));
        } else if(viewModel.postProcessingTrajectoryLoaded) {
            m_statusLabel->setText(m_languageCode == QStringLiteral("zh")
                ? QStringLiteral("当前正向/返向轨迹已载入，全部轨迹点已获得 6 轴关节解。")
                : QStringLiteral("The current forward/return trajectory is loaded and all points have six-axis IK solutions."));
        } else if(!viewModel.postProcessingStatus.empty()) {
            m_statusLabel->setText(QString::fromUtf8(viewModel.postProcessingStatus.c_str()));
        } else {
            m_statusLabel->setText(m_languageCode == QStringLiteral("zh")
                ? QStringLiteral("请选择机器人并加载当前轨迹。")
                : QStringLiteral("Select a robot and load the current trajectory."));
        }
        if(m_outputLabel != nullptr) {
            if(exported) {
                QStringList files;
                for(const std::string& file : viewModel.postProcessingOutputFiles) {
                    files.push_back(QString::fromUtf8(file.c_str()));
                }
                m_outputLabel->setText((m_languageCode == QStringLiteral("zh")
                    ? QStringLiteral("输出文件：\n")
                    : QStringLiteral("Output files:\n")) + files.join(QStringLiteral("\n")));
            } else {
                m_outputLabel->clear();
            }
        }
        m_warningLabel->setText(!viewModel.postProcessingTrajectoryLoaded && hasProgram
            ? QString::fromUtf8(viewModel.postProcessingStatus.c_str())
            : QString());
    }

    void PostProcessingPanel::browseOutputDirectory()
    {
        const QString path = QFileDialog::getExistingDirectory(
            this,
            m_languageCode == QStringLiteral("zh")
                ? QStringLiteral("选择后处理输出目录")
                : QStringLiteral("Choose post-processing output directory"),
            m_outputDirectoryEdit->text());
        if(!path.isEmpty()) {
            m_outputDirectoryEdit->setText(path);
        }
    }

    void PostProcessingPanel::updateActionState()
    {
        const QString selectedRobot = m_robotCombo->currentData().toString();
        const QString loadedRobot =
            QString::fromUtf8(m_viewModel.postProcessingRobotId.c_str());
        const bool inputsValid = !selectedRobot.isEmpty() &&
            !m_templateCombo->currentData().toString().isEmpty() &&
            !m_programNameEdit->text().trimmed().isEmpty() &&
            !m_outputDirectoryEdit->text().trimmed().isEmpty();
        const bool robotMatchesLoadedTrajectory = loadedRobot.isEmpty() ||
            selectedRobot == loadedRobot;
        m_generateButton->setEnabled(
            m_viewModel.canExportPostProcessedProgram &&
            inputsValid &&
            robotMatchesLoadedTrajectory);
    }

    void PostProcessingPanel::retranslate()
    {
        const bool chinese = m_languageCode == QStringLiteral("zh");
        if(m_settingsGroup != nullptr) {
            m_settingsGroup->setTitle(chinese ? QStringLiteral("后处理") : QStringLiteral("Post Processing"));
        }
        if(m_codeGroup != nullptr) {
            m_codeGroup->setTitle(chinese
                ? QStringLiteral("中间指令 / 控制器程序预览")
                : QStringLiteral("Intermediate / controller program preview"));
        }
        if(m_descriptionLabel != nullptr) {
            m_descriptionLabel->setText(chinese
                ? QStringLiteral("先在回转体轨迹规划中生成并保存正向/返向轨迹，再在此求解机器人关节角并使用所选厂商模板生成控制器程序。ABB 指令序列可选；未设置时按轨迹组顺序导出。")
                : QStringLiteral("Generate and save the forward/return trajectories in Rotation Body Planning first. This panel solves robot joints and generates a controller program with the selected vendor template. The ABB instruction sequence is optional; without it, the saved trajectory group order is exported."));
        }
        const auto setRowLabel = [this](QWidget* field, const QString& text) {
            auto* settingsLayout = qobject_cast<QVBoxLayout*>(m_settingsGroup->layout());
            if(settingsLayout == nullptr) return;
            for(int index = 0; index < settingsLayout->count(); ++index) {
                auto* form = qobject_cast<QFormLayout*>(settingsLayout->itemAt(index)->layout());
                if(form != nullptr) {
                    if(QWidget* label = form->labelForField(field)) {
                        if(QLabel* formLabel = qobject_cast<QLabel*>(label)) {
                            formLabel->setText(text);
                        }
                    }
                }
            }
        };
        setRowLabel(m_robotCombo, chinese ? QStringLiteral("机器人") : QStringLiteral("Robot"));
        setRowLabel(m_templateCombo, chinese ? QStringLiteral("模板") : QStringLiteral("Template"));
        setRowLabel(m_programNameEdit, chinese ? QStringLiteral("程序名") : QStringLiteral("Program name"));
        setRowLabel(m_outputDirectoryEdit->parentWidget(), chinese ? QStringLiteral("输出目录") : QStringLiteral("Output directory"));
        m_browseButton->setText(chinese ? QStringLiteral("浏览...") : QStringLiteral("Browse..."));
        if(m_loadButton != nullptr) {
            m_loadButton->setText(chinese ? QStringLiteral("加载当前轨迹") : QStringLiteral("Load current trajectory"));
        }
        if(m_generateButton != nullptr) {
            m_generateButton->setText(chinese ? QStringLiteral("生成控制器程序") : QStringLiteral("Generate controller program"));
        }
        setViewModel(m_viewModel);
    }
}
