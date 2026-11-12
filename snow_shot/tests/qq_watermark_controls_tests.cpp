#include "snow_shot/presentation/screenshottoolpalette.h"
#include "snow_shot/storage/applicationstorage.h"
#include "palette_style_edit_test_support.h"
#include "widgets/input_line_edit.h"

#include <QApplication>
#include <QDir>
#include <QLayout>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QToolButton>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void checkWatermarkControls(bool compatibility) {
    ScreenshotToolPalette::Options options;
    options.screenshotCompatibility = compatibility;
    options.showShapeTool = false;
    options.showWatermarkTool = true;
    options.showTextTool = true;
    ScreenshotToolPalette palette(options);
    QString committedText;
    int edits = 0;
    observePaletteStyleEdit<SnowCanvasWatermarkEdit>(
        palette, [&](const SnowCanvasWatermarkConfig& config, quint32 properties) {
            if ((properties & SnowCanvasWatermarkText) != 0) {
                committedText = config.text;
                ++edits;
            }
        });
    palette.setActiveTool(ScreenshotToolPalette::Tool::Watermark);
    auto* text = palette.findChild<adqt::widgets::AdLineEdit*>(
        QStringLiteral("screenshotWatermarkTextEdit"));
    require(text != nullptr, "watermark activation must create a text input");
    const QString sixteen = QString::fromUtf8("一二三四五六七八九十甲乙丙丁戊己");
    text->setText(sixteen);
    text->setCursorPosition(text->text().size());
    text->insert(QString::fromUtf8("庚"));
    require(text->text() == (compatibility ? sixteen : sixteen + QString::fromUtf8("庚")),
            "QQ input rejects the seventeenth Chinese character; generic input remains unlimited");
    require(committedText == text->text(), "the committed watermark must match visible input");
    require(text->allowClear() == compatibility, "QQ watermark must expose its clear action");
    auto* advanced = palette.findChild<QWidget*>(QStringLiteral("screenshotWatermarkAngleEditor"));
    require((advanced == nullptr) == compatibility,
            "only the generic palette exposes advanced watermark controls");
    if (!compatibility)
        return;
    require(text->width() == 191, "QQ input width must be expressed in logical pixels");

    auto* row = palette.findChild<QWidget*>(QStringLiteral("screenshotWatermarkStyleControls"));
    require(row != nullptr && row->layout()->itemAt(0)->widget() == text,
            "QQ watermark input precedes the separator and color picker");
    auto* clear = text->findChild<QToolButton*>(QStringLiteral("ad-input-clear"));
    require(clear != nullptr, "clear button must exist");
    const int editsBeforeClear = edits;
    clear->click();
    require(text->text().isEmpty() && committedText.isEmpty() && edits == editsBeforeClear + 1,
            "clear must issue one empty watermark edit immediately");

    text->setText(sixteen);
    palette.setActiveTool(ScreenshotToolPalette::Tool::Text);
    palette.setActiveTool(ScreenshotToolPalette::Tool::Watermark);
    text = palette.findChild<adqt::widgets::AdLineEdit*>(
        QStringLiteral("screenshotWatermarkTextEdit"));
    require(text != nullptr && text->text() == sixteen && text->maxLength() == 16 &&
                text->allowClear(),
            "tool switches must retain the watermark and its QQ input behavior");
    SnowCanvasWatermarkConfig config;
    config.text = QStringLiteral("external update");
    palette.setWatermarkConfig(config);
    require(text->text() == config.text,
            "canvas updates must refresh the compact editor after a tool switch");
    config.text = QStringLiteral("existing watermark longer than sixteen");
    palette.setWatermarkConfig(config);
    require(text->text() == config.text,
            "opening legacy watermarks must not silently truncate the displayed content");
    text->clear();
    require(text->maxLength() == 16, "clearing a legacy watermark restores the QQ input limit");
    palette.setPhysicalScale(1.25);
    require(text->width() == qRound(191 * 1.25),
            "DPI metric refresh must preserve the reference input width");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "temporary storage must be available");
    const QString executableDirectory = QDir(directory.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executableDirectory), "temporary executable directory must be available");
    require(snow_shot::storage::ApplicationStorage::instance()
                .initialize({executableDirectory, directory.path(), 60000})
                .success,
            "isolated storage must initialize");
    const auto shutdown =
        qScopeGuard([] { snow_shot::storage::ApplicationStorage::instance().shutdown(); });
    checkWatermarkControls(true);
    checkWatermarkControls(false);
    return EXIT_SUCCESS;
}
