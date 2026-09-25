/*---------------------------------------------------------*\
| OpenRGB Wake Plugin - hardened build                       |
| Version 1.3.2-hardened.1                                   |
|                                                            |
| Restores lighting only by asking OpenRGB to load a profile |
| configured in OpenRGB Profile Manager. The unsafe raw       |
| RGBControllerInterface fallback is intentionally disabled.  |
\*---------------------------------------------------------*/
#include "WakePlugin.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>

#include <QImage>
#include <QString>
#include <QTimer>

#include "MouseActivityWatcher.h"
#include "PowerWatcher.h"
#include "SettingsWidget.h"

enum
{
    LOG_LEVEL_ERROR   = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_INFO    = 2,
};

namespace
{
constexpr int kMinIdleResumeSec = 10;
constexpr int kMaxIdleResumeSec = 600;

/* Startup window: how long (ms) after Load() the plugin should keep
   watching ResourceManager events and retrying the profile restore. */
constexpr int kStartupWindowMs    = 30000;

/* Debounce delays after detection / device-list changes */
constexpr int kDetectionDebounceMs  = 750;
constexpr int kDetectionRetryMs     = 2500;
}

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
    info.Version         = "1.3.2-hardened.1";
    info.Commit          = "";
    info.URL             = "https://github.com/Tyoman1/openrgb-wake-plugin-hardened";
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
    widget_settings.reapply_on_wake         = reapply_on_wake_;
    widget_settings.activity_detect_enabled = activity_detect_enabled_;
    widget_settings.idle_resume_sec         = idle_resume_sec_;
    widget_settings.log_enabled             = log_enabled_;

    widget_ = new SettingsWidget(widget_settings);
    connect(widget_, &SettingsWidget::settingsChanged, this, &WakePlugin::SettingsChanged);

    power_watcher_ = new PowerWatcher(this);
    power_watcher_->on_resume = [this]()
    {
        OnPowerResume();
    };

    mouse_watcher_ = new MouseActivityWatcher(this);
    mouse_watcher_->setIdleThresholdMs(static_cast<quint64>(idle_resume_sec_) * 1000ULL);

    connect(mouse_watcher_, &MouseActivityWatcher::mouseResumed, this, [this]()
    {
        /* Mouse wake applies profile, but during the startup window
           a later detection-complete apply may still follow. */
        QTimer::singleShot(700, this, [this]()
        {
            ApplyTargets("mouse activity");
        });

        QTimer::singleShot(2600, this, [this]()
        {
            ApplyTargets("mouse activity (retry)");
        });
    });

    if (activity_detect_enabled_)
    {
        if (!mouse_watcher_->start())
        {
            Log("Warning: could not install mouse activity hook", LOG_LEVEL_WARNING);
        }
        else
        {
            Log("Mouse activity hook installed");
            mouse_watcher_->armOneShot();
        }
    }

    /* ── Initial startup attempt ── */
    QTimer::singleShot(2000, this, [this]()
    {
        /* This is the first quick attempt; the startup window keeps
           working even if this returns true — see ApplyStartup(). */
        ApplyStartup();
    });

    Log("Plugin loaded; startup window active for 30 s");
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
    /* Close the startup window immediately so no more callbacks fire */
    startup_restore_pending_ = false;

    if (mouse_watcher_)
    {
        if (!mouse_watcher_->stop())
        {
            Log("Warning: Windows did not confirm removal of the mouse hook; "
                "the DLL remains pinned for safety",
                LOG_LEVEL_WARNING);
        }

        delete mouse_watcher_;
        mouse_watcher_ = nullptr;
    }

    delete power_watcher_;
    power_watcher_ = nullptr;

    /* OpenRGB owns the settings widget after it is placed in the plugin tab. */
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
}

nlohmann::json WakePlugin::OnProfileSave()
{
    return nlohmann::json::object();
}

unsigned char* WakePlugin::OnSDKCommand(unsigned int /* pkt_id */,
                                         unsigned char* /* pkt_data */,
                                         unsigned int* /* pkt_size */)
{
    return nullptr;
}

/*---------------------------------------------------------*\
| Update Signals                                            |
\*---------------------------------------------------------*/
void WakePlugin::ProfileManagerUpdated(unsigned int /* update_reason */)
{
}

void WakePlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    if (!startup_restore_pending_)
        return;

    if (update_reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE)
    {
        Log("Startup: detection complete received", LOG_LEVEL_INFO);

        /* Debounced: wait for firmware to stabilise */
        ScheduleDetectionRestore(kDetectionDebounceMs,
                                 "startup detection complete");

        /* Also schedule a later retry for devices that need more time */
        ScheduleDetectionRestore(kDetectionRetryMs,
                                 "startup detection retry");
    }
    else if (update_reason == RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED)
    {
        /* During startup, device list changes may indicate a late-arriving device */
        Log("Startup: device list updated during startup window", LOG_LEVEL_INFO);
        ScheduleDetectionRestore(kDetectionDebounceMs,
                                 "startup device list updated");
    }
}

void WakePlugin::SettingsManagerUpdated(unsigned int /* update_reason */)
{
}

/*---------------------------------------------------------*\
| Event Handlers                                            |
\*---------------------------------------------------------*/
void WakePlugin::OnPowerResume()
{
    if (!reapply_on_wake_)
    {
        return;
    }

    QTimer::singleShot(2500, this, [this]()
    {
        ApplyTargets("PC wake", true);
    });

    if (mouse_watcher_ && mouse_watcher_->isActive())
    {
        mouse_watcher_->armOneShot();
    }
}

void WakePlugin::SettingsChanged()
{
    if (!widget_)
    {
        return;
    }

    const SettingsWidget::Settings widget_settings = widget_->GetSettings();

    reapply_on_wake_         = widget_settings.reapply_on_wake;
    activity_detect_enabled_ = widget_settings.activity_detect_enabled;
    idle_resume_sec_         = std::clamp(widget_settings.idle_resume_sec,
                                          kMinIdleResumeSec,
                                          kMaxIdleResumeSec);
    log_enabled_             = widget_settings.log_enabled;

    if (mouse_watcher_)
    {
        mouse_watcher_->setIdleThresholdMs(static_cast<quint64>(idle_resume_sec_) * 1000ULL);

        if (activity_detect_enabled_)
        {
            if (!mouse_watcher_->start())
            {
                Log("Warning: could not install mouse activity hook", LOG_LEVEL_WARNING);
            }
        }
        else
        {
            if (!mouse_watcher_->stop())
            {
                Log("Warning: could not remove mouse activity hook", LOG_LEVEL_WARNING);
            }
        }
    }

    SaveSettings();
    Log("Settings saved");
}

/*---------------------------------------------------------*\
| Profile Name Resolution                                   |
\*---------------------------------------------------------*/
std::string WakePlugin::ResolveProfileName(bool resume_trigger)
{
    if (!api_)
    {
        return "";
    }

    try
    {
        const nlohmann::json pm_settings = api_->GetSettings("ProfileManager");
        if (!pm_settings.is_object())
        {
            return "";
        }

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
            if (!pm_settings.contains(key) || !pm_settings[key].is_object())
            {
                continue;
            }

            const nlohmann::json& profile = pm_settings[key];

            if (!profile.contains("enabled")
                || !profile["enabled"].is_boolean()
                || !profile["enabled"].get<bool>()
                || !profile.contains("name")
                || !profile["name"].is_string())
            {
                continue;
            }

            const std::string name = profile["name"].get<std::string>();
            if (!name.empty())
            {
                return name;
            }
        }
    }
    catch (const std::exception& ex)
    {
        char msg[256];
        std::snprintf(msg,
                      sizeof(msg),
                      "Warning: failed to resolve OpenRGB profile: %s",
                      ex.what());
        Log(msg, LOG_LEVEL_WARNING);
    }
    catch (...)
    {
        Log("Warning: failed to resolve OpenRGB profile due to an unknown error",
            LOG_LEVEL_WARNING);
    }

    return "";
}

/*---------------------------------------------------------*\
| Profile Restore                                            |
\*---------------------------------------------------------*/
bool WakePlugin::ApplyTargets(const char* reason, bool resume_trigger)
{
    if (!api_)
    {
        last_restore_ok_ = false;
        return false;
    }

    const std::string profile_name = ResolveProfileName(resume_trigger);

    if (profile_name.empty())
    {
        last_restore_ok_ = false;

        Log("No auto-load profile configured; raw-controller fallback is disabled",
            LOG_LEVEL_WARNING);

        if (widget_)
        {
            widget_->SetStatusText("Настройте профиль автозагрузки OpenRGB");
        }

        return false;
    }

    bool loaded = false;

    try
    {
        loaded = api_->LoadProfile(profile_name);
    }
    catch (const std::exception& ex)
    {
        char msg[256];
        std::snprintf(msg,
                      sizeof(msg),
                      "Warning: LoadProfile threw an exception: %s",
                      ex.what());
        Log(msg, LOG_LEVEL_WARNING);
        loaded = false;
    }
    catch (...)
    {
        Log("Warning: LoadProfile failed with an unknown exception",
            LOG_LEVEL_WARNING);
        loaded = false;
    }

    last_restore_ok_ = loaded;

    char msg[192];
    std::snprintf(msg,
                  sizeof(msg),
                  "Loaded profile '%s' after %s (%s)",
                  profile_name.c_str(),
                  reason,
                  loaded ? "done" : "failed");
    Log(msg);

    if (widget_)
    {
        widget_->SetStatusText(
            loaded
                ? QString("Загружен профиль «%1» (%2)")
                      .arg(QString::fromStdString(profile_name), reason)
                : "Не удалось загрузить профиль");
    }

    return loaded;
}

/*---------------------------------------------------------*\
| Startup helpers                                            |
\*---------------------------------------------------------*/

/* Schedule a delayed apply within the startup window.
   Multiple calls with the same reason are coalesced:
   only the latest timer matters. */
void WakePlugin::ScheduleDetectionRestore(int delay_ms, const char* reason)
{
    if (!startup_restore_pending_)
        return;

    /* Keep the string alive for the lambda */
    QString reason_q = QString::fromUtf8(reason);

    QTimer::singleShot(delay_ms, this, [this, reason_q]()
    {
        if (!startup_restore_pending_)
            return;
        OnDetectionRestore(reason_q.toUtf8().constData());
    });
}

void WakePlugin::OnDetectionRestore(const char* reason)
{
    if (!startup_restore_pending_)
        return;

    ApplyTargets(reason);
}

void WakePlugin::ApplyStartup()
{
    if (!startup_restore_pending_)
        return;

    /* Always try — return value does NOT close the startup window */
    ApplyTargets("startup");

    if (startup_retries_left_ > 0)
    {
        --startup_retries_left_;
        QTimer::singleShot(1000, this, [this]()
        {
            ApplyStartup();
        });
    }
    else
    {
        /* Retries exhausted; the ResourceManager callback can still
           reapply during the remaining startup window time. */
        Log("Startup: initial retries exhausted, "
            "still watching ResourceManager events",
            LOG_LEVEL_INFO);

        /* Close the startup window after kStartupWindowMs total */
        QTimer::singleShot(kStartupWindowMs, this, [this]()
        {
            startup_restore_pending_ = false;
            startup_restore_scheduled_ = false;
            Log("Startup restore window finished");
        });
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

    try
    {
        const nlohmann::json settings = api_->GetSettings("OpenRGBWakePlugin");

        if (!settings.is_object())
        {
            Log("Warning: Wake Plugin settings are not a JSON object; using defaults",
                LOG_LEVEL_WARNING);
            return;
        }

        if (settings.contains("reapply_on_wake")
            && settings["reapply_on_wake"].is_boolean())
        {
            reapply_on_wake_ = settings["reapply_on_wake"].get<bool>();
        }

        if (settings.contains("activity_detect_enabled")
            && settings["activity_detect_enabled"].is_boolean())
        {
            activity_detect_enabled_ = settings["activity_detect_enabled"].get<bool>();
        }

        if (settings.contains("idle_resume_sec")
            && settings["idle_resume_sec"].is_number_integer())
        {
            const std::int64_t value =
                settings["idle_resume_sec"].get<std::int64_t>();

            idle_resume_sec_ = static_cast<int>(
                std::clamp(value,
                           static_cast<std::int64_t>(kMinIdleResumeSec),
                           static_cast<std::int64_t>(kMaxIdleResumeSec)));
        }

        if (settings.contains("log_enabled")
            && settings["log_enabled"].is_boolean())
        {
            log_enabled_ = settings["log_enabled"].get<bool>();
        }
    }
    catch (const std::exception& ex)
    {
        char msg[256];
        std::snprintf(msg,
                      sizeof(msg),
                      "Warning: failed to load Wake Plugin settings: %s",
                      ex.what());
        Log(msg, LOG_LEVEL_WARNING);
    }
    catch (...)
    {
        Log("Warning: failed to load Wake Plugin settings due to an unknown error",
            LOG_LEVEL_WARNING);
    }
}

void WakePlugin::SaveSettings()
{
    if (!api_)
    {
        return;
    }

    try
    {
        nlohmann::json settings;
        settings["reapply_on_wake"]         = reapply_on_wake_;
        settings["activity_detect_enabled"] = activity_detect_enabled_;
        settings["idle_resume_sec"]         = idle_resume_sec_;
        settings["log_enabled"]             = log_enabled_;

        api_->SetSettings("OpenRGBWakePlugin", settings);
        api_->SaveSettings();
    }
    catch (const std::exception& ex)
    {
        char msg[256];
        std::snprintf(msg,
                      sizeof(msg),
                      "Warning: failed to save Wake Plugin settings: %s",
                      ex.what());
        Log(msg, LOG_LEVEL_WARNING);
    }
    catch (...)
    {
        Log("Warning: failed to save Wake Plugin settings due to an unknown error",
            LOG_LEVEL_WARNING);
    }
}

void WakePlugin::Log(const char* msg, unsigned int level)
{
    if (!log_enabled_ || !api_)
    {
        return;
    }

    api_->LogEntry(__FILE__, __LINE__, level, "%s", msg);
}