// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPoint>
#include <QWidget>

#include <array>

class QToolButton;

enum class ScreenshotCaptureToolbarMode { Screenshot, Recording, Scrolling };

class ScreenshotCaptureModeToolbarWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit ScreenshotCaptureModeToolbarWidget(QWidget* parent = nullptr);
    void setMode(ScreenshotCaptureToolbarMode mode);
    [[nodiscard]] ScreenshotCaptureToolbarMode mode() const {
        return m_mode;
    }

  signals:
    void modeRequested(ScreenshotCaptureToolbarMode mode);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

  private:
    void retranslate();
    QWidget* m_dragHandle = nullptr;
    std::array<QToolButton*, 3> m_buttons{};
    ScreenshotCaptureToolbarMode m_mode = ScreenshotCaptureToolbarMode::Screenshot;
    QPoint m_dragOffset;
    bool m_dragging = false;
};
