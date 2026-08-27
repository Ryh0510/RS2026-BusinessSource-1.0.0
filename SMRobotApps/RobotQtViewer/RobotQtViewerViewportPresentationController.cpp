#include "RobotQtViewerViewportPresentationController.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

namespace robot_qt_viewer
{
namespace
{
    class WindowPresentationTransition final
    {
    public:
        explicit WindowPresentationTransition(QMainWindow& window)
            : m_window(window)
            , m_updatesEnabled(window.updatesEnabled())
            , m_animated(window.isAnimated())
        {
            if(m_updatesEnabled) {
                m_window.setUpdatesEnabled(false);
            }
            m_window.setAnimated(false);
        }

        ~WindowPresentationTransition()
        {
            m_window.setAnimated(m_animated);
            if(m_updatesEnabled) {
                m_window.setUpdatesEnabled(true);
                m_window.update();
            }
        }

    private:
        QMainWindow& m_window;
        bool m_updatesEnabled = true;
        bool m_animated = true;
    };
}

RobotQtViewerViewportPresentationController::RobotQtViewerViewportPresentationController(
    QMainWindow& window,
    QWidget& viewport)
    : m_window(window)
    , m_viewport(viewport)
{
}

bool RobotQtViewerViewportPresentationController::isActive() const
{
    return m_active;
}

void RobotQtViewerViewportPresentationController::setActive(bool active)
{
    if(active == m_active) {
        return;
    }

    if(active) {
        enter();
    } else {
        leave();
    }
}

void RobotQtViewerViewportPresentationController::enter()
{
    m_savedWindowState = m_window.windowState() &
        Qt::WindowMaximized;
    m_savedWidgetVisibility.clear();

    {
        WindowPresentationTransition transition(m_window);
        rememberAndHide(m_window.menuBar());
        rememberAndHide(m_window.statusBar());

        const QList<QDockWidget*> dockWidgets =
            m_window.findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for(QDockWidget* dockWidget : dockWidgets) {
            rememberAndHide(dockWidget);
        }

        const QList<QToolBar*> toolBars =
            m_window.findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly);
        for(QToolBar* toolBar : toolBars) {
            rememberAndHide(toolBar);
        }

        m_active = true;
        m_window.showFullScreen();
    }
    m_viewport.setFocus(Qt::OtherFocusReason);
}

void RobotQtViewerViewportPresentationController::leave()
{
    const Qt::WindowStates savedWindowState = m_savedWindowState;
    const QVector<SavedWidgetVisibility> savedWidgetVisibility = m_savedWidgetVisibility;

    m_active = false;
    m_savedWindowState = Qt::WindowNoState;
    m_savedWidgetVisibility.clear();

    {
        WindowPresentationTransition transition(m_window);
        for(const SavedWidgetVisibility& saved : savedWidgetVisibility) {
            if(saved.widget != nullptr) {
                saved.widget->setVisible(saved.visible);
            }
        }

        if(savedWindowState.testFlag(Qt::WindowMaximized)) {
            m_window.showMaximized();
        } else {
            m_window.showNormal();
        }
    }

    m_viewport.setFocus(Qt::OtherFocusReason);
}

void RobotQtViewerViewportPresentationController::rememberAndHide(QWidget* widget)
{
    if(widget == nullptr || widget == &m_viewport) {
        return;
    }

    for(const SavedWidgetVisibility& saved : m_savedWidgetVisibility) {
        if(saved.widget == widget) {
            return;
        }
    }

    SavedWidgetVisibility saved;
    saved.widget = widget;
    saved.visible = widget->isVisible();
    m_savedWidgetVisibility.push_back(saved);
    widget->hide();
}
}
