#include "snow_shot/presentation/startupsetupdialog.h"
#include "snow_shot/storage/configurationstore.h"
#include "widgets/checkbox.h"
#include "widgets/modal.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTranslator>
#include <QWidget>
#include <cstdio>
#include <stdexcept>

using namespace snow_shot;
namespace {
const QString kSetupKey = QStringLiteral("system/startup_setup");
const QString kAutoStartKey = QStringLiteral("system/auto_start_at_boot");
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void settle() {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 100) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
}
adqt::widgets::AdModal* modal(presentation::StartupSetupDialog& dialog) {
    auto* result = dialog.findChild<adqt::widgets::AdModal*>();
    require(result != nullptr, "setup exposes its modal");
    return result;
}
adqt::widgets::AdCheckbox* checkbox(presentation::StartupSetupDialog& dialog) {
    auto* result = modal(dialog)->contentWidget()->findChild<adqt::widgets::AdCheckbox*>();
    require(result != nullptr, "setup exposes its startup choice");
    return result;
}
void writeLegacy(const QString& path, bool enabled) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "legacy fixture is writable");
    const QJsonObject document{
        {QStringLiteral("storage"),
         QJsonObject{{QStringLiteral("schema_version"),
                      storage::ConfigurationStore::currentSchemaVersion()}}},
        {QStringLiteral("system"), QJsonObject{{QStringLiteral("auto_start_at_boot"), enabled}}}};
    require(file.write(QJsonDocument(document).toJson()) > 0, "legacy configuration is written");
}
void launchAdmission() {
    using presentation::startupSetupLaunchAllowed;
    require(startupSetupLaunchAllowed({QStringLiteral("snow_shot")}) &&
                startupSetupLaunchAllowed(
                    {QStringLiteral("snow_shot"), QStringLiteral("--show-main-window")}),
            "ordinary foreground launches offer setup");
    for (const auto& flag :
         {"--autostart", "--startup-probe", "--distribution-probe", "--self-test", "--e2e-ui",
          "--silent", "--background", "--administrator-helper", "--update-probe", "image.png"}) {
        require(
            !startupSetupLaunchAllowed({QStringLiteral("snow_shot"), QString::fromLatin1(flag)}),
            "background, file and diagnostic launches do not offer setup");
    }
    require(!startupSetupLaunchAllowed({QStringLiteral("--autostart")}),
            "argument-only background requests remain silent");
}
void initialChoicesAndMigration() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "temporary config directory");
    for (const auto profile :
         {storage::ConfigurationDefaults::Standard, storage::ConfigurationDefaults::Portable}) {
        const QString path = temporary.filePath(profile == storage::ConfigurationDefaults::Portable
                                                    ? QStringLiteral("portable.json")
                                                    : QStringLiteral("standard.json"));
        {
            storage::ConfigurationStore config(path, true, true, 60000, nullptr, profile);
            require(config.value(kSetupKey).toString() == QStringLiteral("pending_enabled"),
                    "new standard and Mac portable profiles default the choice to enabled");
            QWidget owner;
            presentation::StartupSetupDialog dialog(config, [](bool, auto) {}, &owner);
            require(checkbox(dialog)->isChecked(), "fresh first-run checkbox is checked");
            require(config.flushNow().success, "pending choice persists before confirmation");
        }
        storage::ConfigurationStore reopened(path, true, true, 60000, nullptr, profile);
        require(reopened.value(kSetupKey).toString() == QStringLiteral("pending_enabled"),
                "restarting before setup does not reinterpret portable defaults as an opt-out");
    }
    for (const bool enabled : {false, true}) {
        const QString path = temporary.filePath(enabled ? QStringLiteral("old-on.json")
                                                        : QStringLiteral("old-off.json"));
        writeLegacy(path, enabled);
        storage::ConfigurationStore config(path, true, true, 60000);
        QWidget owner;
        presentation::StartupSetupDialog dialog(config, [](bool, auto) {}, &owner);
        require(!presentation::startupSetupCompleted(config) &&
                    checkbox(dialog)->isChecked() == enabled &&
                    config.value(kAutoStartKey).toBool() == enabled,
                "legacy settings prompt once without changing an explicit previous preference");
        require(config.flushNow().success, "legacy setup state persists");
        storage::ConfigurationStore reopened(path, true, true, 60000);
        require(reopened.value(kSetupKey) == config.value(kSetupKey),
                "legacy choice survives reload");
    }
}
void cancelAndAsyncRetry() {
    QTemporaryDir temporary;
    storage::ConfigurationStore config(temporary.filePath(QStringLiteral("config.json")), true,
                                       true, 60000);
    QWidget owner;
    owner.resize(600, 400);
    owner.show();
    int registrations = 0;
    presentation::StartupSetupDialog::Completion complete;
    bool requested = true;
    const auto apply = [&](bool enabled, presentation::StartupSetupDialog::Completion done) {
        ++registrations;
        requested = enabled;
        complete = std::move(done);
    };
    {
        presentation::StartupSetupDialog deferred(config, apply, &owner);
        deferred.open();
        settle();
        modal(deferred)->rejectButton()->click();
        settle();
        require(registrations == 0 && !presentation::startupSetupCompleted(config),
                "Not now neither registers startup nor marks setup complete");
    }
    presentation::StartupSetupDialog dialog(config, apply, &owner);
    dialog.open();
    settle();
    checkbox(dialog)->setChecked(false);
    modal(dialog)->acceptButton()->click();
    require(registrations == 1 && !requested && !checkbox(dialog)->isEnabled() &&
                !presentation::startupSetupCompleted(config),
            "unchecked choice waits for native completion without premature confirmation");
    complete(false, QStringLiteral("registration failed"));
    auto* error =
        modal(dialog)->contentWidget()->findChild<QLabel*>(QStringLiteral("startupSetupError"));
    require(error && error->text() == QStringLiteral("registration failed") &&
                checkbox(dialog)->isEnabled() && !presentation::startupSetupCompleted(config),
            "native failure is visible and allows retry without confirming setup");
    modal(dialog)->acceptButton()->click();
    require(registrations == 2 && !requested, "retry preserves the user's opt-out");
    require(config.setValue(kAutoStartKey, requested) && config.flushNow().success,
            "fake registration service saves its applied choice");
    complete(true, {});
    require(presentation::startupSetupCompleted(config), "native success confirms setup");
    storage::ConfigurationStore reopened(temporary.filePath(QStringLiteral("config.json")), true,
                                         true, 60000);
    require(presentation::startupSetupCompleted(reopened) &&
                !reopened.value(kAutoStartKey).toBool(),
            "saved opt-out is confirmed durably and will not prompt again");
}
void persistenceFailureDoesNotConfirm() {
    QTemporaryDir temporary;
    const QString path = temporary.filePath(QStringLiteral("config.json"));
    storage::ConfigurationStore config(path, true, true, 60000);
    require(config.flushNow().success, "save initial configuration");
    QWidget owner;
    owner.show();
    presentation::StartupSetupDialog::Completion complete;
    presentation::StartupSetupDialog dialog(
        config, [&](bool, auto done) { complete = std::move(done); }, &owner);
    dialog.open();
    settle();
    modal(dialog)->acceptButton()->click();
    require(QFile::remove(path) && QDir().mkdir(path), "make config destination unwritable");
    complete(true, {});
    require(!presentation::startupSetupCompleted(config) && checkbox(dialog)->isEnabled(),
            "failed completion persistence restores pending state and permits retry");
    require(QDir().rmdir(path), "restore configuration destination");
    modal(dialog)->acceptButton()->click();
    complete(true, {});
    require(presentation::startupSetupCompleted(config), "repaired storage permits confirmation");
}
void readOnlyStorageDoesNotRegister() {
    QTemporaryDir temporary;
    storage::ConfigurationStore config(temporary.filePath(QStringLiteral("config.json")), true,
                                       false, 60000);
    QWidget owner;
    owner.show();
    bool applied = false;
    presentation::StartupSetupDialog dialog(config, [&](bool, auto) { applied = true; }, &owner);
    dialog.open();
    settle();
    modal(dialog)->acceptButton()->click();
    require(!applied && !presentation::startupSetupCompleted(config),
            "read-only storage must not change the native registration");
    auto* error =
        modal(dialog)->contentWidget()->findChild<QLabel*>(QStringLiteral("startupSetupError"));
    require(error && !error->isHidden() && !error->text().isEmpty(),
            "read-only error explains why setup remains pending");
}
void savePreview(const QString& path, const QString& catalog) {
    const auto fontPath = qEnvironmentVariable("SNOW_TEST_PREVIEW_FONT");
    if (!fontPath.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        const auto families = QFontDatabase::applicationFontFamilies(id);
        require(!families.isEmpty(), "load preview font for the offscreen platform");
        QApplication::setFont(QFont(families.first(), 10));
    }
    QTranslator translator;
    require(translator.load(catalog), "load translated preview catalog");
    QCoreApplication::installTranslator(&translator);
    QTemporaryDir temporary;
    storage::ConfigurationStore config(temporary.filePath(QStringLiteral("config.json")), true,
                                       true, 60000);
    QWidget owner;
    owner.resize(640, 440);
    owner.show();
    presentation::StartupSetupDialog dialog(config, [](bool, auto) {}, &owner);
    dialog.open();
    settle();
    require(modal(dialog)->contentWidget()->window()->grab().save(path), "save preview image");
    QCoreApplication::removeTranslator(&translator);
}
} // namespace
int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        if (argc == 4 && QByteArray(argv[1]) == "--screenshot") {
            savePreview(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
            return 0;
        }
        launchAdmission();
        initialChoicesAndMigration();
        cancelAndAsyncRetry();
        persistenceFailureDoesNotConfirm();
        readOnlyStorageDoesNotRegister();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    return 0;
}
