#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "KmbFavoriteModel.h"
#include "ScheduleModel.h"

struct SleepScheduleTask
{
  uint8_t hour;       // 0-23
  uint8_t minute;     // 0-59
  uint8_t days_mask;  // bit 0 = Sun, bit 1 = Mon, ..., bit 6 = Sat
};

struct WiFiCredential
{
  String ssid;
  String password;
};

struct Config
{
  String wifi_ssid;
  String wifi_pass;
  std::vector<WiFiCredential> wifi_networks;
  String ap_ssid;
  String ap_pass;
  std::vector<String> kmb_stop_ids;
  std::vector<KmbFavorite> kmb_favorites;
  bool kmb_defaults_initialized = false;
  std::vector<String> ctb_stop_ids;
  std::vector<SleepScheduleTask> wake_tasks;
  std::vector<SleepScheduleTask> sleep_tasks;
  std::vector<ScheduleItem> agenda_items;
  bool google_tasks_enabled = false;
  String google_tasks_bridge_url;
  String google_tasks_device_token;
  String google_tasks_list_id = "@default";
  uint8_t grace_period_minutes = 5;  // wake-window grace before re-sleep
};

class ConfigManager
{
public:
  bool load();
  bool save();
  bool update(const Config &newConfig, String &error);
  const Config &getConfig() const { return config; }
  void setConfig(const Config &newConfig) { config = newConfig; }
  bool hasWiFiCredentials() const;

private:
  Config config;
  bool loadDefault();
};

extern ConfigManager ConfigMgr;
