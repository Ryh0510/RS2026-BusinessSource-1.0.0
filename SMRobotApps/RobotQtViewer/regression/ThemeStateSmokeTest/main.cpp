#include "RobotQtViewerTheme.h"
#include "RobotQtWidgetUtils.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QImage>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdint>
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

    std::uint64_t renderDigest(QWidget& widget)
    {
        widget.ensurePolished();
        const QSize size = widget.size().expandedTo(widget.sizeHint());
        widget.resize(size);
        QImage image(size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        widget.render(&image);

        std::uint64_t digest = 1469598103934665603ULL;
        for(int y = 0; y < image.height(); ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for(int x = 0; x < image.width(); ++x) {
                digest ^= static_cast<std::uint64_t>(row[x]);
                digest *= 1099511628211ULL;
            }
        }
        return digest;
    }

    void requireThemeResources()
    {
        const char* resources[] = {
            ":/RobotQtViewer/icons/theme/arrow-dark.xpm",
            ":/RobotQtViewer/icons/theme/arrow-light.xpm",
            ":/RobotQtViewer/icons/theme/check.xpm",
            ":/RobotQtViewer/icons/theme/minus.xpm",
            ":/RobotQtViewer/icons/theme/radio-dot.xpm"
        };
        for(const char* resource : resources) {
            require(QFile::exists(QString::fromLatin1(resource)), "theme resource is missing");
        }
    }

    void verifyTheme(
        QApplication& application,
        robot_qt_viewer::ThemeKind theme)
    {
        using namespace robot_qt_viewer;

        ThemeManager::apply(application, theme);
        const QString styleSheet = application.styleSheet();
        require(styleSheet.contains(QStringLiteral("uiActionRole=\"primary\"")),
            "primary action selector is missing");
        require(styleSheet.contains(QStringLiteral("uiActionRole=\"destructive\"")),
            "destructive action selector is missing");
        require(styleSheet.contains(QStringLiteral("QCheckBox::indicator:checked")),
            "checked indicator selector is missing");
        require(styleSheet.contains(QStringLiteral("QComboBox::down-arrow")),
            "combo arrow selector is missing");

        QWidget root;
        root.resize(420, 420);
        auto* layout = new QVBoxLayout(&root);

        QPushButton standard(QStringLiteral("Action"), &root);
        QPushButton primary(QStringLiteral("Action"), &root);
        QPushButton accent(QStringLiteral("Action"), &root);
        QPushButton destructive(QStringLiteral("Action"), &root);
        configureActionButton(&standard, UiActionRole::Standard);
        configureActionButton(&primary, UiActionRole::Primary);
        configureActionButton(&accent, UiActionRole::Accent);
        configureActionButton(&destructive, UiActionRole::Destructive);
        layout->addWidget(&standard);
        layout->addWidget(&primary);
        layout->addWidget(&accent);
        layout->addWidget(&destructive);

        QComboBox combo(&root);
        combo.addItems({ QStringLiteral("First"), QStringLiteral("Second") });
        configureInspectorCombo(&combo);
        layout->addWidget(&combo);

        QCheckBox check(QStringLiteral("Enabled"), &root);
        configureInspectorToggle(&check);
        layout->addWidget(&check);
        QRadioButton radio(QStringLiteral("Selected"), &root);
        configureInspectorToggle(&radio);
        radio.setChecked(true);
        layout->addWidget(&radio);

        QDialogButtonBox buttons(
            QDialogButtonBox::Ok |
            QDialogButtonBox::Cancel |
            QDialogButtonBox::Discard,
            &root);
        configureDialogButtonBox(&buttons);
        layout->addWidget(&buttons);

        root.show();
        application.processEvents();

        require(primary.property("uiActionRole").toString() == QStringLiteral("primary"),
            "primary action role was not applied");
        require(accent.property("uiActionRole").toString() == QStringLiteral("accent"),
            "accent action role was not applied");
        require(destructive.property("uiActionRole").toString() == QStringLiteral("destructive"),
            "destructive action role was not applied");
        require(check.property("uiControlRole").toString() == QStringLiteral("toggle"),
            "toggle role was not applied");
        require(buttons.button(QDialogButtonBox::Ok)->property("uiActionRole").toString() ==
                QStringLiteral("primary"),
            "dialog accept role was not promoted");
        require(buttons.button(QDialogButtonBox::Cancel)->property("uiActionRole").toString() ==
                QStringLiteral("standard"),
            "dialog reject role is not standard");
        require(buttons.button(QDialogButtonBox::Discard)->property("uiActionRole").toString() ==
                QStringLiteral("destructive"),
            "dialog destructive role was not applied");

        require(renderDigest(standard) != renderDigest(primary),
            "primary action is visually indistinguishable from a standard action");
        require(renderDigest(standard) != renderDigest(destructive),
            "destructive action is visually indistinguishable from a standard action");
        const std::uint64_t uncheckedDigest = renderDigest(check);
        check.setChecked(true);
        application.processEvents();
        require(uncheckedDigest != renderDigest(check),
            "checked and unchecked controls are visually indistinguishable");

        standard.setEnabled(false);
        application.processEvents();
        require(renderDigest(primary) != renderDigest(standard),
            "disabled and primary actions are visually indistinguishable");
    }
}

int main(int argc, char** argv)
{
    try {
        QApplication application(argc, argv);
        requireThemeResources();
        verifyTheme(application, robot_qt_viewer::ThemeKind::Modern);
        verifyTheme(application, robot_qt_viewer::ThemeKind::Dark);
        verifyTheme(application, robot_qt_viewer::ThemeKind::Light);
        std::cout << "RobotQtViewer theme state smoke passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << "RobotQtViewer theme state smoke failed: " << error.what() << '\n';
        return 1;
    }
}
