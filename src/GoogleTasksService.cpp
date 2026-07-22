#include "GoogleTasksService.h"

#include "Display.h"
#include "JsonInternal.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

static SemaphoreHandle_t s_mutex = nullptr;
static std::vector<GoogleTaskItem> s_items;
static GoogleTasksStatus s_status = GoogleTasksStatus::Disabled;
static std::string s_error;

static std::string localDate()
{
    time_t now = time(nullptr);
    struct tm value {};
    localtime_r(&now, &value);
    char date[11];
    snprintf(date, sizeof(date), "%04d-%02d-%02d",
             value.tm_year + 1900, value.tm_mon + 1, value.tm_mday);
    return date;
}

void GoogleTasks_Begin()
{
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
}

static void publishError(const char *message)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_status = GoogleTasksStatus::Error;
    s_error = message;
    xSemaphoreGive(s_mutex);
}

extern volatile bool g_apiFetchInProgress;

bool GoogleTasks_Refresh(const Config &config)
{
    if (!s_mutex) GoogleTasks_Begin();
    if (!config.google_tasks_enabled)
    {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_status = GoogleTasksStatus::Disabled;
        s_error.clear();
        s_items.clear();
        xSemaphoreGive(s_mutex);
        return true;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        publishError("Wi-Fi 未連線");
        return false;
    }
    if (!config.google_tasks_bridge_url.startsWith(
            "https://script.google.com/macros/s/") ||
        config.google_tasks_device_token.length() < 16)
    {
        publishError("Google Tasks 尚未完成設定");
        return false;
    }

    g_apiFetchInProgress = true;
    File rootsFile = LittleFS.open("/google-roots.pem", "r");
    if (!rootsFile)
    {
        publishError("Google TLS 根證書缺失");
        g_apiFetchInProgress = false;
        return false;
    }
    String trustedRoots = rootsFile.readString();
    rootsFile.close();
    if (trustedRoots.isEmpty())
    {
        publishError("Google TLS 根證書無效");
        g_apiFetchInProgress = false;
        return false;
    }
    WiFiClientSecure client;
    client.setCACert(trustedRoots.c_str());
    HTTPClient http;
    if (!http.begin(client, config.google_tasks_bridge_url))
    {
        publishError("無法連接 Google Tasks bridge");
        g_apiFetchInProgress = false;
        return false;
    }
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");

    JsonDocument request;
    request["device_token"] = config.google_tasks_device_token;
    request["date"] = localDate();
    request["tasklist_id"] = config.google_tasks_list_id;
    String body;
    serializeJson(request, body);

    int status = http.POST(body);
    if (status != HTTP_CODE_OK)
    {
        printf("Google Tasks bridge HTTP %d\n", status);
        http.end();
        publishError("Google Tasks 同步失敗");
        g_apiFetchInProgress = false;
        return false;
    }

    JsonDocument filter;
    filter["ok"] = true;
    filter["tasks"][0]["title"] = true;
    filter["tasks"][0]["due"] = true;
    filter["tasks"][0]["completed"] = true;
    JsonDocument response(jsonInternalAllocator());
    DeserializationError parseError = deserializeJson(
        response, http.getStream(), DeserializationOption::Filter(filter));
    http.end();

    if (parseError || !(response["ok"] | false))
    {
        publishError("Google Tasks 回應無效");
        g_apiFetchInProgress = false;
        return false;
    }

    std::vector<GoogleTaskItem> next;
    std::string validationError;
    for (JsonObjectConst value : response["tasks"].as<JsonArrayConst>())
    {
        GoogleTaskItem item;
        item.title = value["title"] | "";
        item.due = value["due"] | "";
        item.completed = value["completed"] | false;
        if (!GoogleTaskModel::append(next, item, validationError) &&
            next.size() < GoogleTaskModel::kMaxItems)
        {
            printf("Skipped invalid Google Task: %s\n",
                   validationError.c_str());
        }
        if (next.size() == GoogleTaskModel::kMaxItems) break;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_items.swap(next);
    s_status = GoogleTasksStatus::Ready;
    s_error.clear();
    xSemaphoreGive(s_mutex);
    g_apiFetchInProgress = false;
    Update_Schedule_Display();
    printf("Google Tasks updated\n");
    return true;
}

GoogleTasksStatus GoogleTasks_GetStatus()
{
    if (!s_mutex) return GoogleTasksStatus::Disabled;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    GoogleTasksStatus status = s_status;
    xSemaphoreGive(s_mutex);
    return status;
}

std::vector<GoogleTaskItem> GoogleTasks_GetForDate(const std::string &yyyyMmDd)
{
    if (!s_mutex) return {};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    std::vector<GoogleTaskItem> snapshot = s_items;
    xSemaphoreGive(s_mutex);
    return GoogleTaskModel::forDate(snapshot, yyyyMmDd);
}

std::string GoogleTasks_GetError()
{
    if (!s_mutex) return "";
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    std::string error = s_error;
    xSemaphoreGive(s_mutex);
    return error;
}
