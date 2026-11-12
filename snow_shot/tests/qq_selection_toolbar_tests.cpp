#include "snow_shot/presentation/screenshotselectionlimits.h"
#include "snow_shot/presentation/screenshotselectiontoolbarwidget.h"
#include "snow_shot/presentation/screenshottoolbarcommands.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QLabel>
#include <QSlider>
#include <QTranslator>
#include <QVariant>

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

class RecordingCommands final : public ScreenshotSelectionToolbarCommandSink {
  public:
    void toggleSelectionAspectRatioLockFromToolbar() override {}
    void setSelectionAspectRatioPresetFromToolbar(ScreenshotSelectionAspectRatioPreset) override {}
    void openSelectionResizeModalFromToolbar() override {}
    void hideColorPickersForScreenshotUi() override {}
    void adjustSelectionFromToolbar(int, int, int, int) override {}
    void adjustSelectionEffectsFromWheel(int, int) override {}
    void setSelectionToolbarHovered(bool) override {}
    void setSelectionCornerRadiusFromToolbar(int radius) override {
        radii.push_back(radius);
    }
    void setSelectionEffectWidthFromToolbar(int width) override {
        effectWidths.push_back(width);
    }
    void setSelectionShadowWidthFromToolbar(int width) override {
        shadowWidths.push_back(width);
    }

    std::vector<int> radii;
    std::vector<int> effectWidths;
    std::vector<int> shadowWidths;
};

struct Fixture {
    RecordingCommands commands;
    QWidget host;
    ScreenshotSelectionToolbarWidget toolbar{commands, &host};

    Fixture() {
        host.resize(1200, 400);
        toolbar.setSelectionState(QRect(30, 40, 438, 385), false, 0, 0);
        host.show();
        toolbar.show();
        QApplication::processEvents();
    }

    template <typename T> T* control(const char* name) {
        T* result = toolbar.findChild<T*>(QString::fromLatin1(name));
        require(result != nullptr, "expected toolbar control should exist");
        return result;
    }

    QSlider* slider() {
        return control<QSlider>("screenshotCompatibilityRadiusSlider");
    }

    QCheckBox* shadow() {
        return control<QCheckBox>("screenshotCompatibilityShadowCheckBox");
    }

    QLabel* field(int id) {
        for (QLabel* label : toolbar.findChildren<QLabel*>()) {
            const QVariant field = label->property("selectionToolbarField");
            if (field.isValid() && field.toInt() == id) {
                return label;
            }
        }
        require(false, "expected selection field should exist");
        return nullptr;
    }

    void setState(int radius, int shadowWidth,
                  ScreenshotSelectionToolbarWidget::DisplayMode mode =
                      ScreenshotSelectionToolbarWidget::DisplayMode::Full) {
        toolbar.setSelectionState(QRect(30, 40, 438, 385), false, radius, shadowWidth, mode);
    }
};

void compatibilityIsOptInAndRestoresGenericControls() {
    Fixture fixture;
    QWidget* ratio = fixture.control<QWidget>("screenshotSelectionAspectRatioSelect");
    require(fixture.field(0)->isVisible() && fixture.field(1)->isVisible(),
            "generic toolbar should keep coordinates visible by default");
    require(ratio->isVisible() && fixture.field(5)->isVisible(),
            "generic toolbar should keep ratio and numeric effect width");
    require(!fixture.slider()->isVisible() && !fixture.shadow()->isVisible(),
            "compatibility controls should be hidden by default");

    fixture.toolbar.setScreenshotCompatibilityEnabled(true);
    require(fixture.toolbar.contentSizeHint().height() == 24,
            "36 physical pixels at the reference 150 percent DPI must be 24 logical pixels");
    require(fixture.slider()->size() == QSize(100, 22),
            "QQ slider must fit the compact row without expanding its logical height");
    require(!fixture.field(0)->isVisible() && !fixture.field(1)->isVisible(),
            "compatibility toolbar should hide coordinates");
    require(!ratio->isVisible() && !fixture.field(5)->isVisible(),
            "compatibility toolbar should hide ratio and numeric effect width");
    require(fixture.field(2)->text() == QStringLiteral("438") &&
                fixture.field(3)->text() == QStringLiteral("385"),
            "compatibility toolbar should keep the selected dimensions");
    require(fixture.slider()->isVisible() && fixture.shadow()->isVisible(),
            "compatibility toolbar should show radius slider and shadow toggle");
    bool asteriskVisible = false;
    for (QLabel* label : fixture.toolbar.findChildren<QLabel*>()) {
        if (!label->isVisible()) {
            continue;
        }
        require(label->text() != QStringLiteral("px"),
                "compatibility toolbar should omit pixel units");
        asteriskVisible |= label->text() == QStringLiteral("*");
    }
    require(asteriskVisible, "compatibility dimensions should use an asterisk separator");

    fixture.toolbar.setScreenshotCompatibilityEnabled(false);
    require(fixture.toolbar.contentSizeHint().height() == 26,
            "disabling compatibility must restore the generic row height");
    require(fixture.field(0)->isVisible() && ratio->isVisible() && fixture.field(5)->isVisible(),
            "disabling compatibility should restore generic controls");
    require(!fixture.slider()->isVisible() && !fixture.shadow()->isVisible(),
            "disabling compatibility should hide its controls");
}

void radiusSynchronizesAndRespectsEditingCapabilities() {
    Fixture fixture;
    fixture.toolbar.setScreenshotCompatibilityEnabled(true);
    fixture.setState(42, 0);
    QLabel* value = fixture.control<QLabel>("screenshotCompatibilityRadiusValue");
    require(fixture.slider()->value() == 42 && value->text() == QStringLiteral("42"),
            "radius slider and numeric value should follow model state");
    require(fixture.commands.radii.empty() && fixture.commands.shadowWidths.empty(),
            "model synchronization must not dispatch edit commands");

    fixture.slider()->setValue(98);
    require(fixture.commands.radii == std::vector<int>{98},
            "radius edits should dispatch the existing radius command once");
    fixture.setState(98, 0);
    const int valueWidth = value->width();
    fixture.setState(0, 0);
    require(value->width() == valueWidth,
            "radius digits should not change the slider position while dragging");

    fixture.toolbar.setCornerRadiusApplicable(false);
    require(!fixture.slider()->isEnabled() && fixture.shadow()->isEnabled(),
            "custom regions should disable radius without disabling shadow");
    fixture.slider()->setValue(12);
    require(fixture.commands.radii.size() == 1, "unavailable radius must not dispatch commands");
    fixture.toolbar.setCornerRadiusApplicable(true);
    require(fixture.slider()->value() == 0 && fixture.slider()->isEnabled(),
            "restoring applicability should restore the authoritative radius");

    fixture.toolbar.setSelectionEditCapabilities(false, false);
    require(!fixture.slider()->isEnabled() && !fixture.shadow()->isEnabled(),
            "read-only selection should disable effect controls");
    fixture.slider()->setValue(15);
    fixture.shadow()->click();
    require(fixture.commands.radii.size() == 1 && fixture.commands.shadowWidths.empty(),
            "read-only selection must not dispatch effect commands");
    fixture.toolbar.setSelectionEditCapabilities(true, false);
    require(fixture.slider()->isEnabled() && fixture.shadow()->isEnabled(),
            "non-resizable editable selections should still support effects");
    fixture.toolbar.setPointerInteractionEnabled(false);
    require(!fixture.slider()->isEnabled() && !fixture.shadow()->isEnabled(),
            "selection dragging should disable effect controls");
    fixture.toolbar.setPointerInteractionEnabled(true);
    fixture.setState(10, 0, ScreenshotSelectionToolbarWidget::DisplayMode::SizeOnly);
    require(!fixture.slider()->isVisible() && !fixture.shadow()->isVisible(),
            "smart-selection size-only mode should omit effect controls");
    fixture.setState(10, 0);
    require(fixture.slider()->isEnabled() && fixture.slider()->value() == 10,
            "returning to full mode should refresh the radius control");
}

void shadowToggleRemembersOnlyShadowWidth() {
    Fixture fixture;
    fixture.toolbar.setScreenshotCompatibilityEnabled(true);
    fixture.shadow()->click();
    require(fixture.commands.shadowWidths.size() == 1 && fixture.commands.shadowWidths[0] > 0 &&
                fixture.commands.shadowWidths[0] <=
                    snow_shot::presentation::kScreenshotSelectionShadowWidthMax,
            "first shadow toggle should use a valid nonzero default");
    fixture.commands.shadowWidths.clear();
    fixture.setState(0, 24);
    require(fixture.shadow()->isChecked() && fixture.commands.shadowWidths.empty(),
            "external shadow state should synchronize without sending commands");
    fixture.shadow()->click();
    require(fixture.commands.shadowWidths == std::vector<int>{0},
            "unchecking shadow should disable the effect");
    fixture.setState(0, 0);
    fixture.shadow()->click();
    require(fixture.commands.shadowWidths == std::vector<int>({0, 24}),
            "enabling shadow should restore its last nonzero width");

    fixture.toolbar.setBorderType(ScreenshotSelectionBorderType::Border);
    fixture.setState(0, 45);
    require(!fixture.shadow()->isChecked(), "a border must not appear as an enabled shadow");
    fixture.shadow()->click();
    require(fixture.commands.shadowWidths.back() == 24 && fixture.commands.effectWidths.empty(),
            "shadow toggle should select shadow explicitly without remembering border width");

    fixture.toolbar.setBorderType(ScreenshotSelectionBorderType::Shadow);
    fixture.setState(0, 24);
    fixture.toolbar.resetForNewCapture();
    require(!fixture.shadow()->isChecked(), "new capture should synchronize its cleared effect");
    fixture.shadow()->click();
    require(fixture.commands.shadowWidths.back() == 24,
            "new capture should retain the previous nonzero shadow width");
}

class CompatibilityTranslator final : public QTranslator {
  public:
    bool isEmpty() const override {
        return false;
    }

    QString translate(const char* context, const char* source, const char*, int) const override {
        if (QByteArray(context) == "ScreenshotSelectionToolbarWidget") {
            if (QByteArray(source) == "Corner radius") {
                return QStringLiteral("Translated radius");
            }
            if (QByteArray(source) == "Shadow") {
                return QStringLiteral("Translated shadow");
            }
        }
        return {};
    }
};

void compatibilityControlsRetranslateAtRuntime() {
    Fixture fixture;
    fixture.toolbar.setScreenshotCompatibilityEnabled(true);
    CompatibilityTranslator translator;
    QApplication::installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QApplication::sendEvent(&fixture.toolbar, &languageChange);
    require(fixture.control<QLabel>("screenshotCompatibilityRadiusCaption")->text() ==
                QStringLiteral("Translated radius"),
            "radius caption should retranslate at runtime");
    require(fixture.slider()->accessibleName() == QStringLiteral("Translated radius") &&
                fixture.shadow()->text() == QStringLiteral("Translated shadow"),
            "compatibility controls should retranslate accessible names and checkbox text");
    QApplication::removeTranslator(&translator);
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    compatibilityIsOptInAndRestoresGenericControls();
    radiusSynchronizesAndRespectsEditingCapabilities();
    shadowToggleRemembersOnlyShadowWidth();
    compatibilityControlsRetranslateAtRuntime();
    return 0;
}
