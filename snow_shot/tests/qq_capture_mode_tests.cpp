// SPDX-License-Identifier: GPL-3.0-or-later
#include "screenshot_controller_test_access.h"
#include "snow_shot/presentation/screenshotcapturemodetoolbarwidget.h"
#include "snow_shot/presentation/screenshotinteractionstate.h"
#include "snow_shot/presentation/screenshotoverlaywindow.h"
#include "snow_shot/presentation/screenshotoverlayinputhandler.h"
#include "snow_shot/presentation/screenshotselectionmodel.h"
#include "snow_shot/presentation/screenshotselectionparams.h"
#include "snow_shot/storage/applicationstorage.h"
#include "widgets/platform_compatibility.h"

#include <QApplication>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QToolButton>
#include <QTranslator>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

QToolButton* modeButton(ScreenshotCaptureModeToolbarWidget& toolbar, int index) {
    auto* button = toolbar.findChild<QToolButton*>(QStringLiteral("captureMode_%1").arg(index));
    require(button != nullptr, "all three mode buttons exist");
    return button;
}

void sendMouse(QWidget& target, QEvent::Type type, QPoint local, QPoint global,
               Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent event(type, QPointF(local), QPointF(global), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&target, &event);
}

class ModeTranslator final : public QTranslator {
  public:
    bool isEmpty() const override {
        return false;
    }
    QString translate(const char* context, const char* source, const char*, int) const override {
        if (QByteArray(context) == "ScreenshotCaptureModeToolbarWidget" &&
            QByteArray(source) == "Screenshot")
            return QStringLiteral("Translated screenshot");
        return {};
    }
};

void widgetBehavior() {
    QWidget owner;
    owner.resize(1000, 600);
    ScreenshotCaptureModeToolbarWidget toolbar(&owner);
    toolbar.move(100, 30);
    owner.show();
    toolbar.show();
    int requests = 0;
    ScreenshotCaptureToolbarMode requested = ScreenshotCaptureToolbarMode::Screenshot;
    QObject::connect(&toolbar, &ScreenshotCaptureModeToolbarWidget::modeRequested,
                     [&requests, &requested](ScreenshotCaptureToolbarMode mode) {
                         ++requests;
                         requested = mode;
                     });
    require(toolbar.height() == 40, "mode toolbar shares the QQ logical main toolbar height");
    modeButton(toolbar, 1)->click();
    require(requests == 1 && requested == ScreenshotCaptureToolbarMode::Recording,
            "recording tab emits a real recording request");
    require(modeButton(toolbar, 0)->isChecked() && !modeButton(toolbar, 1)->isChecked(),
            "a request alone cannot override controller mode state");
    toolbar.setMode(ScreenshotCaptureToolbarMode::Scrolling);
    require(requests == 1 && modeButton(toolbar, 2)->isChecked(),
            "controller synchronization selects one tab without another request");
    toolbar.setEnabled(false);
    modeButton(toolbar, 0)->click();
    require(requests == 1, "disabled mode toolbar cannot request a switch");
    toolbar.setEnabled(true);
    auto* grip = toolbar.findChild<QWidget*>(QStringLiteral("captureModeDragHandle"));
    require(grip != nullptr, "mode toolbar has a drag region");
    const QPoint before = toolbar.pos();
    const QPoint local = grip->rect().center();
    const QPoint start = grip->mapToGlobal(local);
    sendMouse(*grip, QEvent::MouseButtonPress, local, start, Qt::LeftButton, Qt::LeftButton);
    sendMouse(*grip, QEvent::MouseMove, local, start + QPoint(35, 25), Qt::NoButton,
              Qt::LeftButton);
    sendMouse(*grip, QEvent::MouseButtonRelease, local, start + QPoint(35, 25), Qt::LeftButton,
              Qt::NoButton);
    require(toolbar.pos() == before + QPoint(35, 25) && requests == 1,
            "grip moves the toolbar without changing capture mode");
    ModeTranslator translator;
    QCoreApplication::installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&toolbar, &languageChange);
    require(modeButton(toolbar, 0)->text() == QStringLiteral("Translated screenshot"),
            "visible mode labels retranslate on LanguageChange");
    QCoreApplication::removeTranslator(&translator);
}

void controllerRouting() {
    using Action = ScreenshotController::CaptureAction;
    ScreenshotController controller;
    auto* overlay = ScreenshotControllerTestAccess::prepareCaptureModes(controller, Action::None);
    auto* toolbar = overlay->findChild<ScreenshotCaptureModeToolbarWidget*>();
    require(toolbar && toolbar->isVisible() && toolbar->isEnabled(),
            "unselected screenshot exposes the enabled mode toolbar");
    const quint64 session = ScreenshotControllerTestAccess::captureSessionId(controller);
    modeButton(*toolbar, 1)->click();
    require(ScreenshotControllerTestAccess::pendingCaptureAction(controller) ==
                    Action::StartVideo &&
                toolbar->mode() == ScreenshotCaptureToolbarMode::Recording,
            "record tab routes the current selection to the existing recording action");
    modeButton(*toolbar, 2)->click();
    require(ScreenshotControllerTestAccess::pendingCaptureAction(controller) ==
                Action::StartScrolling,
            "scroll tab routes the current selection to the existing scrolling action");
    modeButton(*toolbar, 0)->click();
    require(ScreenshotControllerTestAccess::pendingCaptureAction(controller) == Action::None &&
                ScreenshotControllerTestAccess::captureSessionId(controller) == session,
            "switching tabs retains the captured session and restores normal screenshot editing");

    auto& interaction = ScreenshotControllerTestAccess::interaction(controller);
    require(interaction.enterSelectionDrag(ScreenshotSelectionDragMode::Marquee),
            "fixture begins a real selection drag");
    // An already queued signal must also be rejected before presentation disables the button.
    toolbar->modeRequested(ScreenshotCaptureToolbarMode::Recording);
    require(ScreenshotControllerTestAccess::pendingCaptureAction(controller) == Action::None,
            "controller rejects stale mode requests during a drag");
    ScreenshotControllerTestAccess::refreshCaptureModes(controller);
    require(!toolbar->isEnabled(), "selection drag disables mode buttons");
    interaction.cancelDrag();
    ScreenshotControllerTestAccess::refreshCaptureModes(controller);
    require(toolbar->isEnabled(), "cancelled drag restores mode switching");
    interaction.confirmSelection();
    ScreenshotControllerTestAccess::refreshCaptureModes(controller);
    require(!toolbar->isVisible(), "confirmed selection hides the preselection mode toolbar");
    toolbar->modeRequested(ScreenshotCaptureToolbarMode::Scrolling);
    require(ScreenshotControllerTestAccess::pendingCaptureAction(controller) == Action::None,
            "editing cannot mutate a pending mode through a stale request");
}

void selectionConfirmationDispatchesChosenMode() {
    using Action = ScreenshotController::CaptureAction;
    for (const auto action : {Action::StartVideo, Action::StartScrolling}) {
        ScreenshotController controller;
        auto* overlay =
            ScreenshotControllerTestAccess::prepareCaptureModes(controller, Action::None);
        auto* toolbar = overlay->findChild<ScreenshotCaptureModeToolbarWidget*>();
        require(toolbar, "confirmation fixture has the real mode toolbar");
        int handoffs = 0;
        Action dispatched = Action::None;
        ScreenshotControllerTestAccess::interceptCaptureModeHandoff(controller,
                                                                    [&](Action requested) {
                                                                        ++handoffs;
                                                                        dispatched = requested;
                                                                    });
        modeButton(*toolbar, action == Action::StartVideo ? 1 : 2)->click();
        ScreenshotSelectionParams params;
        params.selection = QRect(50, 80, 300, 200);
        require(ScreenshotControllerTestAccess::selection(controller)
                    .applyParams(params, QRect(0, 0, 800, 600)),
                "confirmation fixture has a valid selection");
        ScreenshotControllerTestAccess::overlayInput(controller).confirmSelection();
        require(handoffs == 1 && dispatched == action && !toolbar->isVisible() &&
                    ScreenshotControllerTestAccess::pendingCaptureAction(controller) ==
                        Action::None,
                "real selection confirmation reaches the chosen handoff and consumes its action");
        ScreenshotControllerTestAccess::overlayInput(controller).confirmSelection();
        require(handoffs == 1, "repeated confirmation cannot dispatch the consumed mode twice");
    }
}

void directRecordingCanReturnToScreenshot() {
    ScreenshotController controller;
    auto* overlay = ScreenshotControllerTestAccess::prepareCaptureModes(
        controller, ScreenshotController::CaptureAction::StartVideo);
    auto* toolbar = overlay->findChild<ScreenshotCaptureModeToolbarWidget*>();
    require(toolbar && modeButton(*toolbar, 1)->isChecked() &&
                ScreenshotControllerTestAccess::captureToolbarSuppressed(controller),
            "direct recording starts with its mode selected and editing toolbar suppressed");
    modeButton(*toolbar, 0)->click();
    require(!ScreenshotControllerTestAccess::captureToolbarSuppressed(controller),
            "the screenshot tab restores editing toolbar presentation after direct recording");
}

void specializedCapturesDoNotExposeModes() {
    using Action = ScreenshotController::CaptureAction;
    for (const auto action : {Action::Pin, Action::RecognizeText}) {
        ScreenshotController controller;
        auto* overlay = ScreenshotControllerTestAccess::prepareCaptureModes(controller, action);
        require(!overlay->findChild<ScreenshotCaptureModeToolbarWidget*>(),
                "specialized quick actions preserve their destination without a mode toolbar");
    }
    ScreenshotController controller;
    auto* overlay =
        ScreenshotControllerTestAccess::prepareCaptureModes(controller, Action::None, true);
    require(!overlay->findChild<ScreenshotCaptureModeToolbarWidget*>(),
            "silent capture never creates the mode toolbar");
}
} // namespace

int runQqCaptureModeTests(int argc, char** argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication application(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    adqt::widgets::initializePlatformCompatibility(application);
    QTemporaryDir directory;
    auto& storage = snow_shot::storage::ApplicationStorage::instance();
    snow_shot::storage::StorageInitializationOptions options{directory.path(), directory.path(),
                                                             60000};
    options.isolateAuxiliaryStorage = true;
    require(directory.isValid() && storage.initialize(options).success,
            "capture mode tests require isolated storage");
    widgetBehavior();
    controllerRouting();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    selectionConfirmationDispatchesChosenMode();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    directRecordingCanReturnToScreenshot();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    specializedCapturesDoNotExposeModes();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    storage.shutdown();
    return EXIT_SUCCESS;
}
