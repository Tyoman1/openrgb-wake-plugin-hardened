#pragma once

#include <string>

#include "OpenRGBPluginInterface.h"
#include "ResourceManagerCallback.h"

class QMenu;
class QWidget;

class MouseActivityWatcher;
class PowerWatcher;
class SettingsWidget;

class WakePlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "OpenRGBWakePlugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    WakePlugin();
    ~WakePlugin() override;

    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int      GetPluginAPIVersion() override;

    void           Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*       GetWidget() override;
    QMenu*         GetTrayMenu() override;
    void           Unload() override;
    void           OnProfileAboutToLoad() override;
    void           OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json OnProfileSave() override;
    unsigned char* OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;
    void           ProfileManagerUpdated(unsigned int update_reason) override;
    void           ResourceManagerUpdated(unsigned int update_reason) override;
    void           SettingsManagerUpdated(unsigned int update_reason) override;

    void OnPowerResume();
    void SettingsChanged();

private:
    bool        ApplyTargets(const char* reason, bool resume_trigger = false);
    std::string ResolveProfileName(bool resume_trigger);
    void        ApplyStartup();
    void        ScheduleDetectionRestore(int delay_ms, const char* reason);
    void        OnDetectionRestore(const char* reason);
    void        LoadSettings();
    void        SaveSettings();
    void        Log(const char* msg, unsigned int level = 2);

    OpenRGBPluginAPIInterface* api_           = nullptr;
    SettingsWidget*            widget_        = nullptr;
    PowerWatcher*              power_watcher_ = nullptr;
    MouseActivityWatcher*      mouse_watcher_ = nullptr;

    bool reapply_on_wake_         = true;
    bool activity_detect_enabled_ = true;
    int  idle_resume_sec_         = 60;
    bool log_enabled_             = true;

    bool last_restore_ok_      = true;

    /* Startup window: the plugin actively tries to restore profile
       during the first ~30 seconds after Load(), even if LoadProfile
       returns true early (the device may not be ready yet). */
    bool startup_restore_pending_    = true;
    bool startup_restore_scheduled_  = false;
    int  startup_retries_left_       = 5;
};