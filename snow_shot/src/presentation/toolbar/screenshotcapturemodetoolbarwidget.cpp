// SPDX-License-Identifier: GPL-3.0-or-later
#include "snow_shot/presentation/screenshotcapturemodetoolbarwidget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>

#include <algorithm>

namespace {
class ModeDragHandle final : public QWidget {
  public:
    explicit ModeDragHandle(QWidget* parent) : QWidget(parent) {
        setFixedSize(24, 32);
        setCursor(Qt::SizeAllCursor);
    }

  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(isEnabled() ? QColor("#cccccc") : QColor("#777777"));
        for (int x : {9, 15})
            for (int y : {10, 16, 22})
                painter.drawEllipse(QPointF(x, y), 1.2, 1.2);
    }
};
constexpr std::array<ScreenshotCaptureToolbarMode, 3> kModes = {
    ScreenshotCaptureToolbarMode::Screenshot, ScreenshotCaptureToolbarMode::Recording,
    ScreenshotCaptureToolbarMode::Scrolling};
} // namespace

ScreenshotCaptureModeToolbarWidget::ScreenshotCaptureModeToolbarWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("screenshotCaptureModeToolbar"));
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus);
    QFont labelFont = font();
    labelFont.setPixelSize(14);
    setFont(labelFont);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(4);
    m_dragHandle = new ModeDragHandle(this);
    m_dragHandle->setObjectName(QStringLiteral("captureModeDragHandle"));
    m_dragHandle->installEventFilter(this);
    row->addWidget(m_dragHandle);
    for (std::size_t index = 0; index < m_buttons.size(); ++index) {
        auto* button = new QToolButton(this);
        m_buttons[index] = button;
        button->setObjectName(QStringLiteral("captureMode_%1").arg(index));
        button->setFocusPolicy(Qt::NoFocus);
        button->setCheckable(true);
        button->setFixedHeight(32);
        button->setStyleSheet(
            QStringLiteral("QToolButton { border: none; border-radius: 6px; padding: 0 14px;"
                           " background: transparent; color: #e7e7e7; }"
                           "QToolButton:hover { background: #404040; }"
                           "QToolButton:checked { background: #555555; }"
                           "QToolButton:disabled { color: #777777; }"));
        row->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, index] {
            // The controller owns the mode; rejected requests must not alter the checked tab.
            setMode(m_mode);
            emit modeRequested(kModes[index]);
        });
    }
    retranslate();
    setMode(m_mode);
    setFixedHeight(40);
    adjustSize();
}

void ScreenshotCaptureModeToolbarWidget::setMode(ScreenshotCaptureToolbarMode mode) {
    m_mode = mode;
    for (std::size_t index = 0; index < m_buttons.size(); ++index)
        m_buttons[index]->setChecked(kModes[index] == mode);
}

void ScreenshotCaptureModeToolbarWidget::retranslate() {
    setAccessibleName(tr("Capture mode"));
    m_dragHandle->setAccessibleName(tr("Move capture toolbar"));
    const std::array<QString, 3> labels = {tr("Screenshot"), tr("Record screen"),
                                           tr("Scrolling screenshot")};
    for (std::size_t index = 0; index < m_buttons.size(); ++index) {
        m_buttons[index]->setText(labels[index]);
        m_buttons[index]->setAccessibleName(labels[index]);
    }
    adjustSize();
}

bool ScreenshotCaptureModeToolbarWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_dragHandle) {
        if (event->type() == QEvent::MouseButtonPress && isEnabled()) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragOffset = mouse->globalPosition().toPoint() - mapToGlobal(QPoint());
                m_dragging = true;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging && isEnabled()) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (auto* owner = parentWidget()) {
                const QPoint position =
                    owner->mapFromGlobal(mouse->globalPosition().toPoint() - m_dragOffset);
                move(std::clamp(position.x(), 0, std::max(0, owner->width() - width())),
                     std::clamp(position.y(), 0, std::max(0, owner->height() - height())));
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            m_dragging = false;
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ScreenshotCaptureModeToolbarWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    if (event->type() == QEvent::EnabledChange && !isEnabled())
        m_dragging = false;
}

void ScreenshotCaptureModeToolbarWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#424242"));
    painter.setBrush(QColor("#303030"));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}
