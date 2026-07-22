#include "WeatherData.h"
#include "Display.h"
#include "Diagnostics.h"
#include "JsonInternal.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

float current_temperature = 0.0f;
uint8_t current_humidity = 68;
char current_weather_emoji[8] = "";
char current_weather_desc[64] = "等待天文台資料";

static std::vector<HkoForecastDay> s_forecast;
static SemaphoreHandle_t s_forecastMutex = nullptr;

static const char *weatherSummary(int icon)
{
    if (icon >= 50 && icon <= 52) return "天晴";
    if (icon >= 53 && icon <= 59) return "多雲";
    if (icon >= 60 && icon <= 69) return "有雨";
    if (icon >= 70 && icon <= 79) return "驟雨";
    if (icon >= 80) return "雷暴";
    return "天氣";
}

static const JsonDocument &forecastFilter()
{
    static JsonDocument filter;
    static bool initialized = false;
    if (!initialized)
    {
        filter["weatherForecast"][0]["forecastDate"] = true;
        filter["weatherForecast"][0]["week"] = true;
        filter["weatherForecast"][0]["forecastWeather"] = true;
        filter["weatherForecast"][0]["forecastMintemp"]["value"] = true;
        filter["weatherForecast"][0]["forecastMaxtemp"]["value"] = true;
        filter["weatherForecast"][0]["ForecastIcon"] = true;
        initialized = true;
    }
    return filter;
}

static const JsonDocument &currentConditionsFilter()
{
    static JsonDocument filter;
    static bool initialized = false;
    if (!initialized)
    {
        filter["temperature"]["data"][0]["place"] = true;
        filter["temperature"]["data"][0]["value"] = true;
        filter["humidity"]["data"][0]["value"] = true;
        filter["icon"][0] = true;
        initialized = true;
    }
    return filter;
}

static bool getJson(const char *url, JsonDocument &doc)
{
    WiFiClientSecure client;
    // HKO data is public and carries no device credentials. The small device
    // does not bundle a mutable CA store, so certificate verification is
    // intentionally disabled for these read-only feeds.
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, url)) return false;
    http.setTimeout(12000);

    int status = http.GET();
    if (status != HTTP_CODE_OK)
    {
        printf("HKO HTTP %d: %s\n", status, url);
        http.end();
        return false;
    }

    // HKO replies with HTTP/1.1 chunked transfer encoding. Reading the raw
    // stream directly has produced InvalidInput on Arduino-ESP32 3.3.x, while
    // getString() lets HTTPClient de-chunk the small (roughly 3-5 KiB) body.
    String payload = http.getString();
    http.end();
    if (payload.isEmpty())
    {
        printf("HKO returned an empty JSON body\n");
        return false;
    }
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        printf("HKO JSON error: %s (%u bytes)\n", error.c_str(),
               static_cast<unsigned>(payload.length()));
        return false;
    }
    return true;
}

static bool fetchForecast(std::vector<HkoForecastDay> &days)
{
    JsonDocument doc(jsonInternalAllocator());
    if (!getJson(
            "https://data.weather.gov.hk/weatherAPI/opendata/weather.php?dataType=fnd&lang=tc&rformat=json",
            doc))
        return false;

    std::string validationError;
    for (JsonObjectConst item : doc["weatherForecast"].as<JsonArrayConst>())
    {
        HkoForecastDay day;
        day.date = item["forecastDate"] | "";
        day.week = item["week"] | "";
        day.weather = item["forecastWeather"] | "";
        day.minTemp = item["forecastMintemp"]["value"] | -99;
        day.maxTemp = item["forecastMaxtemp"]["value"] | 99;
        day.icon = item["ForecastIcon"] | 0;
        if (!HkoForecastModel::append(days, day, validationError) &&
            days.size() < HkoForecastModel::kMaxDays)
        {
            printf("Skipped invalid HKO forecast: %s\n", validationError.c_str());
        }
        if (days.size() == HkoForecastModel::kMaxDays) break;
    }
    printf("HKO forecast parsed: %u accepted day(s), source array=%u\n",
           static_cast<unsigned>(days.size()),
           static_cast<unsigned>(doc["weatherForecast"].size()));
    return days.size() == HkoForecastModel::kMaxDays;
}

static void fetchCurrentConditions()
{
    JsonDocument doc(jsonInternalAllocator());
    if (!getJson(
            "https://data.weather.gov.hk/weatherAPI/opendata/weather.php?dataType=rhrread&lang=tc&rformat=json",
            doc))
        return;

    float fallback = -999.0f;
    for (JsonObjectConst station : doc["temperature"]["data"].as<JsonArrayConst>())
    {
        float value = station["value"] | -999.0f;
        if (fallback < -100.0f) fallback = value;
        const char *place = station["place"] | "";
        if (strcmp(place, "香港天文台") == 0)
        {
            current_temperature = value;
            fallback = -999.0f;
            break;
        }
    }
    if (fallback > -100.0f) current_temperature = fallback;

    int humidity = doc["humidity"]["data"][0]["value"] | -1;
    if (humidity >= 0 && humidity <= 100) current_humidity = humidity;
}

std::vector<HkoForecastDay> Weather_GetForecast()
{
    if (!s_forecastMutex) return {};
    xSemaphoreTake(s_forecastMutex, portMAX_DELAY);
    std::vector<HkoForecastDay> snapshot = s_forecast;
    xSemaphoreGive(s_forecastMutex);
    return snapshot;
}

extern volatile bool g_apiFetchInProgress;

void Weather_FetchHko()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        printf("WiFi not connected - cannot fetch HKO weather\n");
        return;
    }
    if (!s_forecastMutex) s_forecastMutex = xSemaphoreCreateMutex();

    g_apiFetchInProgress = true;
    Heap_Log("HKO weather pre-fetch");
    std::vector<HkoForecastDay> next;
    bool forecastOk = fetchForecast(next);
    if (forecastOk)
    {
        xSemaphoreTake(s_forecastMutex, portMAX_DELAY);
        s_forecast.swap(next);
        xSemaphoreGive(s_forecastMutex);

        const std::vector<HkoForecastDay> snapshot = Weather_GetForecast();
        if (!snapshot.empty())
        {
            strncpy(current_weather_desc, weatherSummary(snapshot.front().icon),
                    sizeof(current_weather_desc) - 1);
            current_weather_desc[sizeof(current_weather_desc) - 1] = '\0';
        }
    }
    fetchCurrentConditions();
    Update_Weather();
    Heap_Log(forecastOk ? "HKO weather updated" : "HKO forecast retained");
    g_apiFetchInProgress = false;
}
