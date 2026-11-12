#include "snow_shot/presentation/globalshortcutmanager.h"
#include "snow_shot/presentation/settings/settingsbackend.h"
#include "snow_shot/presentation/settings/settingsruntimesession.h"
#include "snow_shot/presentation/settings/settingssearchindex.h"
#include "snow_shot/presentation/components/settingspagewidget.h"
#include "snow_shot/presentation/settings/settingsformfield.h"
#include "snow_shot/network/snowshotapiclient.h"
#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/storage/configurationarchive.h"
#include "snow_shot/storage/configurationschema.h"
#include "snow_shot/storage/settingsadapters.h"

#include <QApplication>
#include <QLineEdit>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

namespace settings = snow_shot::presentation::settings;
namespace storage = snow_shot::storage;

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void serverSettings(const QTemporaryDir& temporary) {
    const QString key = QStringLiteral("api_configuration/server_url");
    const QString fieldId = QStringLiteral("api.server-url");
    auto& configuration = storage::ApplicationStorage::instance().configuration();
    snow_shot::presentation::GlobalShortcutManager shortcuts;
    settings::BuiltInSettingsBackend backend(shortcuts);
    const auto& registry = settings::builtInSettingsRegistry();
    settings::SettingsRuntimeSession session(registry, backend);
    require(registry.isValid() &&
                registry.fieldForText(settings::SettingsTextBinding::ServerUrl) == nullptr &&
                registry.catalog().page(QStringLiteral("account")) == nullptr &&
                registry.catalog().section(QStringLiteral("connections-services"),
                                           QStringLiteral("snow-shot-server")) == nullptr,
            "personal settings do not expose accounts or the official server");
    require(
        registry.catalog().item({QStringLiteral("connections-services"), QStringLiteral("ai-model"),
                                 QStringLiteral("api.custom-models")}) != nullptr &&
            registry.catalog().item({QStringLiteral("connections-services"),
                                     QStringLiteral("text-translation"),
                                     QStringLiteral("api.text-translation")}) != nullptr,
        "personal settings retain custom AI and translation configuration");
    const settings::SettingsSearchIndex search(registry);
    for (const auto& entry : search.search(QStringLiteral("Server address")))
        require(entry.location.itemId != fieldId,
                "removed official server is not offered by search");

    SettingsPageWidget page(registry, QStringLiteral("connections-services"), session);
    page.resize(1000, 800);
    page.show();
    page.reveal({page.pageId(), QStringLiteral("snow-shot-server"), fieldId});
    QApplication::processEvents();
    for (auto* candidate : page.findChildren<QObject*>()) {
        if (auto* field = dynamic_cast<settings::SettingsFormField*>(candidate))
            require(field->descriptor().id != fieldId,
                    "a stale server route cannot recreate its removed editor");
    }

    // Preserve imported data without restoring the account workflow.
    const QString accepted = QStringLiteral("http://localhost:8089/prefix");
    require(configuration.setValue(key, accepted) && configuration.flushNow().success,
            "legacy server data remains compatible with configuration files");
    storage::ConfigurationStore reloaded(temporary.filePath(QStringLiteral("data/config.json")),
                                         true, true, 60000);
    require(reloaded.value(key).toString() == accepted,
            "unrelated settings edits do not destroy the old server preference");
    const QString archive = temporary.filePath(QStringLiteral("server.zip"));
    require(storage::ConfigurationArchive::write(
                archive, {{key, accepted}}, storage::ConfigurationStore::currentSchemaVersion())
                .isEmpty(),
            "legacy server data can still be exported");
    const auto imported = storage::ConfigurationArchive::read(archive);
    require(imported.isValid() && imported.values.value(key).toString() == accepted,
            "legacy server data survives archive round trip");
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "isolated settings directory");
    auto& applicationStorage = storage::ApplicationStorage::instance();
    static_cast<void>(
        applicationStorage.initialize({temporary.filePath(QStringLiteral("bin")),
                                       temporary.filePath(QStringLiteral("data")), 60000}));
    serverSettings(temporary);
    applicationStorage.shutdown();
    return 0;
}
