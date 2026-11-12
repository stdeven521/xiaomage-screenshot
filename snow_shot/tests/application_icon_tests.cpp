#include "snow_shot/presentation/components/icons/snowshoticons.h"

#include "icon_renderer.h"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void requirePersonalIconAppearance(const QImage& image) {
    require(!image.isNull() && image.hasAlphaChannel(),
            "the application icon must preserve transparency");
    for (const QPoint corner : {image.rect().topLeft(), image.rect().topRight(),
                                image.rect().bottomLeft(), image.rect().bottomRight()}) {
        require(image.pixelColor(corner).alpha() == 0,
                "rounded application icons must have transparent corners");
    }
    bool containsSkyBlue = false;
    bool containsWhite = false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() < 240)
                continue;
            containsSkyBlue |= std::abs(color.red() - 56) <= 4 &&
                               std::abs(color.green() - 189) <= 4 &&
                               std::abs(color.blue() - 248) <= 4;
            containsWhite |= color.red() >= 245 && color.green() >= 245 && color.blue() >= 245;
        }
    }
    require(containsSkyBlue, "the application icon must retain its sky-blue background");
    require(containsWhite, "the application icon must retain its contrasting white letter");
}

void applicationIconScalesWithoutEmbeddedRasterImages() {
    const auto icon = snow_shot::presentation::icons::custom::app::ApplicationIcon();
    require(icon.isValid(), "application icon must have a valid descriptor");
    const auto svg = icon.descriptor()->svg;
    require(svg.find("<image") == std::string_view::npos &&
                svg.find("data:image") == std::string_view::npos,
            "the small caption icon must rasterize vector paths directly at the target DPI");

    for (const qreal scale : {1.0, 1.25, 1.5, 1.75, 2.0, 3.0}) {
        adqt::icons::IconRenderRequest request;
        request.logicalSize = QSize(16, 16);
        request.devicePixelRatio = scale;
        const QPixmap pixmap = adqt::icons::renderIconPixmap(icon, request);
        require(pixmap.size() == QSize(qRound(16 * scale), qRound(16 * scale)) &&
                    pixmap.devicePixelRatio() == scale,
                "caption icon must provide physical pixels for the window's display scale");
        const QImage image = pixmap.toImage();
        requirePersonalIconAppearance(image);
    }
}

void titleBarIconUsesThePersonalArtwork() {
    const auto icon = snow_shot::presentation::icons::custom::app::ApplicationTitleBarIcon();
    require(icon.isValid(), "title bar icon must have a valid descriptor");
    for (const qreal scale : {1.0, 1.25, 1.5, 1.75, 2.0, 3.0}) {
        adqt::icons::IconRenderRequest request;
        request.logicalSize = QSize(16, 16);
        request.devicePixelRatio = scale;
        const QPixmap pixmap = adqt::icons::renderIconPixmap(icon, request);
        require(pixmap.size() == QSize(qRound(16 * scale), qRound(16 * scale)) &&
                    pixmap.devicePixelRatio() == scale,
                "title bar icon must render physical pixels for the display scale");
        requirePersonalIconAppearance(pixmap.toImage());
    }
}

void installedApplicationIconPreservesThePersonalArtwork() {
    namespace icons = snow_shot::presentation::icons::custom;
    QApplication::setWindowIcon(adqt::icons::makeIcon(icons::app::ApplicationIcon()));
    const QIcon installedIcon = QApplication::windowIcon();
    require(!installedIcon.isNull(), "QApplication did not retain the personal icon");

    for (const int size : {16, 20, 24, 32, 40, 48, 64}) {
        const QPixmap pixmap = installedIcon.pixmap(QSize(size, size));
        require(!pixmap.isNull() && pixmap.size() == QSize(size, size),
                "application icon should provide every taskbar raster size");
        requirePersonalIconAppearance(pixmap.toImage());
    }
}

#ifdef Q_OS_WIN
void executableIconResourcePreservesThePersonalArtwork() {
    HMODULE module = GetModuleHandleW(nullptr);
    require(module != nullptr, "application icon test could not resolve its module");
    for (const int size : {16, 32, 48}) {
        HICON icon = static_cast<HICON>(
            LoadImageW(module, MAKEINTRESOURCEW(101), IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
        require(icon != nullptr, "executable did not contain its application icon");
        const QImage image = QImage::fromHICON(icon);
        DestroyIcon(icon);
        require(image.size() == QSize(size, size),
                "embedded application icon must provide the requested taskbar size");
        requirePersonalIconAppearance(image);
    }
}
#endif

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        applicationIconScalesWithoutEmbeddedRasterImages();
        titleBarIconUsesThePersonalArtwork();
        installedApplicationIconPreservesThePersonalArtwork();
#ifdef Q_OS_WIN
        executableIconResourcePreservesThePersonalArtwork();
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
