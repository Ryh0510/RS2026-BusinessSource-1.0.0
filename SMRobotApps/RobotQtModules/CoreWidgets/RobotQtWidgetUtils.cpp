#include "RobotQtWidgetUtils.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QGridLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>

namespace robot_qt_viewer
{
    namespace
    {
        const char* actionRoleName(UiActionRole role)
        {
            switch(role) {
            case UiActionRole::Primary:
                return "primary";
            case UiActionRole::Accent:
                return "accent";
            case UiActionRole::Destructive:
                return "destructive";
            case UiActionRole::Standard:
            default:
                return "standard";
            }
        }

        class InspectorComboController : public QObject
        {
        public:
            InspectorComboController(QComboBox* combo, bool entitySelector)
                : QObject(combo)
                , m_combo(combo)
                , m_entitySelector(entitySelector)
            {
                if(m_combo == nullptr) {
                    return;
                }
                m_combo->installEventFilter(this);
                if(m_combo->view() != nullptr) {
                    m_combo->view()->installEventFilter(this);
                    m_combo->view()->window()->installEventFilter(this);
                }
                connect(m_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [this](int index) {
                        if(m_combo == nullptr) {
                            return;
                        }
                        if(index >= 0) {
                            m_lastValidIndex = index;
                        }
                        updateToolTip();
                    });
                if(m_entitySelector && m_combo->lineEdit() != nullptr) {
                    connect(m_combo->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
                        restoreValidSelection();
                    });
                }
                updateToolTip();
            }

        protected:
            bool eventFilter(QObject* watched, QEvent* event) override
            {
                if(m_combo == nullptr) {
                    return QObject::eventFilter(watched, event);
                }
                if(event->type() == QEvent::Show || event->type() == QEvent::Resize) {
                    configurePopupGeometry();
                }
                return QObject::eventFilter(watched, event);
            }

        private:
            void restoreValidSelection()
            {
                if(m_combo == nullptr || !m_entitySelector) {
                    return;
                }
                const int exactIndex = m_combo->findText(m_combo->currentText(), Qt::MatchFixedString);
                if(exactIndex >= 0) {
                    m_combo->setCurrentIndex(exactIndex);
                } else if(m_lastValidIndex >= 0 && m_lastValidIndex < m_combo->count()) {
                    m_combo->setCurrentIndex(m_lastValidIndex);
                }
            }

            void updateToolTip()
            {
                if(m_combo == nullptr) {
                    return;
                }
                const int index = m_combo->currentIndex();
                if(index < 0) {
                    return;
                }
                const QString itemToolTip = m_combo->itemData(index, Qt::ToolTipRole).toString();
                m_combo->setToolTip(itemToolTip.isEmpty() ? m_combo->itemText(index) : itemToolTip);
            }

            int popupWidth() const
            {
                if(m_combo == nullptr) {
                    return 0;
                }
                const QFontMetrics metrics(m_combo->font());
                int contentWidth = m_combo->width();
                for(int index = 0; index < m_combo->count(); ++index) {
                    contentWidth = qMax(contentWidth, metrics.horizontalAdvance(m_combo->itemText(index)) + 52);
                }

                const QPoint center = m_combo->mapToGlobal(m_combo->rect().center());
                QScreen* screen = QGuiApplication::screenAt(center);
                if(screen == nullptr) {
                    screen = QGuiApplication::primaryScreen();
                }
                const int screenLimit = screen != nullptr
                    ? qMax(m_combo->width(), static_cast<int>(screen->availableGeometry().width() * 0.72))
                    : contentWidth;
                return qBound(m_combo->width(), contentWidth, screenLimit);
            }

            void configurePopupGeometry()
            {
                if(m_combo == nullptr || m_combo->view() == nullptr) {
                    return;
                }
                const int width = popupWidth();
                m_combo->view()->setMinimumWidth(width);
                m_combo->view()->window()->setMinimumWidth(width);
                if(m_combo->completer() != nullptr && m_combo->completer()->popup() != nullptr) {
                    m_combo->completer()->popup()->setMinimumWidth(width);
                }
            }

            QPointer<QComboBox> m_combo;
            bool m_entitySelector = false;
            int m_lastValidIndex = -1;
        };

        void configureComboBase(QComboBox* combo, int minimumContentsLength, bool entitySelector)
        {
            if(combo == nullptr) {
                return;
            }
            makeHorizontallyCompressible(combo);
            combo->setMinimumHeight(30);
            combo->setMinimumContentsLength(minimumContentsLength);
            combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            combo->setMaxVisibleItems(entitySelector ? 12 : 16);
            combo->setProperty("uiControlRole", entitySelector ? "entitySelector" : "enumSelector");
            if(combo->view() != nullptr) {
                combo->view()->setTextElideMode(Qt::ElideRight);
                combo->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
            }
            if(entitySelector) {
                combo->setEditable(true);
                combo->setInsertPolicy(QComboBox::NoInsert);
                if(combo->completer() != nullptr) {
                    combo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
                    combo->completer()->setFilterMode(Qt::MatchContains);
                    combo->completer()->setCompletionMode(QCompleter::PopupCompletion);
                    combo->completer()->setMaxVisibleItems(12);
                }
            }
            new InspectorComboController(combo, entitySelector);
        }
    }

    QLabel* makePanelTitle(const QString& text, QWidget* parent)
    {
        auto* label = new QLabel(text, parent);
        label->setProperty("panelTitle", true);
        return label;
    }

    void makeHorizontallyCompressible(QWidget* widget)
    {
        if(widget == nullptr) {
            return;
        }
        widget->setMinimumWidth(0);
        widget->setMaximumWidth(QWIDGETSIZE_MAX);
        QSizePolicy policy = widget->sizePolicy();
        policy.setHorizontalPolicy(QSizePolicy::Ignored);
        policy.setHorizontalStretch(0);
        widget->setSizePolicy(policy);
    }

    void configureInspectorButton(QPushButton* button)
    {
        if(button == nullptr) {
            return;
        }
        makeHorizontallyCompressible(button);
        button->setMinimumSize(0, 0);
        if(button->toolTip().isEmpty()) {
            button->setToolTip(button->text());
        }
    }

    void configureActionButton(QAbstractButton* button, UiActionRole role)
    {
        if(button == nullptr) {
            return;
        }
        makeHorizontallyCompressible(button);
        button->setMinimumSize(0, 30);
        button->setProperty("uiActionRole", actionRoleName(role));
        if(button->toolTip().isEmpty()) {
            button->setToolTip(button->text());
        }
    }

    void configureInspectorToggle(QAbstractButton* button)
    {
        if(button == nullptr) {
            return;
        }
        makeHorizontallyCompressible(button);
        button->setMinimumHeight(26);
        button->setProperty("uiControlRole", "toggle");
    }

    void configureDialogButtonBox(QDialogButtonBox* buttonBox)
    {
        if(buttonBox == nullptr) {
            return;
        }
        const QList<QAbstractButton*> buttons = buttonBox->buttons();
        for(QAbstractButton* button : buttons) {
            UiActionRole role = UiActionRole::Standard;
            switch(buttonBox->buttonRole(button)) {
            case QDialogButtonBox::AcceptRole:
            case QDialogButtonBox::YesRole:
            case QDialogButtonBox::ApplyRole:
                role = UiActionRole::Primary;
                break;
            case QDialogButtonBox::DestructiveRole:
                role = UiActionRole::Destructive;
                break;
            default:
                break;
            }
            configureActionButton(button, role);
        }
    }

    void configureInspectorGrid(QGridLayout* layout)
    {
        if(layout == nullptr) {
            return;
        }
        layout->setSizeConstraint(QLayout::SetNoConstraint);
        for(int column = 0; column < layout->columnCount(); ++column) {
            layout->setColumnMinimumWidth(column, 0);
            layout->setColumnStretch(column, 1);
        }
    }

    void configureInspectorForm(QFormLayout* form)
    {
        if(form == nullptr) {
            return;
        }
        form->setSizeConstraint(QLayout::SetNoConstraint);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    }

    void configureInspectorList(QListWidget* list, bool alternatingRows, bool uniformItems)
    {
        if(list == nullptr) {
            return;
        }
        makeHorizontallyCompressible(list);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setTextElideMode(Qt::ElideRight);
        list->setUniformItemSizes(uniformItems);
        list->setAlternatingRowColors(alternatingRows);
    }

    void configureInspectorCombo(QComboBox* combo, int minimumContentsLength)
    {
        configureComboBase(combo, minimumContentsLength, false);
    }

    void configureInspectorEntityCombo(QComboBox* combo, int minimumContentsLength)
    {
        configureComboBase(combo, minimumContentsLength, true);
    }

    QListWidgetItem* addInspectorListItem(QListWidget* list, const QString& text)
    {
        auto* item = new QListWidgetItem(text, list);
        item->setToolTip(text);
        return item;
    }
}
