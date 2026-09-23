/*---------------------------------------------------------*\
| OpenRGB Wake Plugin                                       |
|                                                           |
|  Restores device lighting when a wireless device wakes    |
|  up, the PC resumes, or a fresh session starts.           |
|                                                           |
|  On each trigger the plugin asks OpenRGB to load the      |
|  profile the user configured in OpenRGB's own Profile     |
|  Manager settings (open/resume/service-startup profile).  |
|  The plugin stores no colors and no profile names itself: |
|  the profile name is read live from OpenRGB settings.     |
|  If no auto-load profile is configured, the plugin falls  |
|  back to re-sending the currently active lighting state.  |
|                                                           |
|  This file is part of the openrgb-wake-plugin project     |
\*---------------------------------------------------------*/

#include "WakePlugin.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include <QImage>
#include <QString>
#include <QTimer>

#include "MouseActivityWatcher.h"
#include "PowerWatcher.h"
#include "SettingsWidget.h"

/* Local mirror of the OpenRGB LogManager levels */
enum
{
    LOG_LEVEL_ERROR   = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_INFO    = 2,
};

WakePlugin::WakePlugin()
{
}

WakePlugin::~WakePlugin()
{
}

/*---------------------------------------------------------*\
| Plugin Information                                        |
\*---------------------------------------------------------*/
OpenRGBPluginInfo WakePlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name            = "Wake Plugin";
    info.Description     = "Loads the configured OpenRGB profile when a wireless device wakes or the PC resumes";
    info.Version         = "1.3.0";
    info.Commit          = "";
    info.URL             = "https://github.com/Tyoman1/openrgb-wake-plugin";
    info.Icon            = QImage();
    info.Location        = OPENRGB_PLUGIN_LOCATION_SETTINGS;
    info.Label           = "Wake Plugin";
    info.TabIconString   = "";
    info.TabIcon         = QImage();
    info.ProtocolVersion = 1;

    return info;
}

unsigned int WakePlugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

/*---------------------------------------------------------*\
| Plugin Lifecycle                                          |
\*---------------------------------------------------------*/
void WakePlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api_ = plugin_api_ptr;

    LoadSettings();

    SettingsWidget::Settings widget_settings;
    widget_settings.target_key               = QString::fromStdString(target_key_);
    widget_settings.reapply_on_wake          = reapply_on_wake_;
    widget_settings.activity_detect_enabled  = activity_detect_enabled_;
    widget_settings.idle_resume_sec          = idle_resume_sec_;
    widget_settings.log_enabled              = log_enabled_;

    widget_ = new SettingsWidget(widget_settings);
    connect(widget_, &SettingsWidget::settingsChanged, this, &WakePlugin::SettingsChanged);

    /* Watch for the PC resuming from sleep */
    power_watcher_ = new PowerWatcher(this);
    power_watcher_->on_resume = [this]()
    {
        OnPowerResume();
    };

    /* Instant restore when the mouse wakes up: the first mouse event after
       a long quiet gap means the user just picked the device up again */
    mouse_watcher_ = new MouseActivityWatcher(this);
    mouse_watcher_->setIdleThresholdMs(static_cast<quint64>(idle_resume_sec_) * 1000);
    connect(mouse_watcher_, &MouseActivityWatcher::mouseResumed, this, [this]()
    {
        /* Give the device a moment to finish waking up; a delayed second
           attempt covers the case where it was not ready yet. */
        QTimer::singleShot(700, this, [this]() { ApplyTargets("mouse activity"); });
        QTimer::singleShot(2600, this, [this]()
        {
            if (!last_restore_ok_)
            {
                ApplyTargets("mouse activity (retry)");
            }
        });
    });

    if (activity_detect_enabled_ && !mouse_watcher_->start())
    {
        Log("Warning: could not install mouse activity hook", LOG_LEVEL_WARNING);
    }

    /* Fresh session: re-send the active lighting shortly after startup.
       If the mouse has not been detected yet, ApplyStartup retries. */
    QTimer::singleShot(2000, this, [this]() { ApplyStartup(); });

    Log("Plugin loaded");
}

QWidget* WakePlugin::GetWidget()
{
    return widget_;
}

QMenu* WakePlugin::GetTrayMenu()
{
    return nullptr;
}

void WakePlugin::Unload()
{
    if (mouse_watcher_)
    {
        mouse_watcher_->stop();
        delete mouse_watcher_;
        mouse_watcher_ = nullptr;
    }

    delete power_watcher_;
    power_watcher_ = nullptr;

    /* OpenRGB owns the widget; it is deleted together with the tab */
    widget_ = nullptr;

    api_ = nullptr;
}

/*---------------------------------------------------------*\
| Profile / SDK Interface                                   |
\*---------------------------------------------------------*/
void WakePlugin::OnProfileAboutToLoad()
{
}

void WakePlugin::OnProfileLoad(nlohmann::json /* profile_data */)
{
    /* Loading profiles is OpenRGB's own job; nothing to do here */
}

nlohmann::json WakePlugin::OnProfileSave()
{
    return nlohmann::json::object();
}

unsigned char* WakePlugin::OnSDKCommand(unsigned int /* pkt_id */, unsigned char* /* pkt_data */, unsigned int* /* pkt_size */)
{
    return nullptr;
}

/*---------------------------------------------------------*\
| Update Signals                                            |
\*---------------------------------------------------------*/
void WakePlugin::ProfileManagerUpdated(unsigned int /* update_reason */)
{
}

void WakePlugin::ResourceManagerUpdated(unsigned int /* update_reason */)
{
    /* Device-list events are OpenRGB's concern: it applies the active profile
       to newly detected devices itself. Nothing to do here. */
}

void WakePlugin::SettingsManagerUpdated(unsigned int /* update_reason */)
{
}

/*---------------------------------------------------------*\
| Event Handlers                                            |
\*---------------------------------------------------------*/
void WakePlugin::OnPowerResume()
{
    if (reapply_on_wake_)
    {
        QTimer::singleShot(2500, this, [this]() { ApplyTargets("PC wake", true); });
    }
}

void WakePlugin::SettingsChanged()
{
    if (!widget_)
    {
        return;
    }

    SettingsWidget::Settings widget_settings = widget_->GetSettings();

    target_key_              = widget_settings.target_key.toStdString();
    reapply_on_wake_         = widget_settings.reapply_on_wake;
    activity_detect_enabled_ = widget_settings.activity_detect_enabled;
    idle_resume_sec_         = widget_settings.idle_resume_sec;
    log_enabled_             = widget_settings.log_enabled;

    /* Apply watcher changes live */
    if (mouse_watcher_)
    {
        mouse_watcher_->setIdleThresholdMs(static_cast<quint64>(idle_resume_sec_) * 1000);

        if (activity_detect_enabled_)
        {
            if (!mouse_watcher_->start())
            {
                Log("Warning: could not install mouse activity hook", LOG_LEVEL_WARNING);
            }
        }
        else
        {
            mouse_watcher_->stop();
        }
    }

    SaveSettings();

    Log("Settings saved");
}

/*---------------------------------------------------------*\
| Device Matching and Re-apply                              |
\*---------------------------------------------------------*/
bool WakePlugin::MatchesTarget(RGBControllerInterface* ctrl)
{
    if (!ctrl || target_key_.empty())
    {
        return false;
    }

    std::string haystack = ctrl->GetName() + " " + ctrl->GetDescription() + " " + ctrl->GetLocation();

    std::string key = target_key_;
    std::string hay = haystack;

    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return hay.find(key) != std::string::npos;
}

/*---------------------------------------------------------*\
| Profile Name Resolution                                   |
|                                                           |
|  Reads the profile name from OpenRGB's own ProfileManager |
|  settings: which profile to load is the user's choice in  |
|  OpenRGB, the plugin only follows it. Returns an empty    |
|  string when no auto-load profile is configured.          |
\*---------------------------------------------------------*/
std::string WakePlugin::ResolveProfileName(bool resume_trigger)
{
    if (!api_)
    {
        return "";
    }

    nlohmann::json pm_settings;

    try
    {
        pm_settings = api_->GetSettings("ProfileManager");
    }
    catch (...)
    {
        return "";
    }

    /* Trigger-appropriate key first, then fallbacks */
    const char* keys[3];

    if (resume_trigger)
    {
        keys[0] = "resume_profile";
        keys[1] = "open_profile";
        keys[2] = "service_startup_profile";
    }
    else
    {
        keys[0] = "open_profile";
        keys[1] = "service_startup_profile";
        keys[2] = "resume_profile";
    }

    for (const char* key : keys)
    {
        if (pm_settings.contains(key)
            && pm_settings[key].is_object()
            && pm_settings[key].value("enabled", false)
            && pm_settings[key].contains("name")
            && pm_settings[key]["name"].is_string())
        {
            std::string name = pm_settings[key]["name"].get<std::string>();

            if (!name.empty())
            {
                return name;
            }
        }
    }

    return "";
}

bool WakePlugin::ApplyTargets(const char* reason, bool resume_trigger)
{
    if (!api_)
    {
        last_restore_ok_ = false;
        return false;
    }

    /*-----------------------------------------------------*\
    | Preferred path: load the profile configured in        |
    | OpenRGB's Profile Manager. This pulls the real saved  |
    | state (not the possibly-stale live state) and lets    |
    | OpenRGB apply it to every device it matches.          |
    \*-----------------------------------------------------*/
    std::string profile_name = ResolveProfileName(resume_trigger);

    if (!profile_name.empty())
    {
        bool loaded = false;

        try
        {
            loaded = api_->LoadProfile(profile_name);
        }
        catch (...)
        {
            loaded = false;
        }

        last_restore_ok_ = loaded;

        char msg[192];
        std::snprintf(msg, sizeof(msg), "Loaded profile '%s' after %s (%s)", profile_name.c_str(), reason, loaded ? "done" : "failed");
        Log(msg);

        if (widget_)
        {
            widget_->SetStatusText(loaded
                ? QString("Загружен профиль «%1» (%2)").arg(QString::fromStdString(profile_name), reason)
                : "Не удалось загрузить профиль");
        }

        return loaded;
    }

    /*-----------------------------------------------------*\
    | Fallback path: no auto-load profile is configured in  |
    | OpenRGB, so re-send the currently active lighting of  |
    | the matched target device instead.                    |
    \*-----------------------------------------------------*/
    bool applied = false;

    for (RGBControllerInterface* ctrl : api_->GetRGBControllers())
    {
        if (!MatchesTarget(ctrl))
        {
            continue;
        }

        if (ctrl->SupportsPerZoneModes())
        {
            /* Per-zone devices: re-send the current mode and LED state of
               each zone (no values are changed — the controller already holds
               the active lighting). */
            unsigned int zone_count = ctrl->GetZoneCount();
            for (unsigned int zi = 0; zi < zone_count; zi++)
            {
                ctrl->UpdateZoneMode(static_cast<int>(zi));
                ctrl->UpdateZoneLEDs(static_cast<int>(zi));
            }
        }
        else
        {
            /* Global-mode devices: re-select the current mode and re-send
               the current mode settings and LED colors. */
            int active_mode = ctrl->GetActiveMode();
            if ((active_mode >= 0) && (static_cast<unsigned int>(active_mode) < ctrl->GetModeCount()))
            {
                ctrl->SetActiveMode(active_mode);
            }

            ctrl->UpdateMode();
            ctrl->UpdateLEDs();
        }

        applied = true;
    }

    last_restore_ok_ = applied;

    char msg[160];
    std::snprintf(msg, sizeof(msg), "Re-applied lighting after %s (%s)", reason, applied ? "done" : "no target");
    Log(msg);

    if (widget_)
    {
        widget_->SetStatusText(applied
            ? QString("Подсветка отправлена в мышь (%1)").arg(reason)
            : "Целевое устройство не найдено");
    }

    return applied;
}

/*---------------------------------------------------------*\
| Startup Retry                                             |
\*---------------------------------------------------------*/
void WakePlugin::ApplyStartup()
{
    if (ApplyTargets("startup"))
    {
        return;
    }

    if (startup_retries_left_ > 0)
    {
        startup_retries_left_--;
        QTimer::singleShot(1000, this, [this]() { ApplyStartup(); });
    }
}

/*---------------------------------------------------------*\
| Settings                                                  |
\*---------------------------------------------------------*/
void WakePlugin::LoadSettings()
{
    if (!api_)
    {
        return;
    }

    nlohmann::json settings = api_->GetSettings("OpenRGBWakePlugin");

    if (settings.contains("target_key"))
    {
        target_key_ = settings["target_key"].get<std::string>();
    }
    if (settings.contains("reapply_on_wake"))
    {
        reapply_on_wake_ = settings["reapply_on_wake"].get<bool>();
    }
    if (settings.contains("activity_detect_enabled"))
    {
        activity_detect_enabled_ = settings["activity_detect_enabled"].get<bool>();
    }
    if (settings.contains("idle_resume_sec"))
    {
        idle_resume_sec_ = settings["idle_resume_sec"].get<int>();
    }
    if (settings.contains("log_enabled"))
    {
        log_enabled_ = settings["log_enabled"].get<bool>();
    }
}

void WakePlugin::SaveSettings()
{
    if (!api_)
    {
        return;
    }

    nlohmann::json settings;
    settings["target_key"]              = target_key_;
    settings["reapply_on_wake"]         = reapply_on_wake_;
    settings["activity_detect_enabled"] = activity_detect_enabled_;
    settings["idle_resume_sec"]         = idle_resume_sec_;
    settings["log_enabled"]             = log_enabled_;

    api_->SetSettings("OpenRGBWakePlugin", settings);
    api_->SaveSettings();
}

void WakePlugin::Log(const char* msg, unsigned int level)
{
    if (!log_enabled_ || !api_)
    {
        return;
    }

    api_->LogEntry(__FILE__, __LINE__, level, "%s", msg);
}