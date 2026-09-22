/*---------------------------------------------------------*\
| OpenRGB Wake Plugin                                       |
|                                                           |
|  Re-applies a saved device state (mode + colors) when a   |
|  wireless device wakes up or reconnects.                  |
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
    info.Description     = "Re-applies device lighting when a wireless device wakes or reconnects";
    info.Version         = "1.1.0";
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
    widget_settings.target_key            = QString::fromStdString(target_key_);
    widget_settings.reapply_on_change     = reapply_on_change_;
    widget_settings.reapply_on_wake       = reapply_on_wake_;
    widget_settings.activity_detect_enabled = activity_detect_enabled_;
    widget_settings.idle_resume_sec       = idle_resume_sec_;
    widget_settings.log_enabled           = log_enabled_;

    widget_ = new SettingsWidget(widget_settings);
    connect(widget_, &SettingsWidget::settingsChanged, this, &WakePlugin::SettingsChanged);
    connect(widget_, &SettingsWidget::snapshotRequested, this, &WakePlugin::TakeSnapshotNow);

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
        QTimer::singleShot(700, this, [this]() { ReapplyTargets("mouse activity"); });
        QTimer::singleShot(2600, this, [this]()
        {
            if (!last_restore_ok_)
            {
                ReapplyTargets("mouse activity (retry)");
            }
        });
    });

    if (activity_detect_enabled_ && !mouse_watcher_->start())
    {
        Log("Warning: could not install mouse activity hook", LOG_LEVEL_WARNING);
    }

    /* Capture the current state of the target device(s) if already detected */
    SnapshotTargets();

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
    /* A new profile was applied; refresh the snapshot shortly after */
    QTimer::singleShot(1000, this, [this]()
    {
        SnapshotTargets();
        Log("Snapshot refreshed after profile load");
    });
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

void WakePlugin::ResourceManagerUpdated(unsigned int update_reason)
{
    if ((update_reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE) ||
        (update_reason == RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED))
    {
        /* First time devices show up: capture their state instead of restoring */
        if (snapshots_.empty())
        {
            SnapshotTargets();
        }
        else if (reapply_on_change_)
        {
            ScheduleReapply("device change", 1500);
        }
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
    if (reapply_on_wake_)
    {
        ScheduleReapply("PC wake", 2500);
    }
}

void WakePlugin::SettingsChanged()
{
    if (!widget_)
    {
        return;
    }

    SettingsWidget::Settings widget_settings = widget_->GetSettings();

    target_key_             = widget_settings.target_key.toStdString();
    reapply_on_change_      = widget_settings.reapply_on_change;
    reapply_on_wake_        = widget_settings.reapply_on_wake;
    activity_detect_enabled_ = widget_settings.activity_detect_enabled;
    idle_resume_sec_        = widget_settings.idle_resume_sec;
    log_enabled_            = widget_settings.log_enabled;

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

void WakePlugin::TakeSnapshotNow()
{
    SnapshotTargets();

    if (widget_)
    {
        if (snapshots_.empty())
        {
            widget_->SetStatusText("Целевое устройство не найдено — снимок не сделан");
        }
        else
        {
            widget_->SetStatusText(QString("Снимок сделан: устройств — %1").arg(static_cast<int>(snapshots_.size())));
        }
    }
}

/*---------------------------------------------------------*\
| Device Matching and State Snapshot/Restore                |
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

bool WakePlugin::SnapshotController(RGBControllerInterface* ctrl, DeviceSnap& snap)
{
    snap = DeviceSnap();

    snap.name           = ctrl->GetName();
    snap.location       = ctrl->GetLocation();
    snap.description    = ctrl->GetDescription();
    snap.active_mode    = ctrl->GetActiveMode();
    snap.per_zone_modes = ctrl->SupportsPerZoneModes();

    /* Per-LED colors of the whole device */
    unsigned int led_count = ctrl->GetLEDCount();
    snap.led_colors.resize(led_count);
    for (unsigned int li = 0; li < led_count; li++)
    {
        snap.led_colors[li] = ctrl->GetColor(li);
    }

    /* Global modes */
    unsigned int mode_count = ctrl->GetModeCount();
    snap.modes.resize(mode_count);
    for (unsigned int mi = 0; mi < mode_count; mi++)
    {
        ModeSnap& ms = snap.modes[mi];
        ms.speed      = ctrl->GetModeSpeed(mi);
        ms.brightness = ctrl->GetModeBrightness(mi);
        ms.direction  = ctrl->GetModeDirection(mi);
        ms.color_mode = ctrl->GetModeColorMode(mi);

        unsigned int color_count = ctrl->GetModeColorsCount(mi);
        ms.colors.resize(color_count);
        for (unsigned int ci = 0; ci < color_count; ci++)
        {
            ms.colors[ci] = ctrl->GetModeColor(mi, ci);
        }
    }

    /* Zones (used for per-LED colors and per-zone modes) */
    unsigned int zone_count = ctrl->GetZoneCount();
    snap.zones.resize(zone_count);
    for (unsigned int zi = 0; zi < zone_count; zi++)
    {
        ZoneSnap& zs = snap.zones[zi];
        zs.active_mode = ctrl->GetZoneActiveMode(zi);

        unsigned int zone_leds = ctrl->GetZoneLEDsCount(zi);
        zs.colors.resize(zone_leds);
        for (unsigned int li = 0; li < zone_leds; li++)
        {
            zs.colors[li] = ctrl->GetZoneColor(zi, li);
        }

        unsigned int zone_mode_count = ctrl->GetZoneModeCount(zi);
        zs.modes.resize(zone_mode_count);
        for (unsigned int mi = 0; mi < zone_mode_count; mi++)
        {
            ModeSnap& ms = zs.modes[mi];
            ms.speed      = ctrl->GetZoneModeSpeed(zi, mi);
            ms.brightness = ctrl->GetZoneModeBrightness(zi, mi);
            ms.direction  = ctrl->GetZoneModeDirection(zi, mi);
            ms.color_mode = ctrl->GetZoneModeColorMode(zi, mi);

            unsigned int color_count = ctrl->GetZoneModeColorsCount(zi, mi);
            ms.colors.resize(color_count);
            for (unsigned int ci = 0; ci < color_count; ci++)
            {
                ms.colors[ci] = ctrl->GetZoneModeColor(zi, mi, ci);
            }
        }
    }

    return true;
}

bool WakePlugin::RestoreController(RGBControllerInterface* ctrl, const DeviceSnap& snap)
{
    if (!ctrl)
    {
        return false;
    }

    if (snap.per_zone_modes)
    {
        unsigned int zone_count = std::min<unsigned int>(ctrl->GetZoneCount(), static_cast<unsigned int>(snap.zones.size()));

        for (unsigned int zi = 0; zi < zone_count; zi++)
        {
            const ZoneSnap& zs = snap.zones[zi];

            ctrl->SetZoneActiveMode(zi, zs.active_mode);

            unsigned int mode_count = std::min<unsigned int>(ctrl->GetZoneModeCount(zi), static_cast<unsigned int>(zs.modes.size()));
            for (unsigned int mi = 0; mi < mode_count; mi++)
            {
                const ModeSnap& ms = zs.modes[mi];
                ctrl->SetZoneModeSpeed(zi, mi, ms.speed);
                ctrl->SetZoneModeBrightness(zi, mi, ms.brightness);
                ctrl->SetZoneModeDirection(zi, mi, ms.direction);
                ctrl->SetZoneModeColorMode(zi, mi, ms.color_mode);

                unsigned int color_count = std::min<unsigned int>(ctrl->GetZoneModeColorsCount(zi, mi), static_cast<unsigned int>(ms.colors.size()));
                for (unsigned int ci = 0; ci < color_count; ci++)
                {
                    ctrl->SetZoneModeColor(zi, mi, ci, ms.colors[ci]);
                }
            }

            unsigned int color_count = std::min<unsigned int>(ctrl->GetZoneLEDsCount(zi), static_cast<unsigned int>(zs.colors.size()));
            for (unsigned int ci = 0; ci < color_count; ci++)
            {
                ctrl->SetZoneColor(zi, ci, zs.colors[ci]);
            }

            ctrl->UpdateZoneMode(static_cast<int>(zi));
            ctrl->UpdateZoneLEDs(static_cast<int>(zi));
        }
    }
    else
    {
        if ((snap.active_mode >= 0) && (static_cast<unsigned int>(snap.active_mode) < ctrl->GetModeCount()))
        {
            int mode_index = snap.active_mode;
            const ModeSnap& ms = snap.modes[mode_index];

            ctrl->SetActiveMode(mode_index);
            ctrl->SetModeSpeed(mode_index, ms.speed);
            ctrl->SetModeBrightness(mode_index, ms.brightness);
            ctrl->SetModeDirection(mode_index, ms.direction);
            ctrl->SetModeColorMode(mode_index, ms.color_mode);

            unsigned int color_count = std::min<unsigned int>(ctrl->GetModeColorsCount(mode_index), static_cast<unsigned int>(ms.colors.size()));
            for (unsigned int ci = 0; ci < color_count; ci++)
            {
                ctrl->SetModeColor(mode_index, ci, ms.colors[ci]);
            }
        }

        unsigned int led_count = std::min<unsigned int>(ctrl->GetLEDCount(), static_cast<unsigned int>(snap.led_colors.size()));
        for (unsigned int li = 0; li < led_count; li++)
        {
            ctrl->SetColor(li, snap.led_colors[li]);
        }

        ctrl->UpdateMode();
        ctrl->UpdateLEDs();
    }

    return true;
}

/*---------------------------------------------------------*\
| Snapshot and Re-apply Driver                              |
\*---------------------------------------------------------*/
void WakePlugin::SnapshotTargets()
{
    if (!api_)
    {
        return;
    }

    snapshots_.clear();

    for (RGBControllerInterface* ctrl : api_->GetRGBControllers())
    {
        if (MatchesTarget(ctrl))
        {
            DeviceSnap snap;
            if (SnapshotController(ctrl, snap))
            {
                snapshots_.push_back(snap);
            }
        }
    }

    char msg[128];
    std::snprintf(msg, sizeof(msg), "Snapshot taken of %zu target device(s)", snapshots_.size());
    Log(msg);
}

WakePlugin::DeviceSnap* WakePlugin::FindSnapshotFor(RGBControllerInterface* ctrl)
{
    std::string location = ctrl->GetLocation();
    std::string name     = ctrl->GetName();

    for (DeviceSnap& snap : snapshots_)
    {
        if (!location.empty() && (snap.location == location))
        {
            return &snap;
        }
    }

    for (DeviceSnap& snap : snapshots_)
    {
        if (snap.name == name)
        {
            return &snap;
        }
    }

    if (snapshots_.size() == 1)
    {
        return &snapshots_[0];
    }

    return nullptr;
}

void WakePlugin::ScheduleReapply(const char* reason, int delay_ms)
{
    if (reapply_pending_)
    {
        return;
    }

    reapply_pending_ = true;

    QTimer::singleShot(delay_ms, this, [this, reason]()
    {
        ReapplyTargets(reason);
    });
}

void WakePlugin::ReapplyTargets(const char* reason)
{
    reapply_pending_ = false;

    if (!api_)
    {
        return;
    }

    if (snapshots_.empty())
    {
        SnapshotTargets();
        last_restore_ok_ = true;
        return;
    }

    bool restored = false;

    for (RGBControllerInterface* ctrl : api_->GetRGBControllers())
    {
        if (MatchesTarget(ctrl))
        {
            DeviceSnap* snap = FindSnapshotFor(ctrl);
            if ((snap != nullptr) && RestoreController(ctrl, *snap))
            {
                restored = true;
            }
        }
    }

    last_restore_ok_ = restored;

    char msg[160];
    std::snprintf(msg, sizeof(msg), "Re-applied lighting after %s (%s)", reason, restored ? "done" : "no target");
    Log(msg);
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
    if (settings.contains("reapply_on_change"))
    {
        reapply_on_change_ = settings["reapply_on_change"].get<bool>();
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
    settings["target_key"]             = target_key_;
    settings["reapply_on_change"]      = reapply_on_change_;
    settings["reapply_on_wake"]        = reapply_on_wake_;
    settings["activity_detect_enabled"] = activity_detect_enabled_;
    settings["idle_resume_sec"]        = idle_resume_sec_;
    settings["log_enabled"]            = log_enabled_;

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