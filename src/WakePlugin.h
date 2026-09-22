#pragma once

#include <string>

#include "OpenRGBPluginInterface.h"
#include "ResourceManagerCallback.h"
#include "RGBControllerInterface.h"

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

    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;

    void                Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override;
    void                Unload() override;

    void                OnProfileAboutToLoad() override;
    void                OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json      OnProfileSave() override;
    unsigned char*      OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;

    void                ProfileManagerUpdated(unsigned int update_reason) override;
    void                ResourceManagerUpdated(unsigned int update_reason) override;
    void                SettingsManagerUpdated(unsigned int update_reason) override;

    /* Called by PowerWatcher when the PC resumes from sleep */
    void                OnPowerResume();

    /* Called by SettingsWidget */
    void                SettingsChanged();

private:
    bool                MatchesTarget(RGBControllerInterface* ctrl);
    bool                ApplyTargets(const char* reason);
    void                ApplyStartup();
    void                LoadSettings();
    void                SaveSettings();
    void                Log(const char* msg, unsigned int level = 2);

    OpenRGBPluginAPIInterface*  api_                     = nullptr;
    SettingsWidget*             widget_                  = nullptr;
    PowerWatcher*               power_watcher_           = nullptr;
    MouseActivityWatcher*       mouse_watcher_           = nullptr;

    std::string                 target_key_              = "DeathAdder";
    bool                        reapply_on_wake_         = true;
    bool                        activity_detect_enabled_ = true;
    int                         idle_resume_sec_         = 60;
    bool                        log_enabled_             = true;

    /* True when the last re-apply reached at least one target device */
    bool                        last_restore_ok_         = true;
    /* Remaining retries when the mouse has not been detected yet at startup */
    int                         startup_retries_left_    = 5;
};