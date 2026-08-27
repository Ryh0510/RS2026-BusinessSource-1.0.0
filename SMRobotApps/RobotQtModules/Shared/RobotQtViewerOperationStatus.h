#pragma once

#include <QVector>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace robot_qt_viewer
{
    class RobotQtViewerEventHub;
    class RobotQtViewerLocalizationService;

    enum class RobotQtViewerOperationState
    {
        Running,
        Succeeded,
        Failed,
        Canceled
    };

    enum class RobotQtViewerOperationSeverity
    {
        Info,
        Warning,
        Error
    };

    struct RobotQtViewerOperationStatus
    {
        QString operationId;
        QString parentOperationId;
        QString sourceId;
        QString titleKey;
        QString titleFallback;
        QString target;
        QString stepKey;
        QString stepFallback;
        QString resultKey;
        QString resultFallback;
        QString diagnosticCode;
        QString diagnosticDetail;
        RobotQtViewerOperationState state = RobotQtViewerOperationState::Running;
        RobotQtViewerOperationSeverity severity = RobotQtViewerOperationSeverity::Info;
        int stepIndex = 0;
        int totalSteps = 0;
        int progressValue = -1;
        int progressMaximum = -1;
        bool foreground = true;
        qint64 startedAtMs = 0;
        qint64 updatedAtMs = 0;
        qint64 elapsedMs = 0;
    };

    class RobotQtViewerOperationStatusStore
    {
    public:
        explicit RobotQtViewerOperationStatusStore(RobotQtViewerEventHub& eventHub);

        QString begin(
            const QString& sourceId,
            const QString& titleKey,
            const QString& titleFallback,
            const QString& target = QString(),
            int totalSteps = 0,
            bool foreground = true,
            const QString& parentOperationId = QString());
        bool reportProgress(
            const QString& operationId,
            const QString& stepKey,
            const QString& stepFallback,
            int stepIndex,
            int totalSteps,
            int progressValue = -1,
            int progressMaximum = -1);
        bool succeed(
            const QString& operationId,
            const QString& resultKey,
            const QString& resultFallback);
        bool fail(
            const QString& operationId,
            const QString& resultKey,
            const QString& resultFallback,
            const QString& diagnosticCode = QString(),
            const QString& diagnosticDetail = QString());
        bool cancel(
            const QString& operationId,
            const QString& resultKey,
            const QString& resultFallback);

        QVector<RobotQtViewerOperationStatus> history() const;
        bool status(const QString& operationId, RobotQtViewerOperationStatus* value) const;

    private:
        bool finish(
            const QString& operationId,
            RobotQtViewerOperationState state,
            RobotQtViewerOperationSeverity severity,
            const QString& resultKey,
            const QString& resultFallback,
            const QString& diagnosticCode,
            const QString& diagnosticDetail);
        int indexOf(const QString& operationId) const;
        void publish(const RobotQtViewerOperationStatus& status);
        void trimHistory();

        RobotQtViewerEventHub& m_eventHub;
        QVector<RobotQtViewerOperationStatus> m_history;
        std::uint64_t m_nextOperationId = 1;
    };

    struct RobotQtViewerOperationStatusViewModel
    {
        QString operationId;
        QString currentMessage;
        QStringList historyItems;
        int timeoutMs = 0;
        bool progressVisible = false;
        int progressMinimum = 0;
        int progressMaximum = 0;
        int progressValue = 0;
    };

    class RobotQtViewerOperationStatusPresenter
    {
    public:
        explicit RobotQtViewerOperationStatusPresenter(
            const RobotQtViewerLocalizationService& localization);

        RobotQtViewerOperationStatusViewModel build(
            const QVector<RobotQtViewerOperationStatus>& history,
            const QString& focusOperationId = QString()) const;

    private:
        QString localized(
            const QString& key,
            const QString& fallback,
            const QStringList& arguments = {}) const;
        QString title(const RobotQtViewerOperationStatus& status) const;
        QString step(const RobotQtViewerOperationStatus& status) const;
        QString summary(const RobotQtViewerOperationStatus& status) const;
        QString historyItem(const RobotQtViewerOperationStatus& status) const;

        const RobotQtViewerLocalizationService& m_localization;
    };
}
