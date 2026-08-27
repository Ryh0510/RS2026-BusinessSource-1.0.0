#pragma once

#include <QPointer>
#include <QObject>
#include <QString>
#include <QVector>

#include <memory>

class QApplication;
class QEvent;

namespace robot_qt_viewer
{
    class RobotQtViewerWorkbenchPackageRegistry;

    struct RobotQtViewerLanguageInfo
    {
        QString id;
        QString nativeName;
        QString fallbackId;
    };

    class RobotQtViewerLocalizationService final : public QObject
    {
    public:
        explicit RobotQtViewerLocalizationService(
            const QString& catalogDirectory,
            QObject* parent = nullptr);
        ~RobotQtViewerLocalizationService() override;

        bool reloadCatalogs(QString* errorMessage = nullptr);
        void installOnApplication(QApplication& application);
        static RobotQtViewerLocalizationService* installedOnApplication(
            const QApplication& application) noexcept;

        QVector<RobotQtViewerLanguageInfo> availableLanguages() const;
        QString currentLanguageId() const;
        QString savedLanguageId() const;
        QString languageNativeName(const QString& languageId) const;
        bool setLanguage(
            const QString& languageId,
            bool persist,
            QString* errorMessage = nullptr);

        QString text(const QString& key, const QString& englishFallback = QString()) const;
        QString translateLegacyText(const QString& text) const;
        void retranslateObjectTree(QObject* root) const;
        void retranslateApplication() const;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override;

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    class IRobotQtViewerLanguageParticipant
    {
    public:
        virtual ~IRobotQtViewerLanguageParticipant() = default;
        virtual void retranslateUi(
            const RobotQtViewerLocalizationService& localization) noexcept = 0;
    };

    class RobotQtViewerWidgetLanguageParticipant final
        : public IRobotQtViewerLanguageParticipant
    {
    public:
        RobotQtViewerWidgetLanguageParticipant() = default;
        explicit RobotQtViewerWidgetLanguageParticipant(QObject& root);

        void addRoot(QObject& root);
        void retranslateUi(
            const RobotQtViewerLocalizationService& localization) noexcept override;

    private:
        QVector<QPointer<QObject>> m_roots;
    };

    class RobotQtViewerNoOpLanguageParticipant final
        : public IRobotQtViewerLanguageParticipant
    {
    public:
        void retranslateUi(
            const RobotQtViewerLocalizationService& localization) noexcept override;
    };

    class RobotQtViewerLanguageCoordinator final
    {
    public:
        RobotQtViewerLanguageCoordinator(
            RobotQtViewerLocalizationService& localization,
            RobotQtViewerWorkbenchPackageRegistry& registry);

        void registerShellParticipant(IRobotQtViewerLanguageParticipant& participant);
        bool switchLanguage(
            const QString& languageId,
            QString* errorMessage = nullptr);
        bool retranslateCurrentLanguage(QString* errorMessage = nullptr);

    private:
        bool dispatch(QString* errorMessage);

        RobotQtViewerLocalizationService& m_localization;
        RobotQtViewerWorkbenchPackageRegistry& m_registry;
        QVector<IRobotQtViewerLanguageParticipant*> m_shellParticipants;
    };
}
