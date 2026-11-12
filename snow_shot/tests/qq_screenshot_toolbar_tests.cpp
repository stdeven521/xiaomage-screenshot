#include "snow_shot/presentation/screenshottoolbarlayoutmodel.h"
#include "snow_shot/presentation/screenshottoolpalette.h"
#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "../src/presentation/tools/screenshottoolbarcompatibility.h"

#include "widgets/button.h"
#include "widgets/popover.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDir>
#include <QGraphicsEffect>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QScopeGuard>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace {
namespace layouts = snow_shot::presentation::toolbar_layout;
using Kind = snow_shot::storage::ScreenshotToolbarLayoutKind;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void requirePixelColor(const QImage& image, const QPoint& position, const QColor& expected,
                       const char* message) {
    const QColor actual = image.pixelColor(position);
    if (actual != expected) {
        const QString path =
            QDir::current().filePath(QStringLiteral("snow-shot-qq-toolbar-render-failure.png"));
        std::cerr << "pixel (" << position.x() << ", " << position.y()
                  << ") actual=" << actual.name(QColor::HexArgb).toStdString()
                  << " expected=" << expected.name(QColor::HexArgb).toStdString() << '\n';
        if (image.save(path))
            std::cerr << "render saved to " << path.toStdString() << '\n';
    }
    require(actual == expected, message);
}

void layoutsKeepHiddenToolsAndCustomizations() {
    const auto drawing = layouts::qqScreenshotLayout(Kind::DrawingTools);
    const auto actions = layouts::qqScreenshotLayout(Kind::ActionTools);
    require(layouts::normalizedLayout(drawing, Kind::DrawingTools).positions == drawing.positions,
            "normalization must not resurrect the hidden extra drawing tools");
    require(layouts::normalizedLayout(actions, Kind::ActionTools).positions == actions.positions,
            "normalization must not resurrect the hidden extra actions");
    require(drawing.positions.first() ==
                QStringList{QStringLiteral("line"), QStringLiteral("shape")},
            "line remains available inside the shape group");
    require(drawing.hidden.contains(QStringLiteral("undo")) &&
                drawing.hidden.contains(QStringLiteral("redo")),
            "history is removed from the drawing section");
    require(actions.hidden.contains(QStringLiteral("redo")) &&
                actions.hidden.contains(QStringLiteral("record-screen")),
            "redo and recording are not mistaken for QQ forward or screenshot tools");
    require(layouts::qqScreenshotLayoutForSettings({}, Kind::DrawingTools).positions ==
                drawing.positions,
            "fresh screenshot settings must select the QQ profile");
    require(layouts::qqScreenshotLayoutForSettings(layouts::normalizedLayout({}, Kind::ActionTools),
                                                   Kind::ActionTools)
                    .positions == actions.positions,
            "stored upstream defaults must select the QQ profile");

    auto custom = drawing;
    std::swap(custom.positions[0], custom.positions[1]);
    const auto retained = layouts::qqScreenshotLayoutForSettings(custom, Kind::DrawingTools);
    require(retained.positions == custom.positions && retained.hidden == custom.hidden,
            "explicitly customized layouts must retain order and hidden entries");
    const snow_shot::storage::ScreenshotToolbarSettings settings;
    require(settings.setLayout(Kind::ActionTools, actions), "action layout must be persisted");
    const auto restoredActions = settings.layout(Kind::ActionTools);
    require(restoredActions.positions == actions.positions &&
                restoredActions.hidden == actions.hidden,
            "schema validation must retain action-row history and hidden tools on save");
}

ScreenshotToolPalette::Options options() {
    ScreenshotToolPalette::Options result;
    result.screenshotCompatibility = true;
    result.showDragHandle = true;
    result.enableStyleToolbar = false;
    result.showHistoryActions = true;
    result.showLineTool = true;
    result.showBrushTool = true;
    result.showTextTool = true;
    result.showSequenceNumberTool = true;
    result.showFilterTool = true;
    result.showSpotlightTool = true;
    result.showWatermarkTool = true;
    result.showOcrTool = true;
    result.showTextTranslationTool = true;
    result.showScrollingScreenshotTool = true;
    result.showSaveButton = true;
    result.actions = ScreenshotToolPalette::PinAction | ScreenshotToolPalette::CancelAction |
                     ScreenshotToolPalette::CopyAction;
    return result;
}

adqt::widgets::AdButton* visibleItem(ScreenshotToolPalette& palette, const QString& id) {
    for (auto* button : palette.mainPanel()->findChildren<adqt::widgets::AdButton*>()) {
        if (!button->isHidden() && button->property("screenshotToolbarItemId").toString() == id)
            return button;
    }
    return nullptr;
}

void renderedToolbarHasFunctionalQqOrder() {
    ScreenshotToolPalette palette(options());
    palette.show();
    palette.prepareForDisplay();
    QCoreApplication::processEvents();

    QStringList actual;
    auto* row = palette.mainPanel()->layout();
    for (int index = 0; index < row->count(); ++index) {
        auto* widget = row->itemAt(index)->widget();
        if (!widget || widget->isHidden())
            continue;
        const QString id = widget->property("screenshotToolbarItemId").toString();
        if (!id.isEmpty())
            actual.append(id);
    }
    QStringList expected{QStringLiteral("shape"),
                         QStringLiteral("arrow"),
                         QStringLiteral("free-draw"),
                         QStringLiteral("text"),
                         QStringLiteral("filter"),
                         QStringLiteral("pin-to-screen"),
                         QStringLiteral("scrolling-screenshot")};
    if (snow_shot::app::edition::textTranslation)
        expected.append(QStringLiteral("text-translation"));
    if (!snow_shot::app::edition::isMini)
        expected.append(QStringLiteral("text-recognition"));
    expected.append({QStringLiteral("undo"), QStringLiteral("save-as-file"),
                     QStringLiteral("cancel"), QStringLiteral("copy")});
    require(actual == expected, "visible buttons must follow the measured QQ order");
    require(visibleItem(palette, QStringLiteral("redo")) == nullptr,
            "no redo button may occupy the unimplemented QQ forward slot");

    auto* text = visibleItem(palette, QStringLiteral("text"));
    auto* process = visibleItem(palette, QStringLiteral("filter"));
    require(text && text->property("screenshotToolbarPositionItems")
                        .toStringList()
                        .contains(QStringLiteral("serial-number")),
            "text must expose the sequence number alternative");
    require(process && process->property("screenshotToolbarPositionItems").toStringList() ==
                           QStringList{QStringLiteral("watermark"), QStringLiteral("spotlight"),
                                       QStringLiteral("filter")},
            "mosaic must expose spotlight and watermark alternatives");
    auto* textMenu = text->findChild<adqt::widgets::AdPopover*>();
    require(textMenu != nullptr, "text stack must have an executable dropdown");
    int textActivations = 0;
    QObject::connect(&palette, &ScreenshotToolPalette::textRequested, &palette,
                     [&textActivations] { ++textActivations; });
    const QPointF clickPosition(text->width() - 2, text->height() / 2);
    QMouseEvent press(QEvent::MouseButtonPress, clickPosition,
                      QPointF(text->mapToGlobal(clickPosition.toPoint())), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(text, &press);
    QCoreApplication::processEvents();
    require(textMenu->contentWidget() != nullptr,
            "clicking the chevron must open choices without activating the current tool");
    require(textActivations == 0, "dropdown click must not run the current text command");
    auto* sequence = textMenu->contentWidget()->findChild<adqt::widgets::AdButton*>(
        QStringLiteral("screenshotDrawingToolGroupOption-serial-number"));
    require(sequence != nullptr, "sequence number must be reachable in the text dropdown");
    sequence->click();
    require(visibleItem(palette, QStringLiteral("serial-number")) != nullptr,
            "dropdown selection must activate and remember the sequence tool");
    require(palette.activateDrawingShortcut(QStringLiteral("line")),
            "line must still be executable from its grouped entry");
    require(visibleItem(palette, QStringLiteral("line")) != nullptr,
            "shape group must reflect the selected line tool");

    auto* undo = visibleItem(palette, QStringLiteral("undo"));
    require(undo && !undo->isEnabled(), "empty screenshot history must disable undo");
    int undoCount = 0;
    QObject::connect(&palette, &ScreenshotToolPalette::undoRequested, &palette,
                     [&undoCount] { ++undoCount; });
    SnowCanvasHistoryState history;
    history.canUndo = true;
    palette.setHistoryState(history);
    require(undo->isEnabled(), "action-row undo must track canvas history availability");
    undo->click();
    require(undoCount == 1, "action-row undo must invoke the existing history command once");

    int copyCount = 0;
    QObject::connect(&palette, &ScreenshotToolPalette::copyRequested, &palette,
                     [&copyCount] { ++copyCount; });
    auto* complete = visibleItem(palette, QStringLiteral("copy"));
    require(complete != nullptr, "complete must be present");
    complete->click();
    require(copyCount == 1, "green check must use the existing clipboard completion command");

    for (const auto appearance : {snow_shot::presentation::styles::ThemeAppearance::Light,
                                  snow_shot::presentation::styles::ThemeAppearance::Dark}) {
        snow_shot::presentation::styles::ThemeManager::instance().setThemeAppearance(appearance);
        QCoreApplication::processEvents();
        auto* panel = palette.mainPanel();
        auto* effect = panel->graphicsEffect();
        const bool effectWasEnabled = effect && effect->isEnabled();
        // Match the existing main-panel surface tests: a direct child render with a graphics
        // effect goes through its cached composite, not just the requested panel paint pass.
        if (effect)
            effect->setEnabled(false);
        QImage image(panel->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        panel->render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground);
        painter.end();
        if (effect)
            effect->setEnabled(effectWasEnabled);
        requirePixelColor(
            image, image.rect().center(), qqScreenshotToolbarSurface(),
            "QQ toolbar surface must stay dark independently of the application theme");
        auto* handle = palette.dragHandle();
        require(handle != nullptr, "QQ toolbar must have its drag handle");
        requirePixelColor(
            image, QPoint(handle->geometry().center().x(), 2), QColor(0x3c, 0x3c, 0x3c),
            "QQ drag strip must keep its lighter gray surface within the rounded panel");

        // Also verify the real parent composite with the effect restored. The top padding is
        // inside the opaque surface and outside button/icon ink, unlike the row's center.
        QImage composite(palette.size(), QImage::Format_ARGB32_Premultiplied);
        composite.fill(Qt::transparent);
        QPainter compositePainter(&composite);
        palette.render(&compositePainter);
        compositePainter.end();
        requirePixelColor(composite, panel->mapTo(&palette, QPoint(panel->width() / 2, 2)),
                          qqScreenshotToolbarSurface(),
                          "QQ toolbar's normal shadow composite must preserve its dark surface");
    }
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QTemporaryDir storageDirectory;
    require(storageDirectory.isValid() &&
                snow_shot::storage::ApplicationStorage::instance()
                    .initialize({storageDirectory.path(), storageDirectory.path(), 60000})
                    .success,
            "initialize isolated QQ toolbar settings");
    const auto shutdown =
        qScopeGuard([] { snow_shot::storage::ApplicationStorage::instance().shutdown(); });
    layoutsKeepHiddenToolsAndCustomizations();
    renderedToolbarHasFunctionalQqOrder();
    return 0;
}
