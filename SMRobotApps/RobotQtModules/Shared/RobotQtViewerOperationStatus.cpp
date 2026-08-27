#include "RobotQtViewerOperationStatus.h"

#include "RobotQtViewerEventHub.h"
#include "RobotQtViewerEvents.h"
#include "RobotQtViewerLocalization.h"

#include <QDateTime>

#include <algorithm>

namespace robot_qt_viewer
{
    namespace
    {
        bool isTerminal(RobotQtViewerOperationState state)
        {
            return state != RobotQtViewerOperationState::Running;
        }

        QString stateFallback(RobotQtViewerOperationState state)
        {
            switch(state) {
            case RobotQtViewerOperationState::Running:
                return QStringLiteral("In progress");
            case RobotQtViewerOperationState::Succeeded:
                return QStringLiteral("Succeeded");
            case RobotQtViewerOperationState::Failed:
                return QStringLiteral("Failed");
            case RobotQtViewerOperationState::Canceled:
                return QStringLiteral("Canceled");
            }
            return QStringLiteral("In progress");
        }

        QString stateKey(RobotQtViewerOperationState state)
        {
            switch(state) {
            case RobotQtViewerOperationState::Running:
                return QStringLiteral("status.operation.state.running");
            case RobotQtViewerOperationState::Succeeded:
                return QStringLiteral("status.operation.state.succeeded");
            case RobotQtViewerOperationState::Failed:
                return QStringLiteral("status.operation.state.failed");
            case RobotQtViewerOperationState::Canceled:
                return QStringLiteral("status.operation.state.canceled");
            }
            return QStringLiteral("status.operation.state.running");
        }
    }

    RobotQtViewerOperationStatusStore::RobotQtViewerOperationStatusStore(
        RobotQtViewerEventHub& eventHub)
        : m_eventHub(eventHub)
    {
    }

    QString RobotQtViewerOperationStatusStore::begin(
        const QString& sourceId,
        const QString& titleKey,
        const QString& titleFallback,
        const QString& target,
        int totalSteps,
        bool foreground,
        const QString& parentOperationId)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        RobotQtViewerOperationStatus status;
        status.operationId = QStringLiteral("operation-%1").arg(m_nextOperationId++);
        status.parentOperationId = parentOperationId;
        status.sourceId = sourceId;
        status.titleKey = titleKey;
        status.titleFallback = titleFallback;
        status.target = target;
        status.totalSteps = std::max(0, totalSteps);
        status.foreground = foreground;
        status.startedAtMs = now;
        status.updatedAtMs = now;
        m_history.push_back(status);
        trimHistory();
        publish(status);
        return status.operationId;
    }

    bool RobotQtViewerOperationStatusStore::reportProgress(
        const QString& operationId,
        const QString& stepKey,
        const QString& stepFallback,
        int stepIndex,
        int totalSteps,
        int progressValue,
        int progressMaximum)
    {
        const int index = indexOf(operationId);
        if(index < 0 || isTerminal(m_history[index].state)) {
            return false;
        }

        RobotQtViewerOperationStatus& status = m_history[index];
        if(stepIndex < status.stepIndex ||
            (progressValue >= 0 && status.progressValue >= 0 &&
             progressValue < status.progressValue)) {
            return false;
        }

        status.stepKey = stepKey;
        status.stepFallback = stepFallback;
        status.stepIndex = std::max(0, stepIndex);
        status.totalSteps = std::max(status.totalSteps, std::max(0, totalSteps));
        status.progressValue = progressValue;
        status.progressMaximum = progressMaximum;
        status.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
        status.elapsedMs = status.updatedAtMs - status.startedAtMs;
        publish(status);
        return true;
    }

    bool RobotQtViewerOperationStatusStore::succeed(
        const QString& operationId,
        const QString& resultKey,
        const QString& resultFallback)
    {
        return finish(
            operationId,
            RobotQtViewerOperationState::Succeeded,
            RobotQtViewerOperationSeverity::Info,
            resultKey,
            resultFallback,
            QString(),
            QString());
    }

    bool RobotQtViewerOperationStatusStore::fail(
        const QString& operationId,
        const QString& resultKey,
        const QString& resultFallback,
        const QString& diagnosticCode,
        const QString& diagnosticDetail)
    {
        return finish(
            operationId,
            RobotQtViewerOperationState::Failed,
            RobotQtViewerOperationSeverity::Error,
            resultKey,
            resultFallback,
            diagnosticCode,
            diagnosticDetail);
    }

    bool RobotQtViewerOperationStatusStore::cancel(
        const QString& operationId,
        const QString& resultKey,
        const QString& resultFallback)
    {
        return finish(
            operationId,
            RobotQtViewerOperationState::Canceled,
            RobotQtViewerOperationSeverity::Warning,
            resultKey,
            resultFallback,
            QString(),
            QString());
    }

    QVector<RobotQtViewerOperationStatus> RobotQtViewerOperationStatusStore::history() const
    {
        return m_history;
    }

    bool RobotQtViewerOperationStatusStore::status(
        const QString& operationId,
        RobotQtViewerOperationStatus* value) const
    {
        const int index = indexOf(operationId);
        if(index < 0 || value == nullptr) {
            return false;
        }
        *value = m_history[index];
        return true;
    }

    bool RobotQtViewerOperationStatusStore::finish(
        const QString& operationId,
        RobotQtViewerOperationState state,
        RobotQtViewerOperationSeverity severity,
        const QString& resultKey,
        const QString& resultFallback,
        const QString& diagnosticCode,
        const QString& diagnosticDetail)
    {
        const int index = indexOf(operationId);
        if(index < 0 || isTerminal(m_history[index].state) ||
            state == RobotQtViewerOperationState::Running) {
            return false;
        }

        RobotQtViewerOperationStatus& status = m_history[index];
        status.state = state;
        status.severity = severity;
        status.resultKey = resultKey;
        status.resultFallback = resultFallback;
        status.diagnosticCode = diagnosticCode;
        status.diagnosticDetail = diagnosticDetail;
        status.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
        status.elapsedMs = status.updatedAtMs - status.startedAtMs;
        if(state == RobotQtViewerOperationState::Succeeded && status.totalSteps > 0) {
            status.stepIndex = status.totalSteps;
            status.progressValue = status.progressMaximum > 0
                ? status.progressMaximum
                : status.totalSteps;
            status.progressMaximum = status.progressMaximum > 0
                ? status.progressMaximum
                : status.totalSteps;
        }
        publish(status);
        return true;
    }

    int RobotQtViewerOperationStatusStore::indexOf(const QString& operationId) const
    {
        for(int index = m_history.size() - 1; index >= 0; --index) {
            if(m_history[index].operationId == operationId) {
                return index;
            }
        }
        return -1;
    }

    void RobotQtViewerOperationStatusStore::publish(
        const RobotQtViewerOperationStatus& status)
    {
        RobotQtViewerEvent event;
        event.kind = RobotQtViewerEventKind::OperationStatusChanged;
        event.sourceId = status.sourceId;
        event.operationStatus = status;
        m_eventHub.publish(event);
    }

    void RobotQtViewerOperationStatusStore::trimHistory()
    {
        constexpr int kMaximumHistory = 100;
        while(m_history.size() > kMaximumHistory) {
            int removable = -1;
            for(int index = 0; index < m_history.size(); ++index) {
                if(isTerminal(m_history[index].state)) {
                    removable = index;
                    break;
                }
            }
            if(removable < 0) {
                break;
            }
            m_history.removeAt(removable);
        }
    }

    RobotQtViewerOperationStatusPresenter::RobotQtViewerOperationStatusPresenter(
        const RobotQtViewerLocalizationService& localization)
        : m_localization(localization)
    {
    }

    RobotQtViewerOperationStatusViewModel RobotQtViewerOperationStatusPresenter::build(
        const QVector<RobotQtViewerOperationStatus>& history,
        const QString& focusOperationId) const
    {
        RobotQtViewerOperationStatusViewModel viewModel;
        const RobotQtViewerOperationStatus* runningForeground = nullptr;
        for(auto it = history.crbegin(); it != history.crend(); ++it) {
            if(it->foreground && it->state == RobotQtViewerOperationState::Running) {
                runningForeground = &(*it);
                break;
            }
        }

        const RobotQtViewerOperationStatus* focus = nullptr;
        if(!focusOperationId.isEmpty()) {
            for(const RobotQtViewerOperationStatus& status : history) {
                if(status.operationId == focusOperationId) {
                    if(runningForeground == nullptr || status.foreground ||
                        status.state == RobotQtViewerOperationState::Failed) {
                        focus = &status;
                    }
                    break;
                }
            }
        }
        if(focus == nullptr) {
            focus = runningForeground;
        }
        if(focus == nullptr && !history.isEmpty()) {
            focus = &history.back();
        }

        if(focus != nullptr) {
            viewModel.operationId = focus->operationId;
            viewModel.currentMessage = summary(*focus);
            viewModel.timeoutMs = focus->state == RobotQtViewerOperationState::Running
                ? 0
                : (focus->state == RobotQtViewerOperationState::Failed ? 8000 : 3500);
            viewModel.progressVisible = focus->state == RobotQtViewerOperationState::Running &&
                focus->progressMaximum > 0;
            if(viewModel.progressVisible) {
                viewModel.progressMaximum = focus->progressMaximum;
                viewModel.progressValue = std::clamp(
                    focus->progressValue,
                    0,
                    focus->progressMaximum);
            }
        }

        const int first = std::max(0, history.size() - 100);
        for(int index = history.size() - 1; index >= first; --index) {
            viewModel.historyItems.push_back(historyItem(history[index]));
        }
        return viewModel;
    }

    QString RobotQtViewerOperationStatusPresenter::localized(
        const QString& key,
        const QString& fallback,
        const QStringList& arguments) const
    {
        QString value = m_localization.text(key, fallback);
        for(const QString& argument : arguments) {
            value = value.arg(argument);
        }
        return value;
    }

    QString RobotQtViewerOperationStatusPresenter::title(
        const RobotQtViewerOperationStatus& status) const
    {
        return localized(status.titleKey, status.titleFallback, { status.target });
    }

    QString RobotQtViewerOperationStatusPresenter::step(
        const RobotQtViewerOperationStatus& status) const
    {
        return localized(status.stepKey, status.stepFallback);
    }

    QString RobotQtViewerOperationStatusPresenter::summary(
        const RobotQtViewerOperationStatus& status) const
    {
        if(status.state != RobotQtViewerOperationState::Running) {
            return localized(status.resultKey, status.resultFallback, { status.target });
        }

        const QString operationTitle = title(status);
        const QString operationStep = step(status);
        QString value = operationStep.isEmpty()
            ? operationTitle
            : localized(
                QStringLiteral("status.operation.running.withStep"),
                QStringLiteral("%1 - %2"),
                { operationTitle, operationStep });
        if(status.totalSteps > 0 && status.stepIndex > 0) {
            value = localized(
                QStringLiteral("status.operation.running.stepProgress"),
                QStringLiteral("%1 (%2/%3)"),
                { value, QString::number(status.stepIndex), QString::number(status.totalSteps) });
        }
        return value;
    }

    QString RobotQtViewerOperationStatusPresenter::historyItem(
        const RobotQtViewerOperationStatus& status) const
    {
        const QString time = QDateTime::fromMSecsSinceEpoch(status.updatedAtMs)
            .toString(QStringLiteral("HH:mm:ss"));
        const QString state = localized(stateKey(status.state), stateFallback(status.state));
        QString detail = status.state == RobotQtViewerOperationState::Running
            ? summary(status)
            : localized(status.resultKey, status.resultFallback, { status.target });
        if(!status.diagnosticCode.isEmpty()) {
            detail += QStringLiteral(" [%1]").arg(status.diagnosticCode);
        }
        if(status.elapsedMs > 0) {
            detail += localized(
                QStringLiteral("status.operation.elapsed"),
                QStringLiteral(" - %1 ms"),
                { QString::number(status.elapsedMs) });
        }
        return localized(
            QStringLiteral("status.operation.historyItem"),
            QStringLiteral("%1  %2  %3"),
            { time, state, detail });
    }
}
