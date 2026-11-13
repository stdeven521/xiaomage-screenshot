#ifndef SNOW_SHOT_PRESENTATION_STARTUPSETUPDIALOG_H
#define SNOW_SHOT_PRESENTATION_STARTUPSETUPDIALOG_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <functional>

class QLabel;
class QWidget;
namespace adqt::widgets {
class AdCheckbox;
class AdModal;
} // namespace adqt::widgets
namespace snow_shot::storage {
class ConfigurationStore;
}
namespace snow_shot::presentation {
// Only ordinary foreground launches offer setup; probes and background launches stay silent.
bool startupSetupLaunchAllowed(const QStringList& arguments);
bool startupSetupCompleted(const storage::ConfigurationStore& configuration);

class StartupSetupDialog final : public QObject {
    Q_OBJECT
  public:
    using Completion = std::function<void(bool, const QString&)>;
    using Apply = std::function<void(bool, Completion)>;
    StartupSetupDialog(storage::ConfigurationStore& configuration, Apply apply, QWidget* owner);
    void open();

  signals:
    void finished();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void retranslate();
    void submit();
    void setBusy(bool busy);
    QPointer<storage::ConfigurationStore> m_configuration;
    Apply m_apply;
    adqt::widgets::AdModal* m_modal;
    adqt::widgets::AdCheckbox* m_enabled;
    QLabel* m_description;
    QLabel* m_error;
    bool m_busy = false;
};
} // namespace snow_shot::presentation
#endif
