#include "snow_shot/presentation/startupsetupdialog.h"
#include "snow_shot/storage/configurationstore.h"
#include "widgets/checkbox.h"
#include "widgets/modal.h"

#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace snow_shot::presentation {
namespace {
const QString kSetupKey = QStringLiteral("system/startup_setup");
QString setupText(const char* text) {
    return QCoreApplication::translate("StartupSetup", text);
}
} // namespace

bool startupSetupLaunchAllowed(const QStringList& arguments) {
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        if (i == 0 && !arguments.at(i).startsWith(QLatin1Char('-')))
            continue;
        if (arguments.at(i) != QStringLiteral("--show-main-window"))
            return false;
    }
    return true;
}

bool startupSetupCompleted(const storage::ConfigurationStore& configuration) {
    return configuration.value(kSetupKey).toString() == QStringLiteral("completed");
}

StartupSetupDialog::StartupSetupDialog(storage::ConfigurationStore& configuration, Apply apply,
                                       QWidget* owner)
    : QObject(owner), m_configuration(&configuration), m_apply(std::move(apply)),
      m_modal(new adqt::widgets::AdModal(this)) {
    using adqt::widgets::AdModal;
    m_modal->setObjectName(QStringLiteral("startupSetupDialog"));
    m_modal->setMode(AdModal::Mode::Window);
    m_modal->setOwnerWindow(owner);
    m_modal->setWindowModality(Qt::WindowModal);
    m_modal->setPreferredWidth(440);
    m_modal->setCentered(true);
    m_modal->setCloseOnMaskClick(false);
    m_modal->setClosePolicy(AdModal::ClosePolicy::Manual);
    m_modal->setStandardButtons(AdModal::StandardButton::Ok | AdModal::StandardButton::Cancel);
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    m_description = new QLabel(content);
    m_description->setWordWrap(true);
    layout->addWidget(m_description);
    m_enabled = new adqt::widgets::AdCheckbox(content);
    m_enabled->setObjectName(QStringLiteral("startupSetupEnabled"));
    m_enabled->setChecked(configuration.value(kSetupKey).toString() !=
                          QStringLiteral("pending_disabled"));
    layout->addWidget(m_enabled);
    m_error = new QLabel(content);
    m_error->setObjectName(QStringLiteral("startupSetupError"));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);
    m_modal->setContentWidget(content);
    content->installEventFilter(this);
    retranslate();
    connect(m_modal, &AdModal::closeRequested, this, [this](AdModal::CloseReason reason) {
        if (m_busy)
            return;
        if (reason == AdModal::CloseReason::OkAction)
            submit();
        else
            m_modal->reject();
    });
    connect(m_modal, &AdModal::finished, this, &StartupSetupDialog::finished);
}

void StartupSetupDialog::open() {
    m_modal->open();
}

bool StartupSetupDialog::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    return QObject::eventFilter(watched, event);
}

void StartupSetupDialog::retranslate() {
    m_modal->setWindowTitle(setupText(QT_TRANSLATE_NOOP("StartupSetup", "Startup preference")));
    m_description->setText(setupText(
        QT_TRANSLATE_NOOP("StartupSetup", "Keep the screenshot shortcut ready after signing in. "
                                          "You can change this later in Settings.")));
#ifdef Q_OS_MACOS
    m_enabled->setText(setupText(QT_TRANSLATE_NOOP("StartupSetup", "Launch at login")));
#else
    m_enabled->setText(setupText(QT_TRANSLATE_NOOP("StartupSetup", "Start with Windows")));
#endif
    m_modal->setAcceptText(setupText(QT_TRANSLATE_NOOP("StartupSetup", "Save and continue")));
    m_modal->setRejectText(setupText(QT_TRANSLATE_NOOP("StartupSetup", "Not now")));
}

void StartupSetupDialog::setBusy(bool busy) {
    m_busy = busy;
    m_enabled->setEnabled(!busy);
    m_modal->setAcceptButtonBusy(busy);
    m_modal->setCloseButtonVisible(!busy);
    m_modal->setCloseOnEscape(!busy);
    if (auto* reject = m_modal->rejectButton())
        reject->setEnabled(!busy);
}

void StartupSetupDialog::submit() {
    if (!m_configuration || !m_configuration->isWritable()) {
        m_error->setText(setupText(QT_TRANSLATE_NOOP(
            "StartupSetup",
            "Settings could not be saved. Check the storage location and try again.")));
        m_error->show();
        return;
    }
    setBusy(true);
    m_error->hide();
    m_apply(m_enabled->isChecked(),
            [guard = QPointer<StartupSetupDialog>(this)](bool success, const QString& error) {
                if (!guard)
                    return;
                auto* configuration = guard->m_configuration.data();
                if (success && configuration) {
                    const auto previous = configuration->value(kSetupKey);
                    success = configuration->setValue(kSetupKey, QStringLiteral("completed")) &&
                              configuration->flushNow().success;
                    if (!success)
                        configuration->setValue(kSetupKey, previous);
                } else if (!configuration) {
                    success = false;
                }
                guard->setBusy(false);
                if (success) {
                    guard->m_modal->accept();
                } else {
                    guard->m_error->setText(
                        error.isEmpty()
                            ? setupText(QT_TRANSLATE_NOOP("StartupSetup",
                                                          "Settings could not be saved. Check "
                                                          "the storage location and try again."))
                            : error);
                    guard->m_error->show();
                }
            });
}
} // namespace snow_shot::presentation
