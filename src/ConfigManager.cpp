#include "ConfigManager.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_system.h>
#include "Storage.h"

ConfigManager ConfigMgr;

bool ConfigManager::hasWiFiCredentials() const
{
  if (!config.wifi_networks.empty()) return true;
  return config.wifi_ssid.length() > 0 &&
         config.wifi_ssid != "YOUR_HOME_WIFI_SSID" &&
         config.wifi_ssid != "YOUR_WIFI_SSID";
}

static void readWiFiNetworks(JsonVariantConst node,
                             std::vector<WiFiCredential> &networks)
{
  networks.clear();
  if (!node.is<JsonArrayConst>()) return;
  for (JsonVariantConst value : node.as<JsonArrayConst>())
  {
    String ssid = value["ssid"] | "";
    String password = value["password"] | "";
    ssid.trim();
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 64) continue;
    bool duplicate = false;
    for (const WiFiCredential &existing : networks)
      if (existing.ssid == ssid) duplicate = true;
    if (!duplicate) networks.push_back({ssid, password});
    if (networks.size() == 4) break;
  }
}

static void writeWiFiNetworks(JsonArray array,
                              const std::vector<WiFiCredential> &networks)
{
  for (const WiFiCredential &network : networks)
  {
    JsonObject value = array.add<JsonObject>();
    value["ssid"] = network.ssid;
    value["password"] = network.password;
  }
}

static void readScheduleTaskList(JsonVariantConst node,
                                 std::vector<SleepScheduleTask> &out)
{
  out.clear();
  if (!node.is<JsonArrayConst>()) return;
  for (JsonVariantConst v : node.as<JsonArrayConst>())
  {
    SleepScheduleTask t{};
    int h = v["hour"] | -1;
    int m = v["minute"] | -1;
    if (h < 0 || h > 23 || m < 0 || m > 59) continue;
    t.hour = (uint8_t)h;
    t.minute = (uint8_t)m;
    if (v["days"].is<JsonArrayConst>())
    {
      for (JsonVariantConst d : v["days"].as<JsonArrayConst>())
      {
        int idx = d.as<int>();
        if (idx >= 0 && idx <= 6) t.days_mask |= (uint8_t)(1 << idx);
      }
    }
    if (t.days_mask == 0) continue;  // skip tasks with no active days
    out.push_back(t);
  }
}

static void writeScheduleTaskList(JsonArray arr,
                                  const std::vector<SleepScheduleTask> &tasks)
{
  for (const auto &t : tasks)
  {
    JsonObject o = arr.add<JsonObject>();
    o["hour"] = t.hour;
    o["minute"] = t.minute;
    JsonArray days = o["days"].to<JsonArray>();
    for (int i = 0; i < 7; i++)
      if (t.days_mask & (1 << i)) days.add(i);
  }
}

static void readStopList(JsonVariantConst node, std::vector<String> &out)
{
  out.clear();
  if (node.is<JsonArrayConst>())
  {
    for (JsonVariantConst v : node.as<JsonArrayConst>())
    {
      String s = v.as<String>();
      s.trim();
      if (s.length() > 0)
        out.push_back(s);
    }
  }
  else if (node.is<const char *>())
  {
    String s = node.as<String>();
    s.trim();
    if (s.length() > 0)
      out.push_back(s);
  }
}

static void readKmbFavorites(JsonVariantConst node,
                             std::vector<KmbFavorite> &out)
{
  out.clear();
  if (!node.is<JsonArrayConst>()) return;

  for (JsonObjectConst value : node.as<JsonArrayConst>())
  {
    KmbFavorite favorite;
    favorite.route = value["route"].as<const char *>() ? value["route"].as<const char *>() : "";
    favorite.direction = value["direction"].as<const char *>() ? value["direction"].as<const char *>() : "";
    favorite.serviceType = value["service_type"] | 0;
    favorite.destinationTc = value["destination_tc"].as<const char *>() ? value["destination_tc"].as<const char *>() : "";
    favorite.stopId = value["stop_id"].as<const char *>() ? value["stop_id"].as<const char *>() : "";
    favorite.stopNameTc = value["stop_name_tc"].as<const char *>() ? value["stop_name_tc"].as<const char *>() : "";

    std::string error;
    KmbFavoriteModel::add(out, favorite, error);
    if (out.size() >= KmbFavoriteModel::kMaxFavorites) break;
  }
}

static void writeKmbFavorites(JsonArray array,
                              const std::vector<KmbFavorite> &favorites)
{
  for (const auto &favorite : favorites)
  {
    JsonObject value = array.add<JsonObject>();
    value["route"] = favorite.route.c_str();
    value["direction"] = favorite.direction.c_str();
    value["service_type"] = favorite.serviceType;
    value["destination_tc"] = favorite.destinationTc.c_str();
    value["stop_id"] = favorite.stopId.c_str();
    value["stop_name_tc"] = favorite.stopNameTc.c_str();
  }
}

static void readAgenda(JsonVariantConst node, std::vector<ScheduleItem> &out)
{
  out.clear();
  if (!node.is<JsonArrayConst>()) return;
  for (JsonVariantConst value : node.as<JsonArrayConst>())
  {
    ScheduleItem item;
    item.time = value["time"].as<const char *>() ? value["time"].as<const char *>() : "";
    item.title = value["title"].as<const char *>() ? value["title"].as<const char *>() : "";
    item.daysMask = (uint8_t)(value["days_mask"] | 0);
    if (ScheduleModel::isValid(item)) out.push_back(item);
  }
}

static void writeAgenda(JsonArray array, const std::vector<ScheduleItem> &items)
{
  for (const auto &item : items)
  {
    JsonObject value = array.add<JsonObject>();
    value["time"] = item.time;
    value["title"] = item.title;
    value["days_mask"] = item.daysMask;
  }
}

static String defaultApPassword()
{
  Preferences preferences;
  const bool opened = preferences.begin("buseta", false);
  if (opened)
  {
    const String saved = preferences.getString("ap-pass", "");
    if (saved.length() >= 12)
    {
      preferences.end();
      return saved;
    }
  }

  char password[24];
  snprintf(password, sizeof(password), "cfg-%08lX%08lX",
           (unsigned long)esp_random(), (unsigned long)esp_random());
  if (opened)
  {
    preferences.putString("ap-pass", password);
    preferences.end();
  }
  return password;
}

bool ConfigManager::load()
{
  if (!Storage_Begin())
  {
    printf("LittleFS Mount Failed\n");
    return loadDefault();
  }

  File file = LittleFS.open("/config.json", "r");
  if (!file)
  {
    printf("config.json not found, using defaults\n");
    return loadDefault();
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    printf("Failed to parse config.json\n");
    return loadDefault();
  }

  config.wifi_ssid = doc["wifi"]["ssid"] | "";
  config.wifi_pass = doc["wifi"]["password"] | "";
  readWiFiNetworks(doc["wifi"]["networks"], config.wifi_networks);
  if (!config.wifi_ssid.isEmpty())
  {
    bool found = false;
    for (const WiFiCredential &network : config.wifi_networks)
      if (network.ssid == config.wifi_ssid) found = true;
    if (!found && config.wifi_networks.size() < 4)
      config.wifi_networks.insert(config.wifi_networks.begin(),
                                  {config.wifi_ssid, config.wifi_pass});
  }
  config.ap_ssid = doc["ap"]["ssid"] | "BusETA-Config";
  config.ap_pass = doc["ap"]["password"] | "";
  if (config.ap_pass.length() < 8) config.ap_pass = defaultApPassword();

  readStopList(doc["stops"]["kmb"], config.kmb_stop_ids);
  readKmbFavorites(doc["kmb_favorites"], config.kmb_favorites);
  config.kmb_defaults_initialized = doc["kmb_defaults_initialized"] | false;
  readStopList(doc["stops"]["ctb"], config.ctb_stop_ids);

  readScheduleTaskList(doc["sleep_schedule"]["wake_tasks"], config.wake_tasks);
  readScheduleTaskList(doc["sleep_schedule"]["sleep_tasks"], config.sleep_tasks);
  readAgenda(doc["agenda"], config.agenda_items);
  config.google_tasks_enabled = doc["google_tasks"]["enabled"] | false;
  config.google_tasks_bridge_url = doc["google_tasks"]["bridge_url"] | "";
  config.google_tasks_device_token = doc["google_tasks"]["device_token"] | "";
  config.google_tasks_list_id = doc["google_tasks"]["tasklist_id"] | "@default";
  {
    int g = doc["sleep_schedule"]["grace_period_minutes"] | 5;
    if (g < 0) g = 0;
    if (g > 60) g = 60;
    config.grace_period_minutes = (uint8_t)g;
  }

  printf("Config loaded: %d KMB stop(s), %d CTB stop(s), %d wake task(s), %d sleep task(s)\n",
         (int)config.kmb_stop_ids.size(), (int)config.ctb_stop_ids.size(),
         (int)config.wake_tasks.size(), (int)config.sleep_tasks.size());
  return true;
}

bool ConfigManager::loadDefault()
{
  config.wifi_ssid = "";
  config.wifi_pass = "";
  config.wifi_networks.clear();
  config.ap_ssid = "BusETA-Config";
  config.ap_pass = defaultApPassword();
  config.kmb_stop_ids.clear();
  config.kmb_favorites.clear();
  config.kmb_defaults_initialized = false;
  config.ctb_stop_ids.clear();
  config.agenda_items = {
      {"08:00", "準備返學／返工", 0x7F},
      {"18:00", "晚餐", 0x7F},
  };
  config.google_tasks_enabled = false;
  config.google_tasks_bridge_url = "";
  config.google_tasks_device_token = "";
  config.google_tasks_list_id = "@default";
  return true;
}

bool ConfigManager::save()
{
  if (!Storage_Begin())
    return false;

  JsonDocument doc;
  doc["wifi"]["ssid"] = config.wifi_ssid;
  doc["wifi"]["password"] = config.wifi_pass;
  writeWiFiNetworks(doc["wifi"]["networks"].to<JsonArray>(),
                    config.wifi_networks);
  doc["ap"]["ssid"] = config.ap_ssid;
  doc["ap"]["password"] = config.ap_pass;

  JsonArray kmb = doc["stops"]["kmb"].to<JsonArray>();
  for (const String &s : config.kmb_stop_ids)
    kmb.add(s);

  JsonArray ctb = doc["stops"]["ctb"].to<JsonArray>();
  for (const String &s : config.ctb_stop_ids)
    ctb.add(s);

  writeKmbFavorites(doc["kmb_favorites"].to<JsonArray>(), config.kmb_favorites);
  doc["kmb_defaults_initialized"] = config.kmb_defaults_initialized;

  writeScheduleTaskList(doc["sleep_schedule"]["wake_tasks"].to<JsonArray>(),
                        config.wake_tasks);
  writeScheduleTaskList(doc["sleep_schedule"]["sleep_tasks"].to<JsonArray>(),
                        config.sleep_tasks);
  doc["sleep_schedule"]["grace_period_minutes"] = config.grace_period_minutes;
  writeAgenda(doc["agenda"].to<JsonArray>(), config.agenda_items);
  doc["google_tasks"]["enabled"] = config.google_tasks_enabled;
  doc["google_tasks"]["bridge_url"] = config.google_tasks_bridge_url;
  doc["google_tasks"]["device_token"] = config.google_tasks_device_token;
  doc["google_tasks"]["tasklist_id"] = config.google_tasks_list_id;

  LittleFS.remove("/config.tmp");
  File file = LittleFS.open("/config.tmp", "w");
  if (!file)
    return false;

  const size_t bytesWritten = serializeJson(doc, file);
  file.flush();
  file.close();

  if (bytesWritten == 0)
  {
    LittleFS.remove("/config.tmp");
    return false;
  }

  const bool hadCurrent = LittleFS.exists("/config.json");
  LittleFS.remove("/config.bak");
  if (hadCurrent && !LittleFS.rename("/config.json", "/config.bak"))
  {
    LittleFS.remove("/config.tmp");
    return false;
  }

  if (!LittleFS.rename("/config.tmp", "/config.json"))
  {
    if (hadCurrent)
      LittleFS.rename("/config.bak", "/config.json");
    LittleFS.remove("/config.tmp");
    return false;
  }

  LittleFS.remove("/config.bak");
  printf("config.json saved\n");
  return true;
}

bool ConfigManager::update(const Config &newConfig, String &error)
{
  const Config oldConfig = config;
  config = newConfig;
  if (save())
  {
    error = "";
    return true;
  }

  config = oldConfig;
  error = "儲存失敗，原有設定已保留";
  return false;
}
