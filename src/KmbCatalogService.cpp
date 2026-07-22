#include "KmbCatalogService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <cctype>

#include "ConfigManager.h"
#include "JsonInternal.h"
#include "KmbCatalogModel.h"
#include "KmbFavoriteModel.h"
#include "KmbRouteCache.h"
#include "Storage.h"

namespace {

constexpr const char *kKmbApiBase =
    "https://data.etabus.gov.hk/v1/transport/kmb";
constexpr std::size_t kMaxStops = 80;

const char *apiDirection(const std::string &direction)
{
  if (direction == "I") return "inbound";
  if (direction == "O") return "outbound";
  return "";
}

const JsonDocument &directionFilter()
{
  static JsonDocument filter;
  static bool initialized = false;
  if (!initialized)
  {
    filter["data"]["dest_tc"] = true;
    initialized = true;
  }
  return filter;
}

const JsonDocument &routeStopFilter()
{
  static JsonDocument filter;
  static bool initialized = false;
  if (!initialized)
  {
    filter["data"][0]["stop"] = true;
    filter["data"][0]["seq"] = true;
    initialized = true;
  }
  return filter;
}

const JsonDocument &stopFilter()
{
  static JsonDocument filter;
  static bool initialized = false;
  if (!initialized)
  {
    filter["data"]["name_tc"] = true;
    initialized = true;
  }
  return filter;
}

bool getJson(const String &url,
             JsonDocument &document,
             const JsonDocument &,
             std::string &error)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    error = "網絡未連線";
    return false;
  }

  static WiFiClientSecure client;
  static bool clientInitialized = false;
  if (!clientInitialized)
  {
    client.setInsecure();
    clientInitialized = true;
  }
  for (int attempt = 1; attempt <= 2; ++attempt)
  {
    HTTPClient http;
    http.setTimeout(12000);
    http.setConnectTimeout(6000);
    http.setReuse(true);
    if (!http.begin(client, url))
    {
      error = "無法建立九巴資料連線";
    }
    else
    {
      const int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK)
      {
        const String payload = http.getString();
        http.end();
        if (!payload.isEmpty())
        {
          document.clear();
          const DeserializationError jsonError = deserializeJson(document, payload);
          if (!jsonError)
            return true;
          printf("KMB catalog JSON error: %s, payload bytes=%u, attempt=%d\n",
                 jsonError.c_str(), static_cast<unsigned>(payload.length()),
                 attempt);
          error = "九巴資料格式無效";
        }
        else
        {
          error = "九巴資料服務回覆空白";
        }
      }
      else
      {
        printf("KMB catalog HTTP %d: %s, attempt=%d\n", httpCode,
               url.c_str(), attempt);
        error = httpCode == HTTP_CODE_NOT_FOUND
                    ? "搵唔到呢條九巴路線"
                    : "九巴資料服務暫時未能回應";
        http.end();
        if (httpCode == HTTP_CODE_NOT_FOUND)
          return false;
      }
    }

    client.stop();
    if (attempt < 2 && WiFi.status() == WL_CONNECTED)
      delay(250);
  }
  return false;
}

std::size_t resolveStopNamesFromCache(std::vector<KmbStopOption> &stops)
{
  if (!Storage_Begin()) return 0;
  File file = LittleFS.open("/kmb-stops.tsv", "r");
  if (!file)
  {
    printf("KMB local stop-name cache unavailable\n");
    return 0;
  }

  std::size_t resolved = 0;
  while (file.available() && resolved < stops.size())
  {
    String line = file.readStringUntil('\n');
    const int separator = line.indexOf('\t');
    if (separator != 16 || line.length() <= 17) continue;
    const String stopId = line.substring(0, separator);
    for (auto &stop : stops)
    {
      if (stop.stopNameTc.empty() && stopId == stop.stopId.c_str())
      {
        stop.stopNameTc = line.substring(separator + 1).c_str();
        ++resolved;
        break;
      }
    }
  }
  file.close();
  printf("KMB local stop-name cache matched %u/%u stops\n",
         static_cast<unsigned>(resolved), static_cast<unsigned>(stops.size()));
  return resolved;
}

bool resolveStopName(const std::string &stopId,
                     std::string &stopName,
                     std::string &error)
{
  JsonDocument document(jsonInternalAllocator());
  const String url = String(kKmbApiBase) + "/stop/" + stopId.c_str();
  if (!getJson(url, document, stopFilter(), error))
    return false;

  const char *name = document["data"]["name_tc"] | "";
  if (!name[0])
  {
    error = "車站名稱資料不完整";
    return false;
  }
  stopName = name;
  return true;
}

std::string normalizeStopName(const std::string &name)
{
  std::string withoutCode = name;
  const std::size_t code = withoutCode.rfind(" (");
  if (code != std::string::npos) withoutCode.erase(code);
  const auto first = std::find_if_not(withoutCode.begin(), withoutCode.end(), [](unsigned char character) {
    return std::isspace(character) != 0;
  });
  const auto last = std::find_if_not(withoutCode.rbegin(), withoutCode.rend(), [](unsigned char character) {
    return std::isspace(character) != 0;
  }).base();
  return first < last ? std::string(first, last) : std::string();
}

}  // namespace

KmbCatalogService KmbCatalog;

void KmbCatalogService::begin()
{
  if (!mutex_)
    mutex_ = xSemaphoreCreateMutex();
}

bool KmbCatalogService::requestDirections(const std::string &route)
{
  if (!mutex_)
    return false;

  const std::string normalizedRoute = KmbFavoriteModel::normalizeRoute(route);
  if (normalizedRoute.empty() || normalizedRoute.size() > 6)
    return false;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  if (processing_ || pending_.type != RequestType::None)
  {
    xSemaphoreGive(mutex_);
    return false;
  }

  pending_ = {RequestType::Directions, normalizedRoute, "", 1};
  const uint32_t nextGeneration = snapshot_.generation + 1;
  snapshot_ = {};
  snapshot_.status = KmbCatalogStatus::Loading;
  snapshot_.route = normalizedRoute;
  snapshot_.message = "正在查詢九巴路線…";
  snapshot_.generation = nextGeneration;
  xSemaphoreGive(mutex_);
  return true;
}

bool KmbCatalogService::requestStops(const std::string &route,
                                     const std::string &direction,
                                     int serviceType)
{
  if (!mutex_ || (direction != "I" && direction != "O") ||
      serviceType < 1 || serviceType > 3)
    return false;

  const std::string normalizedRoute = KmbFavoriteModel::normalizeRoute(route);
  if (normalizedRoute.empty() || normalizedRoute.size() > 6)
    return false;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  if (processing_ || pending_.type != RequestType::None)
  {
    xSemaphoreGive(mutex_);
    return false;
  }

  pending_ = {RequestType::Stops, normalizedRoute, direction, serviceType};
  const uint32_t nextGeneration = snapshot_.generation + 1;
  snapshot_ = {};
  snapshot_.status = KmbCatalogStatus::Loading;
  snapshot_.route = normalizedRoute;
  snapshot_.direction = direction;
  snapshot_.serviceType = serviceType;
  snapshot_.message = "正在載入車站…";
  snapshot_.generation = nextGeneration;
  xSemaphoreGive(mutex_);
  return true;
}

KmbCatalogSnapshot KmbCatalogService::snapshot() const
{
  KmbCatalogSnapshot copy;
  if (!mutex_)
    return copy;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  copy = snapshot_;
  xSemaphoreGive(mutex_);
  return copy;
}

void KmbCatalogService::publish(KmbCatalogSnapshot next, bool processing)
{
  xSemaphoreTake(mutex_, portMAX_DELAY);
  next.generation = snapshot_.generation + 1;
  snapshot_ = std::move(next);
  processing_ = processing;
  xSemaphoreGive(mutex_);
}

bool KmbCatalogService::loadDirections(
    const std::string &route,
    std::vector<KmbDirectionOption> &directions,
    std::string &error)
{
  directions.clear();
  const std::vector<KmbCachedDirection> cached = KmbRouteCache::find(route);
  if (!cached.empty())
  {
    directions.reserve(cached.size());
    for (const KmbCachedDirection &value : cached)
      directions.push_back({value.direction, value.serviceType,
                            value.destinationTc});
    printf("KMB route cache matched %s (%u directions)\n", route.c_str(),
           static_cast<unsigned>(directions.size()));
    return true;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    error = "網絡未連線";
    return false;
  }
  std::string lastRequestError;
  for (const char *direction : {"I", "O"})
  {
    for (int serviceType = 1; serviceType <= 3; ++serviceType)
    {
      JsonDocument document(jsonInternalAllocator());
      const String url = String(kKmbApiBase) + "/route/" + route.c_str() +
                         "/" + apiDirection(direction) + "/" + serviceType;
      std::string requestError;
      if (!getJson(url, document, directionFilter(), requestError))
      {
        if (requestError != "搵唔到呢條九巴路線")
          lastRequestError = requestError;
        continue;
      }

      const char *destination = document["data"]["dest_tc"] | "";
      if (destination[0])
        directions.push_back({direction, serviceType, destination});
    }
  }

  if (directions.empty())
  {
    error = lastRequestError.empty() ? "搵唔到呢條九巴路線"
                                     : lastRequestError;
    return false;
  }
  return true;
}

bool KmbCatalogService::loadStops(const std::string &route,
                                  const std::string &direction,
                                  int serviceType,
                                  std::vector<KmbStopOption> &stops,
                                  std::string &error,
                                  bool publishProgress)
{
  stops.clear();
  JsonDocument document(jsonInternalAllocator());
  const String url = String(kKmbApiBase) + "/route-stop/" + route.c_str() +
                     "/" + apiDirection(direction) + "/" + serviceType;
  if (!getJson(url, document, routeStopFilter(), error) ||
      !document["data"].is<JsonArrayConst>())
  {
    if (error.empty()) error = "搵唔到呢個方向嘅車站";
    return false;
  }

  std::vector<KmbStopOption> unresolved;
  const std::size_t rawStopCount = document["data"].size();
  for (JsonObjectConst value : document["data"].as<JsonArrayConst>())
  {
    const char *stopId = value["stop"] | "";
    // The official KMB API represents seq as a JSON string (for example
    // "1"), not a JSON number. Parse it explicitly instead of asking
    // ArduinoJson for an int, which returns zero for every stop.
    const char *sequenceText = value["seq"] | "";
    const int sequence = KmbCatalogModel::parseSequence(sequenceText);
    if (stopId[0] && sequence > 0)
      unresolved.push_back({stopId, "", sequence});
    if (unresolved.size() >= kMaxStops)
      break;
  }

  printf("KMB route-stop parsed: route=%s direction=%s service=%d raw=%u usable=%u\n",
         route.c_str(), direction.c_str(), serviceType,
         static_cast<unsigned>(rawStopCount),
         static_cast<unsigned>(unresolved.size()));

  if (unresolved.empty())
  {
    error = "呢個方向暫時冇車站資料";
    return false;
  }

  resolveStopNamesFromCache(unresolved);
  for (std::size_t index = 0; index < unresolved.size(); ++index)
  {
    if (unresolved[index].stopNameTc.empty())
    {
      std::string stopError;
      resolveStopName(unresolved[index].stopId,
                      unresolved[index].stopNameTc,
                      stopError);
    }
    if (!unresolved[index].stopNameTc.empty())
      stops.push_back(unresolved[index]);

    if (publishProgress)
    {
      KmbCatalogSnapshot progress;
      progress.status = KmbCatalogStatus::Loading;
      progress.route = route;
      progress.direction = direction;
      progress.serviceType = serviceType;
      progress.message = "正在載入車站 " + std::to_string(index + 1) + "/" +
                         std::to_string(unresolved.size());
      publish(std::move(progress), true);
    }
  }

  if (stops.empty())
  {
    error = "未能載入車站名稱";
    return false;
  }

  std::sort(stops.begin(), stops.end(), [](const auto &left, const auto &right) {
    return left.sequence < right.sequence;
  });
  return true;
}

void KmbCatalogService::processOneRequest()
{
  if (!mutex_)
    return;

  xSemaphoreTake(mutex_, portMAX_DELAY);
  if (processing_ || pending_.type == RequestType::None)
  {
    xSemaphoreGive(mutex_);
    return;
  }
  const Request request = pending_;
  pending_ = {};
  processing_ = true;
  xSemaphoreGive(mutex_);

  KmbCatalogSnapshot result;
  result.route = request.route;
  result.direction = request.direction;
  result.serviceType = request.serviceType;
  std::string error;
  bool ok = false;
  if (request.type == RequestType::Directions)
    ok = loadDirections(request.route, result.directions, error);
  else if (request.type == RequestType::Stops)
    ok = loadStops(request.route, request.direction, request.serviceType,
                   result.stops, error, true);

  result.status = ok ? KmbCatalogStatus::Ready : KmbCatalogStatus::Error;
  result.message = ok ? "" : error;
  publish(std::move(result), false);
}

bool KmbCatalogService::resolveInitialPresets()
{
  if (ConfigMgr.getConfig().kmb_defaults_initialized)
    return true;

  Config next = ConfigMgr.getConfig();
  // Public builds intentionally start without location-specific favourites.
  // Every user selects their own routes and stops through the local portal.
  next.kmb_defaults_initialized = true;
  String saveError;
  const bool saved = ConfigMgr.update(next, saveError);
  if (!saved)
    printf("Initial KMB preset state could not be saved\n");
  return saved;
}
