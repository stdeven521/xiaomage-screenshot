#include "snow_canvas_text.h"
#include "snow_canvas_text_draft.h"
#include "snow_canvas_text_editor_session.h"
#include "snow_draw_engine_qt/snow_canvas_widget.h"

#include <QApplication>
#include <QFont>
#include <QInputMethodEvent>
#include <QKeyEvent>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void draftHistoryKeepsActualEditsAndExcludesAbandonedRedoAndPreedit() {
    SnowCanvasTextDraft draft;
    draft.begin(QString());
    for (const QChar character : QStringLiteral("文字测试")) {
        require(draft.replaceSelection(QString(character)), "each character should edit the draft");
    }
    require(draft.committedEditStates() ==
                std::vector<QString>{QStringLiteral("文"), QStringLiteral("文字"),
                                     QStringLiteral("文字测"), QStringLiteral("文字测试")},
            "the actual character edit states must be retained");
    require(draft.undoEdit(), "local undo should remove the last character");
    require(draft.replaceSelection(QStringLiteral("新")), "typing should branch local history");
    QInputMethodEvent preedit(QStringLiteral("unfinished"), {});
    draft.handleInputMethodEvent(preedit);
    require(draft.committedEditStates().back() == QStringLiteral("文字测新"),
            "uncommitted preedit and abandoned redo must not reach document history");
    require(draft.committedEditStates().size() == 4,
            "branching should not retain an abandoned document history step");
}

void screenshotEnterDoesNotCommitAndHistoryKeepsMeasuredGeometry() {
    const QFont font = QApplication::font();
    SnowTextStyle style{};
    style.font_size = 24.0;
    style.opacity = 1.0;
    style.color = SnowColorRgba8{255, 0, 0, 255};
    const auto info = snow_canvas_text::newTextInfoAt(QPointF(100, 100), font, style);
    SnowCanvasTextEditorSession session;
    session.setScreenshotCompatibilityEnabled(true);
    require(session.begin(info, nullptr, font, &style), "screenshot text should begin");
    SceneDisplayInfo scene{};
    for (const auto modifiers :
         {Qt::KeyboardModifiers{}, Qt::KeyboardModifiers{Qt::ShiftModifier}}) {
        QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, modifiers);
        const auto result = session.handleKeyPress(&enter, font, scene);
        require(result.handled && !result.changed &&
                    result.command == SnowCanvasTextEditorSession::EventCommand::None,
                "Enter and Shift+Enter must neither insert a line nor finish the screenshot");
    }
    for (const auto* text : {"a", "b", "c"}) {
        QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QString::fromLatin1(text));
        require(session.handleKeyPress(&key, font, scene).changed, "typing must edit the draft");
    }
    const auto edit = session.finish(font);
    require(edit.text == QStringLiteral("abc") && edit.historySteps.size() == 3,
            "finishing must return each real text edit");
    require(edit.historySteps.front().measuredLayout.width < edit.measuredLayout.width,
            "earlier undo states must carry their own measured text geometry");
    require(std::abs(edit.historySteps.back().measuredLayout.width - edit.measuredLayout.width) <
                0.001,
            "the final history state must match the rendered final draft");

    SnowCanvasTextEditorSession normal;
    require(normal.begin(info, nullptr, font, &style), "generic text should begin");
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    require(normal.handleKeyPress(&enter, font, scene).changed,
            "generic library text must retain newline input");
    require(normal.finish(font).historySteps.empty(),
            "generic text must preserve its original single-commit behavior");
}

void watermarkPolicyIsExplicitlyOptIn() {
    SnowCanvasWidget screenshot;
    screenshot.setScreenshotCompatibilityEnabled(true);
    auto watermark = screenshot.canvasWatermarkConfig();
    watermark.text = QStringLiteral("watermark");
    require(screenshot.setCanvasWatermarkConfig(watermark), "screenshot watermark must apply");
    require(!screenshot.canvasHistoryState().canUndo, "watermark must not enable annotation undo");
    watermark.text.clear();
    require(screenshot.setCanvasWatermarkConfig(watermark), "watermark must clear directly");
    require(!screenshot.canvasHistoryState().canUndo, "clearing must not create undo history");
    SnowCanvasWidget generic;
    watermark.text = QStringLiteral("generic");
    require(generic.setCanvasWatermarkConfig(watermark), "generic watermark must apply");
    require(generic.canvasHistoryState().canUndo, "generic watermark must remain undoable");
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    draftHistoryKeepsActualEditsAndExcludesAbandonedRedoAndPreedit();
    screenshotEnterDoesNotCommitAndHistoryKeepsMeasuredGeometry();
    watermarkPolicyIsExplicitlyOptIn();
    return 0;
}
