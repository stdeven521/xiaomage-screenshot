#pragma once

#include <QColor>
#include <QVariant>
#include <QWidget>

// A palette-scoped profile: other editors retain their normal application theme.
inline bool screenshotToolbarUsesQqAppearance(const QWidget* widget) {
    for (auto* ancestor = widget; ancestor != nullptr; ancestor = ancestor->parentWidget()) {
        if (ancestor->property("snowShotQqScreenshotAppearance").toBool())
            return true;
    }
    return false;
}

inline QColor qqScreenshotToolbarSurface() {
    return QColor(0x30, 0x30, 0x30);
}

inline QColor qqScreenshotToolbarForeground() {
    return QColor(0xe7, 0xe7, 0xe7);
}

inline QColor qqScreenshotToolbarSuccess() {
    return QColor(0x00, 0xd5, 0x83);
}
