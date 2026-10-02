#include "CameraPreviewWidget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QFontMetrics>

#include <algorithm>

namespace
{
    const QSize kDefaultPreviewSize(320, 250);
}

CameraPreviewWidget::CameraPreviewWidget(
    const QString& cameraId,
    const QString& name,
    QWidget* parent)
    : QFrame(parent)
    , m_cameraId(cameraId)
    , m_cameraName(name.isEmpty() ? cameraId : name)
{
    setObjectName(QStringLiteral("cameraPreview"));
    setFrameShape(QFrame::StyledPanel);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(240, 180);
    resize(kDefaultPreviewSize);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);

    auto* titleBar = new QFrame(this);
    titleBar->setObjectName(QStringLiteral("cameraPreviewTitleBar"));
    titleBar->setFixedHeight(36);
    titleBar->installEventFilter(this);
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 2, 4, 2);
    titleLayout->setSpacing(2);

    m_dragHandleLabel = new QLabel(QStringLiteral("::"), titleBar);
    m_dragHandleLabel->setToolTip(QStringLiteral("Drag camera preview"));
    m_dragHandleLabel->setCursor(Qt::SizeAllCursor);
    m_dragHandleLabel->installEventFilter(this);
    titleLayout->addWidget(m_dragHandleLabel);

    m_titleLabel = new QLabel(m_cameraName, titleBar);
    m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_titleLabel->setCursor(Qt::SizeAllCursor);
    m_titleLabel->installEventFilter(this);
    titleLayout->addWidget(m_titleLabel, 1);

    m_statusLabel = new QLabel(QStringLiteral("PAUSE"), titleBar);
    m_statusLabel->setObjectName(QStringLiteral("cameraPreviewStatus"));
    m_statusLabel->setToolTip(QStringLiteral("Camera stream is paused"));
    m_statusLabel->setMinimumWidth(38);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(m_statusLabel);

    m_runButton = new QToolButton(titleBar);
    m_runButton->setObjectName(QStringLiteral("cameraPreviewRunButton"));
    m_runButton->setAutoRaise(true);
    m_runButton->setFixedSize(28, 28);
    m_runButton->setCheckable(true);
    m_runButton->setChecked(true);
    m_runButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_runButton->setToolTip(QStringLiteral("Pause camera stream"));
    titleLayout->addWidget(m_runButton);

    m_lockButton = new QToolButton(titleBar);
    m_lockButton->setObjectName(QStringLiteral("cameraPreviewLockButton"));
    m_lockButton->setCheckable(true);
    m_lockButton->setAutoRaise(true);
    m_lockButton->setFixedSize(28, 28);
    m_lockButton->setText(QStringLiteral("L"));
    m_lockButton->setToolTip(QStringLiteral("Lock preview position and size"));
    titleLayout->addWidget(m_lockButton);

    m_closeButton = new QToolButton(titleBar);
    m_closeButton->setObjectName(QStringLiteral("cameraPreviewCloseButton"));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setFixedSize(28, 28);
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    m_closeButton->setToolTip(QStringLiteral("Hide camera preview"));
    titleLayout->addWidget(m_closeButton);
    root->addWidget(titleBar);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(160, 100);
    m_imageLabel->setText(QStringLiteral("Waiting for camera frame"));
    m_imageLabel->setToolTip(QStringLiteral("Hold Alt and drag to move the preview"));
    m_imageLabel->installEventFilter(this);
    root->addWidget(m_imageLabel, 1);

    m_messageOverlay = new QLabel(m_imageLabel);
    m_messageOverlay->setObjectName(QStringLiteral("cameraPreviewMessageOverlay"));
    m_messageOverlay->setAlignment(Qt::AlignCenter);
    m_messageOverlay->setWordWrap(true);
    m_messageOverlay->setMargin(12);
    m_messageOverlay->setStyleSheet(
        QStringLiteral("background-color: rgba(20, 24, 28, 190); color: white;"));
    m_messageOverlay->hide();

    m_resizeHandle = new QLabel(QStringLiteral("//"), this);
    m_resizeHandle->setObjectName(QStringLiteral("cameraPreviewResizeHandle"));
    m_resizeHandle->setAlignment(Qt::AlignCenter);
    m_resizeHandle->setFixedSize(24, 24);
    m_resizeHandle->setCursor(Qt::SizeFDiagCursor);
    m_resizeHandle->setToolTip(QStringLiteral("Resize camera preview"));
    m_resizeHandle->installEventFilter(this);
    m_resizeHandle->raise();

    connect(m_runButton, &QToolButton::toggled, this, [this](bool checked) {
        m_runButton->setIcon(style()->standardIcon(
            checked ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
        m_runButton->setToolTip(checked
            ? QStringLiteral("Pause camera stream")
            : QStringLiteral("Run camera stream"));
        setStatus(checked ? CameraPreviewStatus::Running : CameraPreviewStatus::Paused);
        emit streamRunningChanged(checked);
    });
    connect(m_lockButton, &QToolButton::toggled, this, &CameraPreviewWidget::setPositionLocked);
    connect(m_closeButton, &QToolButton::clicked, this, [this]() {
        hide();
        emit previewVisibilityChanged(false);
    });
    setPositionLocked(false);
    setStatus(CameraPreviewStatus::Running);
}

QString CameraPreviewWidget::cameraId() const
{
    return m_cameraId;
}

bool CameraPreviewWidget::streamRunning() const
{
    return m_runButton->isChecked();
}

bool CameraPreviewWidget::positionLocked() const
{
    return m_locked;
}

void CameraPreviewWidget::setStreamRunning(bool running)
{
    if(m_runButton->isChecked() == running) {
        return;
    }
    const QSignalBlocker blocker(m_runButton);
    m_runButton->setChecked(running);
    m_runButton->setIcon(style()->standardIcon(
        running ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
    m_runButton->setToolTip(running
        ? QStringLiteral("Pause camera stream")
        : QStringLiteral("Run camera stream"));
    setStatus(running ? CameraPreviewStatus::Running : CameraPreviewStatus::Paused);
}

void CameraPreviewWidget::setPositionLocked(bool locked)
{
    m_locked = locked;
    if(m_lockButton->isChecked() != locked) {
        m_lockButton->setChecked(locked);
    }
    m_lockButton->setToolTip(locked
        ? QStringLiteral("Unlock preview position and size")
        : QStringLiteral("Lock preview position and size"));
    m_lockButton->setText(locked ? QStringLiteral("U") : QStringLiteral("L"));
    m_resizeHandle->setVisible(!locked);
    m_dragHandleLabel->setCursor(locked ? Qt::ArrowCursor : Qt::SizeAllCursor);
    m_titleLabel->setCursor(locked ? Qt::ArrowCursor : Qt::SizeAllCursor);
    m_dragging = false;
    m_resizing = false;
}

void CameraPreviewWidget::setFrameImage(const QImage& image)
{
    m_frameImage = image;
    updateFramePixmap();
    setStatus(CameraPreviewStatus::Running);
}

void CameraPreviewWidget::setStatus(CameraPreviewStatus status, const QString& detail)
{
    m_status = status;
    QString text;
    switch(status) {
    case CameraPreviewStatus::Running:
        text = QStringLiteral("RUN");
        break;
    case CameraPreviewStatus::Paused:
        text = QStringLiteral("PAUSE");
        break;
    case CameraPreviewStatus::Suspended:
        text = QStringLiteral("SUSP");
        break;
    case CameraPreviewStatus::Stale:
        text = QStringLiteral("STALE");
        break;
    case CameraPreviewStatus::Error:
        text = QStringLiteral("ERROR");
        break;
    case CameraPreviewStatus::Disabled:
        text = QStringLiteral("OFF");
        break;
    }
    if(m_statusLabel != nullptr) {
        m_statusLabel->setText(text);
        m_statusLabel->setToolTip(detail.isEmpty() ? text : detail);
    }
    if(m_messageOverlay != nullptr) {
        const bool showMessage = status == CameraPreviewStatus::Error ||
            status == CameraPreviewStatus::Stale ||
            status == CameraPreviewStatus::Disabled;
        m_messageOverlay->setText(detail.isEmpty() ? text : detail);
        m_messageOverlay->setVisible(showMessage);
        if(showMessage) {
            m_messageOverlay->raise();
        }
    }
    if(!m_frameImage.isNull()) {
        m_imageLabel->setText(QString());
    }
}

void CameraPreviewWidget::resetPreviewSize()
{
    QSize target = kDefaultPreviewSize.expandedTo(minimumSize());
    if(parentWidget() != nullptr) {
        target.setWidth(std::min(target.width(), parentWidget()->width()));
        target.setHeight(std::min(target.height(), parentWidget()->height()));
    }
    resize(target);
    clampToParent();
}

QSize CameraPreviewWidget::requestedRenderSize(int sourceWidth, int sourceHeight) const
{
    const int safeWidth = std::max(1, sourceWidth);
    const int safeHeight = std::max(1, sourceHeight);
    const double aspect = static_cast<double>(safeWidth) / static_cast<double>(safeHeight);
    int height = std::clamp(m_imageLabel->height(), 120, 480);
    int width = static_cast<int>(height * aspect + 0.5);
    if(width > 640) {
        width = 640;
        height = static_cast<int>(width / aspect + 0.5);
    }
    return QSize(std::max(160, width), std::max(120, height));
}

void CameraPreviewWidget::clampToParent()
{
    if(parentWidget() == nullptr) {
        return;
    }
    const int maxX = std::max(0, parentWidget()->width() - width());
    const int maxY = std::max(0, parentWidget()->height() - height());
    move(std::clamp(x(), 0, maxX), std::clamp(y(), 0, maxY));
}

bool CameraPreviewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == m_resizeHandle && !m_locked) {
        if(event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if(mouse->button() == Qt::LeftButton) {
                m_resizing = true;
                m_resizeStartGlobal = mouse->globalPos();
                m_resizeStartSize = size();
                raise();
                return true;
            }
        } else if(event->type() == QEvent::MouseMove && m_resizing) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            resize(constrainedResizeSize(mouse->globalPos()));
            return true;
        } else if(event->type() == QEvent::MouseButtonRelease && m_resizing) {
            m_resizing = false;
            return true;
        }
    }

    const bool titleDragTarget = watched == m_titleLabel || watched == m_dragHandleLabel ||
        watched->objectName() == QStringLiteral("cameraPreviewTitleBar");
    const bool imageDragTarget = watched == m_imageLabel;
    if((titleDragTarget || imageDragTarget) && !m_locked) {
        if(event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if(mouse->button() == Qt::LeftButton &&
                (titleDragTarget || (mouse->modifiers() & Qt::AltModifier))) {
                m_dragging = true;
                m_dragOffset = mapFromGlobal(mouse->globalPos());
                raise();
                setFocus(Qt::MouseFocusReason);
                return true;
            }
        } else if(event->type() == QEvent::MouseMove && m_dragging) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            move(parentWidget()->mapFromGlobal(mouse->globalPos()) - m_dragOffset);
            clampToParent();
            return true;
        } else if(event->type() == QEvent::MouseButtonRelease) {
            m_dragging = false;
            return true;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void CameraPreviewWidget::keyPressEvent(QKeyEvent* event)
{
    if(!m_locked) {
        const int step = event->modifiers() & Qt::ShiftModifier ? 10 : 1;
        QPoint delta;
        switch(event->key()) {
        case Qt::Key_Left:
            delta.setX(-step);
            break;
        case Qt::Key_Right:
            delta.setX(step);
            break;
        case Qt::Key_Up:
            delta.setY(-step);
            break;
        case Qt::Key_Down:
            delta.setY(step);
            break;
        default:
            QFrame::keyPressEvent(event);
            return;
        }
        move(pos() + delta);
        clampToParent();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void CameraPreviewWidget::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    m_resizeHandle->move(
        width() - m_resizeHandle->width(),
        height() - m_resizeHandle->height());
    if(m_messageOverlay != nullptr) {
        const int overlayWidth = std::max(80, m_imageLabel->width() - 32);
        const int overlayHeight = std::min(96, std::max(48, m_imageLabel->height() / 3));
        m_messageOverlay->setGeometry(
            (m_imageLabel->width() - overlayWidth) / 2,
            (m_imageLabel->height() - overlayHeight) / 2,
            overlayWidth,
            overlayHeight);
    }
    clampToParent();
    updateTitleText();
    updateFramePixmap();
}

QSize CameraPreviewWidget::constrainedResizeSize(const QPoint& globalPosition) const
{
    const QPoint delta = globalPosition - m_resizeStartGlobal;
    const int requestedWidth = std::max(minimumWidth(), m_resizeStartSize.width() + delta.x());
    const int titleHeight = 36;
    const double aspect = !m_frameImage.isNull()
        ? static_cast<double>(m_frameImage.width()) / std::max(1, m_frameImage.height())
        : 4.0 / 3.0;
    int requestedHeight = titleHeight + static_cast<int>(
        static_cast<double>(requestedWidth) / aspect + 0.5);
    if(std::abs(delta.y()) > std::abs(delta.x())) {
        requestedHeight = std::max(minimumHeight(), m_resizeStartSize.height() + delta.y());
        const int imageHeight = std::max(1, requestedHeight - titleHeight);
        const int aspectWidth = static_cast<int>(imageHeight * aspect + 0.5);
        const int width = std::max(minimumWidth(), aspectWidth);
        requestedHeight = titleHeight + static_cast<int>(width / aspect + 0.5);
        return QSize(
            parentWidget() != nullptr ? std::min(width, parentWidget()->width()) : width,
            parentWidget() != nullptr ? std::min(requestedHeight, parentWidget()->height()) : requestedHeight);
    }
    requestedHeight = std::max(minimumHeight(), requestedHeight);
    return QSize(
        parentWidget() != nullptr ? std::min(requestedWidth, parentWidget()->width()) : requestedWidth,
        parentWidget() != nullptr ? std::min(requestedHeight, parentWidget()->height()) : requestedHeight);
}

void CameraPreviewWidget::updateTitleText()
{
    if(m_titleLabel == nullptr) {
        return;
    }
    m_titleLabel->setText(m_titleLabel->fontMetrics().elidedText(
        m_cameraName,
        Qt::ElideRight,
        std::max(24, m_titleLabel->width())));
    m_titleLabel->setToolTip(QStringLiteral("%1\nCamera instance: %2")
        .arg(m_cameraName, m_cameraId));
}

void CameraPreviewWidget::updateFramePixmap()
{
    if(m_frameImage.isNull()) {
        return;
    }
    m_imageLabel->setPixmap(QPixmap::fromImage(m_frameImage).scaled(
        m_imageLabel->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
}
