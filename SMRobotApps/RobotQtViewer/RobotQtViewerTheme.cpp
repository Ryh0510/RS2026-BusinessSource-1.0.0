#include "RobotQtViewerTheme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QWidget>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#endif

namespace robot_qt_viewer
{
    namespace
    {
        constexpr const char* kSettingsKey = "ui/theme";

        QFont applicationUiFont()
        {
            QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
            QFontDatabase database;

#ifdef Q_OS_WIN
            const QStringList preferredFamilies{
                QStringLiteral("Segoe UI Variable Text"),
                QStringLiteral("Segoe UI")
            };
#elif defined(Q_OS_LINUX)
            const QStringList preferredFamilies{
                QStringLiteral("Noto Sans"),
                QStringLiteral("Ubuntu"),
                QStringLiteral("DejaVu Sans")
            };
#else
            const QStringList preferredFamilies;
#endif

            const QStringList availableFamilies = database.families();
            for(const QString& family : preferredFamilies) {
                if(availableFamilies.contains(family, Qt::CaseInsensitive)) {
                    font.setFamily(family);
                    break;
                }
            }
            font.setStyleHint(QFont::SansSerif);
            return font;
        }

#ifdef Q_OS_WIN
        constexpr DWORD kDwmUseImmersiveDarkMode = 20;
        constexpr DWORD kDwmUseImmersiveDarkModeBefore20H1 = 19;
        constexpr DWORD kDwmWindowBorderColor = 34;
        constexpr DWORD kDwmCaptionColor = 35;
        constexpr DWORD kDwmTextColor = 36;

        COLORREF toColorRef(const QColor& color)
        {
            return RGB(color.red(), color.green(), color.blue());
        }

        void setDwmColor(HWND handle, DWORD attribute, const QColor& color)
        {
            const COLORREF colorRef = toColorRef(color);
            DwmSetWindowAttribute(handle, attribute, &colorRef, sizeof(colorRef));
        }
#endif

        QPalette makePalette(
            const QColor& window,
            const QColor& panel,
            const QColor& text,
            const QColor& disabledText,
            const QColor& base,
            const QColor& alternateBase,
            const QColor& button,
            const QColor& highlight,
            const QColor& highlightedText)
        {
            QPalette palette;
            palette.setColor(QPalette::Window, window);
            palette.setColor(QPalette::WindowText, text);
            palette.setColor(QPalette::Base, base);
            palette.setColor(QPalette::AlternateBase, alternateBase);
            palette.setColor(QPalette::ToolTipBase, panel);
            palette.setColor(QPalette::ToolTipText, text);
            palette.setColor(QPalette::Text, text);
            palette.setColor(QPalette::Button, button);
            palette.setColor(QPalette::ButtonText, text);
            palette.setColor(QPalette::BrightText, QColor(255, 80, 80));
            palette.setColor(QPalette::Highlight, highlight);
            palette.setColor(QPalette::HighlightedText, highlightedText);
            palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
            palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
            palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
            return palette;
        }

        QString darkStyleSheet(
            const QString& window,
            const QString& panel,
            const QString& base,
            const QString& alternate,
            const QString& border,
            const QString& text,
            const QString& muted,
            const QString& accent,
            const QString& button,
            const QString& buttonHover,
            const QString& selection,
            const QString& selectionHover)
        {
            return QString(
                "QMainWindow, QDialog { background: %1; color: %6; }"
                "QWidget { color: %6; font-size: 10pt; }"
                "QMenuBar { background: %2; color: %6; border-bottom: 1px solid %5; }"
                "QMenuBar::item { padding: 6px 12px; }"
                "QMenuBar::item:selected { background: %10; }"
                "QMenu { background: %2; color: %6; border: 1px solid %5; }"
                "QMenu::item:selected { background: %11; color: #ffffff; }"
                "QMenu#RobotQtViewerCameraViewPalette { padding: 8px; }"
                "QToolBar { background: %2; border: 0; border-bottom: 1px solid %5; spacing: 6px; padding: 6px; }"
                "QToolButton, QPushButton { background: %9; color: %6; border: 1px solid %5; border-radius: 4px; padding: 6px 10px; }"
                "QToolButton:hover, QPushButton:hover { background: %10; border-color: %8; }"
                "QToolButton:pressed, QPushButton:pressed { background: %12; border-color: %8; }"
                "QToolButton:focus, QPushButton:focus { border-color: %8; }"
                "QToolButton:disabled, QPushButton:disabled { background: %3; color: %7; border-color: %5; }"
                "QToolButton:checked, QPushButton:checked { background: %11; border-color: %8; color: #ffffff; font-weight: 600; }"
                "QToolButton[cameraViewControl=\"true\"] { border: 1px solid %5; border-radius: 4px; padding: 4px; }"
                "QToolButton[cameraViewControl=\"true\"]:hover { background: %10; border-color: %8; }"
                "QToolButton[cameraViewControl=\"true\"]:pressed { background: %12; }"
                "QPushButton:default, QPushButton[uiActionRole=\"primary\"] { background: %11; color: #ffffff; border-color: %8; font-weight: 600; }"
                "QPushButton:default:hover, QPushButton[uiActionRole=\"primary\"]:hover { background: %12; }"
                "QPushButton:default:disabled, QPushButton[uiActionRole=\"primary\"]:disabled { background: %4; color: %7; border-color: %5; }"
                "QPushButton[uiActionRole=\"accent\"] { background: %10; border-color: %8; font-weight: 600; }"
                "QPushButton[uiActionRole=\"accent\"]:checked { background: %11; color: #ffffff; }"
                "QPushButton[uiActionRole=\"destructive\"] { background: #4a292d; color: #ffd9dc; border-color: #a94b55; font-weight: 600; }"
                "QPushButton[uiActionRole=\"destructive\"]:hover { background: #663238; border-color: #e06b75; color: #ffffff; }"
                "QPushButton[uiActionRole=\"destructive\"]:pressed { background: #7b3941; }"
                "QPushButton[uiActionRole=\"destructive\"]:disabled { background: %3; color: %7; border-color: %5; }"
                "QDockWidget { color: %6; }"
                "QDockWidget::title { background: %2; padding: 8px; border: 1px solid %5; }"
                "QFrame[inspectorSection=\"true\"] { background: %2; border: 1px solid %5; border-radius: 4px; }"
                "QFrame#RobotQtViewerCameraViewOverlay { background: %2; border: 1px solid %5; border-radius: 4px; }"
                "QFrame[sectionDivider=\"true\"] { color: %5; background: %5; max-height: 1px; }"
                "QFrame[inspectorSection=\"true\"] QLabel { background: transparent; }"
                "QLabel[panelTitle=\"true\"] { color: %6; background: %4; border-left: 3px solid %8; font-weight: 600; padding: 6px 8px; }"
                "QFrame[inspectorSection=\"true\"] QLabel[panelTitle=\"true\"] { background: transparent; border: 0; padding: 0 0 5px 0; }"
                "QLabel[inspectorValue=\"true\"] { color: %6; font-weight: 500; padding: 2px 0; }"
                "QLabel[inspectorInset=\"true\"] { background: %3; border: 1px solid %5; border-radius: 4px; padding: 8px; }"
                "QLabel[error=\"true\"] { color: #ff8c94; }"
                "QFrame[inspectorSection=\"true\"] QLabel[inspectorStatus=\"true\"] { color: %7; background: %4; border-top: 1px solid %5; padding: 6px 8px; }"
                "QTreeWidget, QListWidget, QTableWidget, QTextEdit, QPlainTextEdit { background: %3; color: %6; border: 1px solid %5; selection-background-color: %11; selection-color: #ffffff; alternate-background-color: %4; gridline-color: %5; }"
                "QTreeWidget::item, QListWidget::item { min-height: 26px; padding: 4px; }"
                "QTreeWidget::item:hover, QListWidget::item:hover, QTableWidget::item:hover { background: %12; }"
                "QTreeWidget::item:selected, QListWidget::item:selected, QTableWidget::item:selected { background: %11; color: #ffffff; }"
                "QTreeWidget::item:disabled, QListWidget::item:disabled, QTableWidget::item:disabled { color: %7; }"
                "QTabWidget::pane { border: 1px solid %5; background: %3; }"
                "QTabBar::tab { background: %2; color: %7; padding: 7px 12px; border: 1px solid %5; border-bottom: 0; }"
                "QTabBar::tab:selected { background: %3; color: %6; }"
                "QTabBar::tab:hover { color: %8; }"
                "QHeaderView::section { background: %9; color: %7; border: 0; border-right: 1px solid %5; border-bottom: 1px solid %5; padding: 6px 8px; font-weight: 600; }"
                "QScrollArea { background: %3; border: 1px solid %5; }"
                "QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: %3; color: %6; border: 1px solid %5; border-radius: 4px; padding: 5px 7px; selection-background-color: %11; }"
                "QLineEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover, QComboBox:hover { border-color: %8; }"
                "QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus, QComboBox:on { border: 2px solid %8; padding: 4px 6px; }"
                "QLineEdit:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled, QComboBox:disabled { background: %4; color: %7; }"
                "QLineEdit:read-only { background: %4; color: %7; }"
                "QComboBox::drop-down { width: 26px; border: 0; border-left: 1px solid %5; background: %9; }"
                "QComboBox::drop-down:hover { background: %10; }"
                "QComboBox::down-arrow { image: url(:/RobotQtViewer/icons/theme/arrow-light.xpm); width: 10px; height: 6px; }"
                "QCheckBox, QRadioButton, QGroupBox { color: %6; spacing: 6px; }"
                "QCheckBox:hover, QRadioButton:hover { color: %8; }"
                "QCheckBox::indicator, QTreeView::indicator { width: 17px; height: 17px; background: %3; border: 1px solid %7; border-radius: 3px; }"
                "QCheckBox::indicator:hover, QTreeView::indicator:hover { background: %10; border-color: %8; }"
                "QCheckBox::indicator:checked, QTreeView::indicator:checked { background: %11; border-color: %8; image: url(:/RobotQtViewer/icons/theme/check.xpm); }"
                "QCheckBox::indicator:indeterminate, QTreeView::indicator:indeterminate { background: %11; border-color: %8; image: url(:/RobotQtViewer/icons/theme/minus.xpm); }"
                "QCheckBox::indicator:disabled, QTreeView::indicator:disabled { background: %4; border-color: %5; }"
                "QRadioButton::indicator { width: 17px; height: 17px; background: %3; border: 1px solid %7; border-radius: 9px; }"
                "QRadioButton::indicator:hover { background: %10; border-color: %8; }"
                "QRadioButton::indicator:checked { background: %11; border-color: %8; image: url(:/RobotQtViewer/icons/theme/radio-dot.xpm); }"
                "QRadioButton::indicator:disabled { background: %4; border-color: %5; }"
                "QGroupBox { background: %2; border: 1px solid %5; border-radius: 4px; margin-top: 12px; padding: 12px 8px 8px 8px; }"
                "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 5px; color: %8; font-weight: 600; }"
                "QFrame[jointRow=\"true\"] { background: %4; border: 1px solid %5; border-radius: 3px; }"
                "QLabel[jointName=\"true\"] { color: %6; font-weight: 600; }"
                "QSlider::groove:horizontal { height: 4px; background: %5; border-radius: 2px; }"
                "QSlider::handle:horizontal { background: %8; width: 14px; margin: -5px 0; border-radius: 7px; }"
                "QToolTip { background: %2; color: %6; border: 1px solid %8; padding: 5px; }"
                "QStatusBar { background: %3; color: %7; border-top: 1px solid %5; }")
                .arg(window)
                .arg(panel)
                .arg(base)
                .arg(alternate)
                .arg(border)
                .arg(text)
                .arg(muted)
                .arg(accent)
                .arg(button)
                .arg(buttonHover)
                .arg(selection)
                .arg(selectionHover);
        }

        QString lightStyleSheet()
        {
            return QString(
                "QMainWindow, QDialog { background: #f4f6f8; color: #1d252d; }"
                "QWidget { color: #1d252d; font-size: 10pt; }"
                "QMenuBar, QToolBar { background: #ffffff; color: #1d252d; border-bottom: 1px solid #d7dde4; spacing: 6px; padding: 6px; }"
                "QMenu { background: #ffffff; color: #1d252d; border: 1px solid #c9d1da; }"
                "QMenu::item:selected { background: #2f80ed; color: #ffffff; }"
                "QMenu#RobotQtViewerCameraViewPalette { padding: 8px; }"
                "QPushButton, QToolButton { background: #ffffff; color: #1d252d; border: 1px solid #b8c2cc; border-radius: 4px; padding: 6px 10px; }"
                "QPushButton:hover, QToolButton:hover { background: #eaf2ff; border-color: #2f80ed; }"
                "QPushButton:pressed, QToolButton:pressed { background: #dceaff; }"
                "QPushButton:disabled, QToolButton:disabled { background: #edf1f5; color: #8a96a3; border-color: #d7dde4; }"
                "QToolButton:checked, QPushButton:checked { background: #236fbd; color: #ffffff; border-color: #155e9f; font-weight: 600; }"
                "QToolButton[cameraViewControl=\"true\"] { border: 1px solid #c9d1da; border-radius: 4px; padding: 4px; }"
                "QToolButton[cameraViewControl=\"true\"]:hover { background: #eaf2ff; border-color: #2f80ed; }"
                "QToolButton[cameraViewControl=\"true\"]:pressed { background: #dceaff; }"
                "QPushButton:default, QPushButton[uiActionRole=\"primary\"] { background: #236fbd; color: #ffffff; border-color: #155e9f; font-weight: 600; }"
                "QPushButton:default:hover, QPushButton[uiActionRole=\"primary\"]:hover { background: #2f80ed; }"
                "QPushButton:default:disabled, QPushButton[uiActionRole=\"primary\"]:disabled { background: #edf1f5; color: #8a96a3; border-color: #d7dde4; }"
                "QPushButton[uiActionRole=\"accent\"] { background: #eaf2ff; border-color: #2f80ed; color: #155e9f; font-weight: 600; }"
                "QPushButton[uiActionRole=\"accent\"]:checked { background: #236fbd; color: #ffffff; }"
                "QPushButton[uiActionRole=\"destructive\"] { background: #fff0f1; color: #9e2430; border-color: #d65b66; font-weight: 600; }"
                "QPushButton[uiActionRole=\"destructive\"]:hover { background: #ffe0e3; color: #7e1520; border-color: #b8323f; }"
                "QPushButton[uiActionRole=\"destructive\"]:pressed { background: #ffcdd2; }"
                "QPushButton[uiActionRole=\"destructive\"]:disabled { background: #edf1f5; color: #8a96a3; border-color: #d7dde4; }"
                "QDockWidget::title { background: #ffffff; padding: 8px; border: 1px solid #d7dde4; }"
                "QFrame[inspectorSection=\"true\"] { background: #ffffff; border: 1px solid #d7dde4; border-radius: 4px; }"
                "QFrame#RobotQtViewerCameraViewOverlay { background: #ffffff; border: 1px solid #c9d1da; border-radius: 4px; }"
                "QFrame[sectionDivider=\"true\"] { color: #c9d1da; background: #c9d1da; max-height: 1px; }"
                "QFrame[inspectorSection=\"true\"] QLabel { background: transparent; }"
                "QLabel[panelTitle=\"true\"] { color: #1d252d; background: #edf1f5; border-left: 3px solid #2f80ed; font-weight: 600; padding: 6px 8px; }"
                "QFrame[inspectorSection=\"true\"] QLabel[panelTitle=\"true\"] { background: transparent; border: 0; padding: 0 0 5px 0; }"
                "QLabel[inspectorValue=\"true\"] { color: #1d252d; font-weight: 500; padding: 2px 0; }"
                "QLabel[inspectorInset=\"true\"] { background: #f4f6f8; border: 1px solid #c9d1da; border-radius: 4px; padding: 8px; }"
                "QLabel[error=\"true\"] { color: #b4232f; }"
                "QFrame[inspectorSection=\"true\"] QLabel[inspectorStatus=\"true\"] { color: #586575; background: #f1f4f7; border-top: 1px solid #d7dde4; padding: 6px 8px; }"
                "QTreeWidget, QListWidget, QTableWidget, QTextEdit, QPlainTextEdit { background: #ffffff; color: #1d252d; border: 1px solid #c9d1da; selection-background-color: #236fbd; selection-color: #ffffff; alternate-background-color: #f1f4f7; gridline-color: #d7dde4; }"
                "QTreeWidget::item, QListWidget::item { min-height: 26px; padding: 4px; }"
                "QTreeWidget::item:hover, QListWidget::item:hover, QTableWidget::item:hover { background: #eaf2ff; }"
                "QTreeWidget::item:selected, QListWidget::item:selected, QTableWidget::item:selected { background: #236fbd; color: #ffffff; }"
                "QTreeWidget::item:disabled, QListWidget::item:disabled, QTableWidget::item:disabled { color: #8a96a3; }"
                "QTabWidget::pane { border: 1px solid #c9d1da; background: #ffffff; }"
                "QTabBar::tab { background: #e8edf2; color: #46515d; padding: 7px 12px; border: 1px solid #c9d1da; border-bottom: 0; }"
                "QTabBar::tab:selected { background: #ffffff; color: #1d252d; }"
                "QHeaderView::section { background: #edf1f5; color: #46515d; border: 0; border-right: 1px solid #d7dde4; border-bottom: 1px solid #d7dde4; padding: 6px 8px; font-weight: 600; }"
                "QScrollArea { background: #ffffff; border: 1px solid #c9d1da; }"
                "QLineEdit, QDoubleSpinBox, QSpinBox, QComboBox { background: #ffffff; color: #1d252d; border: 1px solid #b8c2cc; border-radius: 4px; padding: 5px 7px; selection-background-color: #236fbd; }"
                "QLineEdit:hover, QDoubleSpinBox:hover, QSpinBox:hover, QComboBox:hover { border-color: #2f80ed; }"
                "QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus, QComboBox:focus, QComboBox:on { border: 2px solid #2f80ed; padding: 4px 6px; }"
                "QLineEdit:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled, QComboBox:disabled { background: #f1f4f7; color: #8a96a3; }"
                "QLineEdit:read-only { background: #f1f4f7; color: #586575; }"
                "QComboBox::drop-down { width: 26px; border: 0; border-left: 1px solid #c9d1da; background: #edf1f5; }"
                "QComboBox::drop-down:hover { background: #eaf2ff; }"
                "QComboBox::down-arrow { image: url(:/RobotQtViewer/icons/theme/arrow-dark.xpm); width: 10px; height: 6px; }"
                "QCheckBox, QRadioButton { spacing: 6px; }"
                "QCheckBox:hover, QRadioButton:hover { color: #155e9f; }"
                "QCheckBox::indicator, QTreeView::indicator { width: 17px; height: 17px; background: #ffffff; border: 1px solid #7d8996; border-radius: 3px; }"
                "QCheckBox::indicator:hover, QTreeView::indicator:hover { background: #eaf2ff; border-color: #2f80ed; }"
                "QCheckBox::indicator:checked, QTreeView::indicator:checked { background: #236fbd; border-color: #155e9f; image: url(:/RobotQtViewer/icons/theme/check.xpm); }"
                "QCheckBox::indicator:indeterminate, QTreeView::indicator:indeterminate { background: #236fbd; border-color: #155e9f; image: url(:/RobotQtViewer/icons/theme/minus.xpm); }"
                "QCheckBox::indicator:disabled, QTreeView::indicator:disabled { background: #edf1f5; border-color: #c9d1da; }"
                "QRadioButton::indicator { width: 17px; height: 17px; background: #ffffff; border: 1px solid #7d8996; border-radius: 9px; }"
                "QRadioButton::indicator:hover { background: #eaf2ff; border-color: #2f80ed; }"
                "QRadioButton::indicator:checked { background: #236fbd; border-color: #155e9f; image: url(:/RobotQtViewer/icons/theme/radio-dot.xpm); }"
                "QRadioButton::indicator:disabled { background: #edf1f5; border-color: #c9d1da; }"
                "QGroupBox { background: #ffffff; border: 1px solid #c9d1da; border-radius: 4px; margin-top: 12px; padding: 12px 8px 8px 8px; }"
                "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 5px; color: #155e9f; font-weight: 600; }"
                "QFrame[jointRow=\"true\"] { background: #ffffff; border: 1px solid #d7dde4; border-radius: 3px; }"
                "QSlider::groove:horizontal { height: 4px; background: #c9d1da; border-radius: 2px; }"
                "QSlider::handle:horizontal { background: #2f80ed; width: 14px; margin: -5px 0; border-radius: 7px; }"
                "QToolTip { background: #ffffff; color: #1d252d; border: 1px solid #2f80ed; padding: 5px; }"
                "QStatusBar { background: #ffffff; color: #586575; border-top: 1px solid #d7dde4; }");
        }
    }

    ThemeKind ThemeManager::savedTheme()
    {
        QSettings settings;
        return themeFromName(settings.value(kSettingsKey, themeName(ThemeKind::Modern)).toString());
    }

    ThemeKind ThemeManager::themeFromName(const QString& name)
    {
        const QString normalized = name.trimmed().toLower();
        if (normalized == "light")
            return ThemeKind::Light;
        if (normalized == "dark")
            return ThemeKind::Dark;
        return ThemeKind::Modern;
    }

    QString ThemeManager::themeName(ThemeKind theme)
    {
        switch (theme)
        {
        case ThemeKind::Light:
            return "Light";
        case ThemeKind::Dark:
            return "Dark";
        case ThemeKind::Modern:
        default:
            return "Modern";
        }
    }

    void ThemeManager::apply(QApplication& app, ThemeKind theme)
    {
        app.setStyle(QStyleFactory::create("Fusion"));
        app.setFont(applicationUiFont());

        switch (theme)
        {
        case ThemeKind::Light:
            app.setPalette(makePalette(
                QColor("#f4f6f8"),
                QColor("#ffffff"),
                QColor("#1d252d"),
                QColor("#8a96a3"),
                QColor("#ffffff"),
                QColor("#f1f4f7"),
                QColor("#ffffff"),
                QColor("#2f80ed"),
                QColor("#ffffff")));
            app.setStyleSheet(lightStyleSheet());
            break;
        case ThemeKind::Dark:
            app.setPalette(makePalette(
                QColor("#111418"),
                QColor("#191f26"),
                QColor("#e3e8ef"),
                QColor("#6f7a86"),
                QColor("#0b0f14"),
                QColor("#131920"),
                QColor("#232b34"),
                QColor("#4e9af1"),
                QColor("#ffffff")));
            app.setStyleSheet(darkStyleSheet(
                "#111418", "#191f26", "#0b0f14", "#131920", "#2b3440",
                "#e3e8ef", "#9ca8b5", "#4e9af1", "#232b34", "#2f3a46",
                "#245b87", "#294964"));
            break;
        case ThemeKind::Modern:
        default:
            app.setPalette(makePalette(
                QColor("#171b20"),
                QColor("#20262d"),
                QColor("#d8dee6"),
                QColor("#727e8a"),
                QColor("#11151a"),
                QColor("#161c22"),
                QColor("#29323b"),
                QColor("#2f80ed"),
                QColor("#ffffff")));
            app.setStyleSheet(darkStyleSheet(
                "#171b20", "#20262d", "#11151a", "#161c22", "#2e3944",
                "#d8dee6", "#aeb8c4", "#7cc7ff", "#29323b", "#34404b",
                "#285f88", "#2b4d67"));
            break;
        }
    }

    void ThemeManager::applySaved(QApplication& app)
    {
        apply(app, savedTheme());
    }

    void ThemeManager::applyNativeWindowFrame(QWidget& window, ThemeKind theme)
    {
#ifdef Q_OS_WIN
        HWND handle = reinterpret_cast<HWND>(window.winId());
        if(handle == nullptr) {
            return;
        }

        const BOOL useDarkFrame = theme != ThemeKind::Light ? TRUE : FALSE;
        if(FAILED(DwmSetWindowAttribute(
            handle,
            kDwmUseImmersiveDarkMode,
            &useDarkFrame,
            sizeof(useDarkFrame)))) {
            DwmSetWindowAttribute(
                handle,
                kDwmUseImmersiveDarkModeBefore20H1,
                &useDarkFrame,
                sizeof(useDarkFrame));
        }

        switch(theme) {
        case ThemeKind::Light:
            setDwmColor(handle, kDwmCaptionColor, QColor("#f4f6f8"));
            setDwmColor(handle, kDwmTextColor, QColor("#1d252d"));
            setDwmColor(handle, kDwmWindowBorderColor, QColor("#c9d1da"));
            break;
        case ThemeKind::Dark:
            setDwmColor(handle, kDwmCaptionColor, QColor("#191f26"));
            setDwmColor(handle, kDwmTextColor, QColor("#f2f6fb"));
            setDwmColor(handle, kDwmWindowBorderColor, QColor("#2b3440"));
            break;
        case ThemeKind::Modern:
        default:
            setDwmColor(handle, kDwmCaptionColor, QColor("#20262d"));
            setDwmColor(handle, kDwmTextColor, QColor("#f2f6fb"));
            setDwmColor(handle, kDwmWindowBorderColor, QColor("#2e3944"));
            break;
        }
#else
        Q_UNUSED(window);
        Q_UNUSED(theme);
#endif
    }

    void ThemeManager::save(ThemeKind theme)
    {
        QSettings settings;
        settings.setValue(kSettingsKey, themeName(theme));
    }
}
