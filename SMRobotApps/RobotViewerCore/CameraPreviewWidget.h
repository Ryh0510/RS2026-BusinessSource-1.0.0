#pragma once

#include <QFrame>
#include <QImage>

class QLabel;
class QToolButton;

enum class CameraPreviewStatus
{
    Running,
    Paused,
    Suspended,
    Stale,
    Error,
    Disabled
};

class CameraPreviewWidget final : public QFrame
{
    Q_OBJECT

public:
    CameraPreviewWidget(const QString& cameraId, const QString& name, QWidget* parent = nullptr);

    QString cameraId() const;
    bool streamRunning() const;
    bool positionLocked() const;
    void setStreamRunning(bool running);
    void setPositionLocked(bool locked);
    void setFrameImage(const QImage& image);
    void setStatus(CameraPreviewStatus status, const QString& detail = QString());
    void resetPreviewSize();
    QSize requestedRenderSize(int sourceWidth, int sourceHeight) const;
    void clampToParent();

signals:
    void streamRunningChanged(bool running);
    void previewVisibilityChanged(bool visible);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QSize constrainedResizeSize(const QPoint& globalPosition) const;
    void updateTitleText();
    void updateFramePixmap();

    QString m_cameraId;
    QString m_cameraName;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_dragHandleLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_imageLabel = nullptr;
    QLabel* m_messageOverlay = nullptr;
    QToolButton* m_runButton = nullptr;
    QToolButton* m_lockButton = nullptr;
    QToolButton* m_closeButton = nullptr;
    QLabel* m_resizeHandle = nullptr;
    QImage m_frameImage;
    QPoint m_dragOffset;
    QPoint m_resizeStartGlobal;
    QSize m_resizeStartSize;
    bool m_dragging = false;
    bool m_resizing = false;
    bool m_locked = false;
    CameraPreviewStatus m_status = CameraPreviewStatus::Paused;
};
