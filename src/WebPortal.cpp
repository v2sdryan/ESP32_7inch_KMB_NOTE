#include "WebPortal.h"
#include "ConfigManager.h"
#include "Display.h"
#include "KmbWebPage.h"
#include "PortalHomePage.h"
#include "PortalIdentity.h"
#include "WiFiNetworkList.h"
#include "WiFiPasswordValidation.h"
#include "WiFiSettingsPage.h"

#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFi.h>

static WebServer server(80);
static bool mdnsActive = false;
static IPAddress mdnsAddress;
static uint32_t lastMdnsCheck = 0;
static bool restorePureApAfterScan = false;
static uint32_t restorePureApAt = 0;
static bool wifiScanRequested = false;
static uint32_t lastWifiScanStartedAt = 0;

static bool accessPointPortalActive()
{
  const wifi_mode_t mode = WiFi.getMode();
  return mode == WIFI_AP || mode == WIFI_AP_STA;
}

static void updateMdns(bool force = false)
{
  const uint32_t now = millis();
  if (!force && now - lastMdnsCheck < 2000) return;
  lastMdnsCheck = now;

  const bool stationReady = WiFi.status() == WL_CONNECTED &&
                            WiFi.getMode() != WIFI_AP;
  const IPAddress currentAddress = stationReady ? WiFi.localIP() : IPAddress();
  if (mdnsActive && (!stationReady || currentAddress != mdnsAddress))
  {
    MDNS.end();
    mdnsActive = false;
    mdnsAddress = IPAddress();
  }
  if (!mdnsActive && stationReady)
  {
    mdnsActive = MDNS.begin(PortalIdentity::kHostName);
    if (mdnsActive)
    {
      mdnsAddress = currentAddress;
      MDNS.setInstanceName("BusETA 家庭資訊屏");
      MDNS.addService("http", "tcp", 80);
      printf("mDNS ready: http://%s\n", PortalIdentity::kLocalHost);
    }
    else
    {
      printf("mDNS unavailable; use IP fallback http://%s\n",
             currentAddress.toString().c_str());
    }
  }
}

static void sendJson(int code, const JsonDocument &doc)
{
  String out;
  serializeJson(doc, out);
  server.send(code, "application/json", out);
}

static bool requirePortalOrigin()
{
  const String origin = server.header("Origin");
  const String referer = server.header("Referer");
  const String stationBase = String("http://") + WiFi.localIP().toString();
  const String accessPointBase =
      String("http://") + WiFi.softAPIP().toString();
  const auto matchesBase = [](const String &value, const String &base,
                              bool allowPath) {
    if (base.endsWith("0.0.0.0")) return false;
    return value == base || value == base + ":80" ||
           (allowPath && (value.startsWith(base + "/") ||
                          value.startsWith(base + ":80/")));
  };
  const bool validOrigin =
      (!origin.isEmpty() &&
       (matchesBase(origin, PortalIdentity::kLocalUrl, false) ||
        matchesBase(origin, stationBase, false) ||
        matchesBase(origin, accessPointBase, false))) ||
      (origin.isEmpty() && !referer.isEmpty() &&
       (matchesBase(referer, PortalIdentity::kLocalUrl, true) ||
        matchesBase(referer, stationBase, true) ||
        matchesBase(referer, accessPointBase, true)));
  if (validOrigin) return true;

  JsonDocument err;
  err["ok"] = false;
  err["error"] = "forbidden origin";
  sendJson(403, err);
  return false;
}

static void writeStopArray(JsonObject parent, const char *key, const std::vector<String> &ids)
{
  JsonArray arr = parent[key].to<JsonArray>();
  for (const String &s : ids)
    arr.add(s);
}

static void readStopListFromJson(JsonVariantConst node, std::vector<String> &out)
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

static void writeScheduleArray(JsonArray arr,
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

static void readScheduleArray(JsonVariantConst node,
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
    if (t.days_mask == 0) continue;
    out.push_back(t);
  }
}

static void writeAgendaArray(JsonArray array, const std::vector<ScheduleItem> &items)
{
  for (const auto &item : items)
  {
    JsonObject value = array.add<JsonObject>();
    value["time"] = item.time;
    value["title"] = item.title;
    value["days_mask"] = item.daysMask;
  }
}

static void readAgendaArray(JsonVariantConst node, std::vector<ScheduleItem> &items)
{
  items.clear();
  if (!node.is<JsonArrayConst>()) return;
  for (JsonVariantConst value : node.as<JsonArrayConst>())
  {
    ScheduleItem item;
    item.time = value["time"].as<const char *>() ? value["time"].as<const char *>() : "";
    item.title = value["title"].as<const char *>() ? value["title"].as<const char *>() : "";
    item.daysMask = (uint8_t)(value["days_mask"] | 0);
    if (ScheduleModel::isValid(item) && items.size() < 24) items.push_back(item);
  }
}

static void writeKmbFavorites(JsonArray array,
                              const std::vector<KmbFavorite> &favorites)
{
  for (const KmbFavorite &favorite : favorites)
  {
    JsonObject value = array.add<JsonObject>();
    value["route"] = favorite.route;
    value["direction"] = favorite.direction;
    value["service_type"] = favorite.serviceType;
    value["destination_tc"] = favorite.destinationTc;
    value["stop_id"] = favorite.stopId;
    value["stop_name_tc"] = favorite.stopNameTc;
  }
}

static bool readKmbFavorites(JsonVariantConst node,
                             std::vector<KmbFavorite> &favorites,
                             String &error)
{
  if (!node.is<JsonArrayConst>())
  {
    error = "九巴收藏設定無效";
    return false;
  }

  std::vector<KmbFavorite> parsed;
  for (JsonVariantConst value : node.as<JsonArrayConst>())
  {
    if (!value.is<JsonObjectConst>())
    {
      error = "九巴收藏設定無效";
      return false;
    }

    KmbFavorite favorite;
    favorite.route = value["route"] | "";
    favorite.direction = value["direction"] | "";
    favorite.serviceType = value["service_type"] | 0;
    favorite.destinationTc = value["destination_tc"] | "";
    favorite.stopId = value["stop_id"] | "";
    favorite.stopNameTc = value["stop_name_tc"] | "";

    std::string validationError;
    if (!KmbFavoriteModel::add(parsed, favorite, validationError))
    {
      error = validationError.c_str();
      if (error.isEmpty()) error = "九巴收藏設定無效";
      return false;
    }
  }

  favorites = std::move(parsed);
  return true;
}

static void handleGetConfig()
{
  const Config &cfg = ConfigMgr.getConfig();

  JsonDocument doc;
  doc["wifi"]["ssid"] = cfg.wifi_ssid;
  doc["wifi"]["password"] = "";
  doc["wifi"]["configured"] = !cfg.wifi_ssid.isEmpty();
  JsonArray wifiNetworks = doc["wifi"]["networks"].to<JsonArray>();
  for (size_t index = 0; index < cfg.wifi_networks.size(); ++index)
  {
    JsonObject network = wifiNetworks.add<JsonObject>();
    network["ssid"] = cfg.wifi_networks[index].ssid;
    network["password_configured"] =
        !cfg.wifi_networks[index].password.isEmpty();
    network["preferred"] = index == 0;
  }
  doc["ap"]["ssid"] = cfg.ap_ssid;
  doc["ap"]["password"] = "";

  JsonObject stops = doc["stops"].to<JsonObject>();
  writeStopArray(stops, "kmb", cfg.kmb_stop_ids);
  writeStopArray(stops, "ctb", cfg.ctb_stop_ids);

  JsonObject sched = doc["sleep_schedule"].to<JsonObject>();
  writeScheduleArray(sched["wake_tasks"].to<JsonArray>(), cfg.wake_tasks);
  writeScheduleArray(sched["sleep_tasks"].to<JsonArray>(), cfg.sleep_tasks);
  sched["grace_period_minutes"] = cfg.grace_period_minutes;
  writeAgendaArray(doc["agenda"].to<JsonArray>(), cfg.agenda_items);
  writeKmbFavorites(doc["kmb_favorites"].to<JsonArray>(), cfg.kmb_favorites);
  doc["kmb_defaults_initialized"] = cfg.kmb_defaults_initialized;
  doc["google_tasks"]["enabled"] = cfg.google_tasks_enabled;
  doc["google_tasks"]["bridge_url"] = cfg.google_tasks_bridge_url;
  doc["google_tasks"]["device_token"] = "";
  doc["google_tasks"]["token_configured"] =
      !cfg.google_tasks_device_token.isEmpty();
  doc["google_tasks"]["tasklist_id"] = cfg.google_tasks_list_id;

  sendJson(200, doc);
}

static void handlePostConfig()
{
  if (!requirePortalOrigin()) return;

  constexpr size_t kMaxConfigBodyBytes = 32768;
  const String contentLength = server.header("Content-Length");
  if ((!contentLength.isEmpty() &&
       contentLength.toInt() > (long)kMaxConfigBodyBytes) ||
      (server.hasArg("plain") &&
       server.arg("plain").length() > kMaxConfigBodyBytes))
  {
    JsonDocument err;
    err["ok"] = false;
    err["error"] = "request too large";
    sendJson(413, err);
    return;
  }

  if (!server.hasArg("plain"))
  {
    JsonDocument err;
    err["ok"] = false;
    err["error"] = "missing body";
    sendJson(400, err);
    return;
  }

  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError parseErr = deserializeJson(doc, body);
  if (parseErr)
  {
    JsonDocument err;
    err["ok"] = false;
    err["error"] = "invalid json";
    sendJson(400, err);
    return;
  }

  Config cfg = ConfigMgr.getConfig();

  struct WiFiNetworkUpdate
  {
    String ssid;
    String password;
    bool keepPassword;
    bool clearPassword;
  };

  const bool networkListSubmitted =
      doc["wifi"]["networks"].is<JsonArrayConst>();
  const bool wifiSubmitted = !networkListSubmitted &&
                             doc["wifi"]["ssid"].is<const char *>();
  const String previousSsid = cfg.wifi_ssid;
  if (wifiSubmitted)
  {
    cfg.wifi_ssid = doc["wifi"]["ssid"].as<String>();
    cfg.wifi_ssid.trim();
  }
  if (doc["wifi"]["clear_password"] | false)
    cfg.wifi_pass = "";
  else if (doc["wifi"]["password"].is<const char *>() &&
           strlen(doc["wifi"]["password"].as<const char *>()) > 0)
    cfg.wifi_pass = doc["wifi"]["password"].as<String>();
  else if (cfg.wifi_ssid != previousSsid)
    cfg.wifi_pass = "";

  if (wifiSubmitted &&
      (cfg.wifi_ssid.isEmpty() || cfg.wifi_ssid.length() > 32 ||
       !WiFiPasswordValidation::isValid(cfg.wifi_pass)))
  {
    JsonDocument err;
    err["ok"] = false;
    err["error"] = "Wi-Fi 設定無效";
    sendJson(400, err);
    return;
  }

  if (wifiSubmitted)
  {
    WiFiCredential selected{cfg.wifi_ssid, cfg.wifi_pass};
    const bool preserveKnownPassword =
        !(doc["wifi"]["clear_password"] | false);
    WiFiNetworkList::upsert(cfg.wifi_networks, selected, 4,
                            preserveKnownPassword);
    cfg.wifi_pass = cfg.wifi_networks.front().password;
  }
  else if (networkListSubmitted)
  {
    const JsonArrayConst submitted = doc["wifi"]["networks"].as<JsonArrayConst>();
    if (submitted.size() > 4)
    {
      JsonDocument err;
      err["ok"] = false;
      err["error"] = "最多只可儲存四個 Wi-Fi";
      sendJson(400, err);
      return;
    }

    std::vector<WiFiNetworkUpdate> updates;
    for (JsonVariantConst value : submitted)
    {
      WiFiNetworkUpdate update;
      update.ssid = value["ssid"] | "";
      update.password = value["password"] | "";
      update.keepPassword = value["keep_password"] | false;
      update.clearPassword = value["clear_password"] | false;
      update.ssid.trim();
      if (update.ssid.isEmpty() || update.ssid.length() > 32 ||
          !WiFiPasswordValidation::isValid(update.password))
      {
        JsonDocument err;
        err["ok"] = false;
        err["error"] = "Wi-Fi 設定無效";
        sendJson(400, err);
        return;
      }
      for (const WiFiNetworkUpdate &existing : updates)
        if (existing.ssid == update.ssid)
        {
          JsonDocument err;
          err["ok"] = false;
          err["error"] = "Wi-Fi 名稱重複";
          sendJson(400, err);
          return;
        }
      updates.push_back(update);
    }

    cfg.wifi_networks =
        WiFiNetworkList::replaceFromUpdates<WiFiCredential>(
            cfg.wifi_networks, updates, 4);
    if (cfg.wifi_networks.empty())
    {
      cfg.wifi_ssid = "";
      cfg.wifi_pass = "";
    }
    else
    {
      cfg.wifi_ssid = cfg.wifi_networks.front().ssid;
      cfg.wifi_pass = cfg.wifi_networks.front().password;
    }
  }

  if (!doc["stops"]["kmb"].isNull())
    readStopListFromJson(doc["stops"]["kmb"], cfg.kmb_stop_ids);
  if (!doc["stops"]["ctb"].isNull())
    readStopListFromJson(doc["stops"]["ctb"], cfg.ctb_stop_ids);

  if (!doc["sleep_schedule"]["wake_tasks"].isNull())
    readScheduleArray(doc["sleep_schedule"]["wake_tasks"], cfg.wake_tasks);
  if (!doc["sleep_schedule"]["sleep_tasks"].isNull())
    readScheduleArray(doc["sleep_schedule"]["sleep_tasks"], cfg.sleep_tasks);
  if (doc["sleep_schedule"]["grace_period_minutes"].is<int>())
  {
    int g = doc["sleep_schedule"]["grace_period_minutes"].as<int>();
    if (g < 0) g = 0;
    if (g > 60) g = 60;
    cfg.grace_period_minutes = (uint8_t)g;
  }
  if (!doc["agenda"].isNull())
    readAgendaArray(doc["agenda"], cfg.agenda_items);

  if (!doc["kmb_favorites"].isNull())
  {
    String validationError;
    if (!readKmbFavorites(doc["kmb_favorites"], cfg.kmb_favorites,
                          validationError))
    {
      JsonDocument err;
      err["ok"] = false;
      err["error"] = validationError;
      sendJson(400, err);
      return;
    }
  }
  if (doc["kmb_defaults_initialized"].is<bool>())
    cfg.kmb_defaults_initialized = doc["kmb_defaults_initialized"].as<bool>();

  if (!doc["google_tasks"].isNull())
  {
    JsonVariantConst google = doc["google_tasks"];
    if (google["enabled"].is<bool>())
      cfg.google_tasks_enabled = google["enabled"].as<bool>();
    if (google["bridge_url"].is<const char *>())
      cfg.google_tasks_bridge_url = google["bridge_url"].as<String>();
    if (google["tasklist_id"].is<const char *>())
      cfg.google_tasks_list_id = google["tasklist_id"].as<String>();
    if (google["clear_token"] | false)
      cfg.google_tasks_device_token = "";
    else if (google["device_token"].is<const char *>() &&
             strlen(google["device_token"].as<const char *>()) > 0)
      cfg.google_tasks_device_token = google["device_token"].as<String>();

    const bool validUrl =
        cfg.google_tasks_bridge_url.isEmpty() ||
        (cfg.google_tasks_bridge_url.startsWith(
             "https://script.google.com/macros/s/") &&
         cfg.google_tasks_bridge_url.length() <= 300);
    const bool validToken =
        cfg.google_tasks_device_token.isEmpty() ||
        (cfg.google_tasks_device_token.length() >= 16 &&
         cfg.google_tasks_device_token.length() <= 128);
    const bool completeWhenEnabled =
        !cfg.google_tasks_enabled ||
        (!cfg.google_tasks_bridge_url.isEmpty() &&
         !cfg.google_tasks_device_token.isEmpty());
    if (!validUrl || !validToken || !completeWhenEnabled ||
        cfg.google_tasks_list_id.length() > 128)
    {
      JsonDocument err;
      err["ok"] = false;
      err["error"] = "Google Tasks 設定無效";
      sendJson(400, err);
      return;
    }
  }

  String saveError;
  bool ok = ConfigMgr.update(cfg, saveError);
  if (ok) Update_Schedule_Display();

  JsonDocument resp;
  resp["ok"] = ok;
  if (!ok) resp["error"] = saveError;
  sendJson(ok ? 200 : 500, resp);
}

static void handlePostReboot()
{
  if (!requirePortalOrigin()) return;

  JsonDocument resp;
  resp["ok"] = true;
  sendJson(200, resp);
  server.client().clear();
  delay(200);
  ESP.restart();
}

static void handleWiFiScan()
{
  if (!requirePortalOrigin()) return;

  int16_t scanState = WiFi.scanComplete();
  if (scanState == WIFI_SCAN_FAILED && !wifiScanRequested)
  {
    const uint32_t now = millis();
    if (lastWifiScanStartedAt != 0 && now - lastWifiScanStartedAt < 5000)
    {
      JsonDocument err;
      err["ok"] = false;
      err["error"] = "scan rate limited";
      sendJson(429, err);
      return;
    }
    const bool pureApMode = WiFi.getMode() == WIFI_AP ||
                            restorePureApAfterScan || restorePureApAt != 0;
    restorePureApAt = 0;
    if (WiFi.getMode() == WIFI_AP) WiFi.mode(WIFI_AP_STA);
    WiFi.setScanTimeout(5000);
    scanState = WiFi.scanNetworks(true, true, false, 120);
    restorePureApAfterScan = pureApMode;
    wifiScanRequested = true;
    lastWifiScanStartedAt = now;

    JsonDocument pending;
    pending["ok"] = true;
    pending["scanning"] = true;
    sendJson(202, pending);
    return;
  }
  if (scanState == WIFI_SCAN_FAILED)
  {
    WiFi.scanDelete();
    wifiScanRequested = false;
    if (restorePureApAfterScan) restorePureApAt = millis() + 500;
    JsonDocument err;
    err["ok"] = false;
    err["error"] = "scan failed";
    sendJson(503, err);
    return;
  }
  if (scanState == WIFI_SCAN_RUNNING)
  {
    JsonDocument pending;
    pending["ok"] = true;
    pending["scanning"] = true;
    sendJson(202, pending);
    return;
  }

  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int index = 0; index < scanState && networks.size() < 20; ++index)
  {
    const String ssid = WiFi.SSID(index);
    if (ssid.isEmpty()) continue;
    bool duplicate = false;
    for (JsonVariantConst existing : networks)
      if (existing["ssid"].as<String>() == ssid) duplicate = true;
    if (duplicate) continue;

    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = ssid;
    network["rssi"] = WiFi.RSSI(index);
    network["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  wifiScanRequested = false;
  if (restorePureApAfterScan) restorePureApAt = millis() + 500;
  doc["ok"] = true;
  doc["scanning"] = false;
  sendJson(200, doc);
}

static String portalIpAddress()
{
  return accessPointPortalActive() ? WiFi.softAPIP().toString()
                                   : WiFi.localIP().toString();
}

static void handleGetStatus()
{
  const bool accessPointMode = accessPointPortalActive();
  const bool connected = accessPointMode || WiFi.status() == WL_CONNECTED;
  JsonDocument doc;
  doc["hostname"] = PortalIdentity::kLocalHost;
  doc["fixed_url"] = PortalIdentity::kLocalUrl;
  doc["ip"] = portalIpAddress();
  doc["mode"] = accessPointMode ? "ap" : "station";
  doc["connected"] = connected;
  doc["mdns"] = mdnsActive;
  sendJson(200, doc);
}

static void handleSettingsPage()
{
  File page = LittleFS.open("/index.html", "r");
  if (!page)
  {
    server.send(404, "text/plain; charset=utf-8", "設定頁不存在");
    return;
  }
  server.streamFile(page, "text/html; charset=utf-8");
  page.close();
}

static void handleNotFound()
{
  server.send(404, "text/plain", "Not found");
}

void WebPortal_Begin()
{
  static const char *requestHeaders[] = {
      "Origin", "Referer", "Content-Length"};
  server.collectHeaders(requestHeaders,
                        sizeof(requestHeaders) / sizeof(requestHeaders[0]));

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", PORTAL_HOME_PAGE);
  });
  server.on("/kmb", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", KMB_FAVORITES_PAGE);
  });
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/wifi", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", WIFI_SETTINGS_PAGE);
  });
  server.serveStatic("/favicon.ico", LittleFS, "/favicon.ico");
  server.serveStatic("/schedule", LittleFS, "/schedule.html");
  server.serveStatic("/schedule.html", LittleFS, "/schedule.html");
  server.serveStatic("/leaflet.js", LittleFS, "/leaflet.js");
  server.serveStatic("/leaflet.css", LittleFS, "/leaflet.css");
  server.serveStatic("/leaflet.markercluster.js", LittleFS, "/leaflet.markercluster.js");
  server.serveStatic("/MarkerCluster.css", LittleFS, "/MarkerCluster.css");
  server.serveStatic("/MarkerCluster.Default.css", LittleFS, "/MarkerCluster.Default.css");
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
  server.on("/api/reboot", HTTP_POST, handlePostReboot);
  server.onNotFound(handleNotFound);

  updateMdns(true);

  server.begin();
  printf("WebPortal started: http://%s (IP fallback: http://%s, mDNS=%s)\n",
         PortalIdentity::kLocalHost, portalIpAddress().c_str(),
         mdnsActive ? "ready" : "unavailable");
}

void WebPortal_Loop()
{
  if (restorePureApAt != 0 &&
      static_cast<int32_t>(millis() - restorePureApAt) >= 0)
  {
    WiFi.mode(WIFI_AP);
    restorePureApAfterScan = false;
    restorePureApAt = 0;
  }
  updateMdns();
  server.handleClient();
}

bool WebPortal_MdnsActive()
{
  return mdnsActive;
}
