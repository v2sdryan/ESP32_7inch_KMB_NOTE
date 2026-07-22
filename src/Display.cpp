#include "Display.h"
#include "BusData.h"
#include "WeatherData.h"
#include "HolidayData.h"
#include "Wireless.h"
#include "Diagnostics.h"
#include "ConfigManager.h"
#include "ScheduleModel.h"
#include "DashboardModel.h"
#include "KmbSettingsView.h"
#include "GoogleTasksService.h"
#include "Storage.h"
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include "lvgl_v8_port.h"
#include "ui/ui.h"
#include <esp_display_panel.hpp>
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

using namespace esp_panel::drivers;
using namespace esp_panel::board;

static Board *s_board = nullptr;
static lv_obj_t *s_infoOverlay = nullptr;
static lv_obj_t *s_infoTitle = nullptr;
static lv_obj_t *s_infoBody = nullptr;
static lv_obj_t *s_weatherGrid = nullptr;
static lv_obj_t *s_weatherCards[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_weatherLabels[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_weatherDetails[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_weatherIcons[HkoForecastModel::kMaxDays] = {};
static void *s_weatherIconBuffers[HkoForecastModel::kMaxDays] = {};
constexpr int kWeatherIconWidth = 72;
constexpr int kWeatherIconHeight = 68;
static lv_obj_t *s_wifiSsid = nullptr;
static lv_obj_t *s_wifiPass = nullptr;
static lv_obj_t *s_wifiSave = nullptr;
static lv_obj_t *s_wifiStatus = nullptr;
static lv_obj_t *s_wifiKeyboard = nullptr;
static uint8_t s_selectedPage = 0;
static lv_obj_t *s_dashboardScreen = nullptr;
static lv_obj_t *s_dashboardAgenda = nullptr;
static lv_obj_t *s_dashboardBus = nullptr;
static lv_obj_t *s_dashboardWeatherCards[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_dashboardWeatherDates[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_dashboardWeatherDetails[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_dashboardWeatherIcons[HkoForecastModel::kMaxDays] = {};
static void *s_dashboardWeatherBuffers[HkoForecastModel::kMaxDays] = {};
static lv_obj_t *s_originalOverviewButton = nullptr;
static bool s_dashboardVisible = true;

static void refreshInfoOverlay();
static void refreshWeatherCards();
static void refreshDashboardAgenda();
static void refreshDashboardBus();
static void refreshDashboardWeather();
static void createDashboardView();
static void createWiFiControls();
static void submitWiFiSettings(lv_event_t *event);

static void onMenuPressed(lv_event_t *event)
{
    s_selectedPage = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    if (s_selectedPage == 4) {
        lv_obj_add_flag(s_infoOverlay, LV_OBJ_FLAG_HIDDEN);
        KmbSettingsView_Open();
    } else if (s_selectedPage == 0) {
        KmbSettingsView_Close();
        lv_obj_add_flag(s_infoOverlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        KmbSettingsView_Close();
        lv_obj_clear_flag(s_infoOverlay, LV_OBJ_FLAG_HIDDEN);
        refreshInfoOverlay();
        lv_obj_move_foreground(s_infoOverlay);
    }
    if (s_originalOverviewButton && s_selectedPage != 4)
        lv_obj_move_foreground(s_originalOverviewButton);
}

static void createMenuButton(lv_obj_t *parent, const char *text, uint8_t page)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 194, 48);
    lv_obj_add_event_cb(button, onMenuPressed, LV_EVENT_PRESSED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(page)));
    lv_obj_t *label = lv_label_create(button);
    // Keep the menu labels in explicit UTF-8 so Windows source-code pages
    // cannot turn them into mojibake before they reach LVGL.
    const char *safeText = page == 0 ? "\xE5\xB7\xB4\xE5\xA3\xAB\xE5\x88\xB0\xE7\xAB\x99"
                           : page == 1 ? "\xE4\xBB\x8A\xE6\x97\xA5\xE6\x97\xA5\xE7\xA8\x8B"
                           : page == 2 ? "\xE6\x9C\xAC\xE5\x9C\xB0\xE5\xA4\xA9\xE6\xB0\xA3"
                           : page == 4 ? "\xE5\xB7\xB4\xE5\xA3\xAB\xE8\xA8\xAD\xE5\xAE\x9A"
                                       : "WiFi \xE8\xA8\xAD\xE5\xAE\x9A";
    lv_label_set_text(label, safeText);
    lv_obj_set_style_text_font(label, &ui_font_NSTC28bold, 0);
    lv_obj_center(label);
}

static void createTouchMenu()
{
    // The generated UI contains a full-screen transparent touch catcher for
    // the old slideshow. Hide it so it cannot steal presses from this menu.
    lv_obj_add_flag(ui_ctrTouch, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *bar = lv_obj_create(ui_Main);
    lv_obj_set_size(bar, 800, 56);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(bar, 3, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    createMenuButton(bar, "巴士到站", 0);
    createMenuButton(bar, "今日日程", 1);
    createMenuButton(bar, "今日天氣", 2);
    createMenuButton(bar, "巴士設定", 4);

    s_infoOverlay = lv_obj_create(ui_Main);
    lv_obj_set_size(s_infoOverlay, 800, 424);
    lv_obj_align(s_infoOverlay, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_infoOverlay, lv_color_hex(0xF7F4EC), 0);
    lv_obj_set_style_bg_opa(s_infoOverlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_infoOverlay, LV_OBJ_FLAG_SCROLLABLE);

    s_infoTitle = lv_label_create(s_infoOverlay);
    lv_obj_set_style_text_font(s_infoTitle, &ui_font_NSTC28bold, 0);
    lv_obj_align(s_infoTitle, LV_ALIGN_TOP_LEFT, 24, 16);

    s_infoBody = lv_label_create(s_infoOverlay);
    lv_obj_set_width(s_infoBody, 740);
    lv_obj_set_style_text_font(s_infoBody, &ui_font_NSTC28bold, 0);
    lv_obj_set_style_text_line_space(s_infoBody, 14, 0);
    lv_label_set_long_mode(s_infoBody, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_infoBody, LV_ALIGN_TOP_LEFT, 24, 78);

    s_weatherGrid = lv_obj_create(s_infoOverlay);
    lv_obj_set_size(s_weatherGrid, 752, 292);
    lv_obj_align(s_weatherGrid, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_opa(s_weatherGrid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_weatherGrid, 0, 0);
    lv_obj_set_style_pad_all(s_weatherGrid, 0, 0);
    lv_obj_set_style_pad_column(s_weatherGrid, 5, 0);
    lv_obj_set_flex_flow(s_weatherGrid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_weatherGrid, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_weatherGrid, LV_OBJ_FLAG_SCROLLABLE);
    for (std::size_t index = 0; index < HkoForecastModel::kMaxDays; ++index) {
        s_weatherCards[index] = lv_obj_create(s_weatherGrid);
        lv_obj_set_size(s_weatherCards[index], 102, 276);
        lv_obj_set_style_radius(s_weatherCards[index], 12, 0);
        lv_obj_set_style_border_width(s_weatherCards[index], 0, 0);
        lv_obj_set_style_pad_all(s_weatherCards[index], 5, 0);
        lv_obj_clear_flag(s_weatherCards[index], LV_OBJ_FLAG_SCROLLABLE);

        s_weatherLabels[index] = lv_label_create(s_weatherCards[index]);
        lv_obj_set_width(s_weatherLabels[index], 92);
        lv_label_set_long_mode(s_weatherLabels[index], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(s_weatherLabels[index], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(s_weatherLabels[index], &ui_font_NSTC28bold, 0);
        lv_obj_align(s_weatherLabels[index], LV_ALIGN_TOP_MID, 0, 4);

        s_weatherIconBuffers[index] = heap_caps_malloc(
            LV_CANVAS_BUF_SIZE_TRUE_COLOR(kWeatherIconWidth, kWeatherIconHeight),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_weatherIconBuffers[index]) {
            s_weatherIcons[index] = lv_canvas_create(s_weatherCards[index]);
            lv_canvas_set_buffer(s_weatherIcons[index], s_weatherIconBuffers[index],
                                 kWeatherIconWidth, kWeatherIconHeight,
                                 LV_IMG_CF_TRUE_COLOR);
            lv_obj_align(s_weatherIcons[index], LV_ALIGN_CENTER, 0, -2);
        }

        s_weatherDetails[index] = lv_label_create(s_weatherCards[index]);
        lv_obj_set_width(s_weatherDetails[index], 92);
        lv_label_set_long_mode(s_weatherDetails[index], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(s_weatherDetails[index], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(s_weatherDetails[index], &ui_font_NSTC28bold, 0);
        lv_obj_align(s_weatherDetails[index], LV_ALIGN_BOTTOM_MID, 0, -5);
    }
    lv_obj_add_flag(s_weatherGrid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_infoOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(bar);
}

static void onWiFiFieldFocused(lv_event_t *event)
{
    if (!s_wifiKeyboard) return;
    lv_keyboard_set_textarea(s_wifiKeyboard, static_cast<lv_obj_t *>(lv_event_get_target(event)));
    lv_obj_clear_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
}

static void createWiFiControls()
{
    s_wifiSsid = lv_textarea_create(s_infoOverlay);
    lv_obj_set_size(s_wifiSsid, 680, 48);
    lv_obj_align(s_wifiSsid, LV_ALIGN_TOP_MID, 0, 56);
    lv_textarea_set_one_line(s_wifiSsid, true);
    lv_textarea_set_placeholder_text(s_wifiSsid, "Wi-Fi 名稱（SSID）");
    lv_obj_set_style_text_font(s_wifiSsid, &ui_font_NSTC28bold, 0);
    lv_obj_add_event_cb(s_wifiSsid, onWiFiFieldFocused, LV_EVENT_FOCUSED, nullptr);

    s_wifiPass = lv_textarea_create(s_infoOverlay);
    lv_obj_set_size(s_wifiPass, 680, 48);
    lv_obj_align(s_wifiPass, LV_ALIGN_TOP_MID, 0, 112);
    lv_textarea_set_one_line(s_wifiPass, true);
    lv_textarea_set_password_mode(s_wifiPass, true);
    lv_textarea_set_placeholder_text(s_wifiPass, "Wi-Fi 密碼");
    lv_obj_set_style_text_font(s_wifiPass, &ui_font_NSTC28bold, 0);
    lv_obj_add_event_cb(s_wifiPass, onWiFiFieldFocused, LV_EVENT_FOCUSED, nullptr);

    s_wifiSave = lv_btn_create(s_infoOverlay);
    lv_obj_set_size(s_wifiSave, 300, 48);
    lv_obj_align(s_wifiSave, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_add_event_cb(s_wifiSave, submitWiFiSettings, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *saveLabel = lv_label_create(s_wifiSave);
    lv_label_set_text(saveLabel, "儲存並連線");
    lv_obj_set_style_text_font(saveLabel, &ui_font_NSTC28bold, 0);
    lv_obj_center(saveLabel);

    s_wifiStatus = lv_label_create(s_infoOverlay);
    lv_obj_set_width(s_wifiStatus, 740);
    lv_label_set_long_mode(s_wifiStatus, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_wifiStatus, &ui_font_NSTC28bold, 0);
    lv_obj_align(s_wifiStatus, LV_ALIGN_TOP_LEFT, 30, 228);

    s_wifiKeyboard = lv_keyboard_create(s_infoOverlay);
    lv_obj_set_size(s_wifiKeyboard, 800, 184);
    lv_obj_align(s_wifiKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(s_wifiKeyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiSsid, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiPass, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiSave, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_wifiStatus, LV_OBJ_FLAG_HIDDEN);
}

static void submitWiFiSettings(lv_event_t *)
{
    const char *ssid = lv_textarea_get_text(s_wifiSsid);
    const char *pass = lv_textarea_get_text(s_wifiPass);
    if (!ssid || strlen(ssid) == 0) {
        lv_label_set_text(s_wifiStatus, "請先輸入 Wi-Fi 名稱。");
        return;
    }
    Config cfg = ConfigMgr.getConfig();
    cfg.wifi_ssid = ssid;
    cfg.wifi_pass = pass ? pass : "";
    bool replaced = false;
    for (WiFiCredential &network : cfg.wifi_networks) {
        if (network.ssid == cfg.wifi_ssid) {
            network.password = cfg.wifi_pass;
            replaced = true;
        }
    }
    if (!replaced) {
        if (cfg.wifi_networks.size() == 4) cfg.wifi_networks.pop_back();
        cfg.wifi_networks.insert(cfg.wifi_networks.begin(),
                                 {cfg.wifi_ssid, cfg.wifi_pass});
    }
    ConfigMgr.setConfig(cfg);
    if (!ConfigMgr.save()) {
        lv_label_set_text(s_wifiStatus, "設定儲存失敗，請再試一次。");
        return;
    }
    lv_label_set_text(s_wifiStatus, "正在連線，請稍候……");
    if (s_wifiKeyboard) lv_obj_add_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
    WiFi_Connect();
    if (WiFi_IsConnected()) {
        String message = "已連線：" + WiFi.localIP().toString();
        lv_label_set_text(s_wifiStatus, message.c_str());
    } else {
        lv_label_set_text(s_wifiStatus, "連線失敗；請檢查密碼後再試。");
    }
}

struct LvglGuard {
    LvglGuard()  { lvgl_port_lock(-1); }
    ~LvglGuard() { lvgl_port_unlock(); }
};
#define WITH_LVGL() LvglGuard _lvgl_guard

void Display_Init()
{
    s_board = new Board();
    s_board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = s_board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#  if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto bus = lcd->getBus();
    if (bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 8);
    }
#  endif
#endif
    assert(s_board->begin());

    lvgl_port_init(s_board->getLCD(), s_board->getTouch());

    lvgl_port_lock(-1);
    ui_init();
    // Make the wifi info label scroll horizontally like the Nextion gWifiInfo did.
    //lv_label_set_long_mode(ui_lblWifiInfo, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // SquareLine clears CLICKABLE on ui_ctrTouch (ui_Main.c) so the touch
    // hotspot never receives LV_EVENT_PRESSED. Re-enable here so we don't
    // have to edit a generated file.
    createTouchMenu();
    createWiFiControls();
    KmbSettingsView_Init();
    // Collapse the three SquareLine background widgets into one. The day
    // widget stays and Update_Background() swaps its image source via
    // lv_img_set_src(); the other two are deleted from the scene graph so
    // LVGL doesn't iterate them on every refresh.
    lv_obj_del(ui_imgBackgroundSunset);
    lv_obj_del(ui_imgBackgroundNight);
    ui_imgBackgroundSunset = nullptr;
    ui_imgBackgroundNight  = nullptr;
    createDashboardView();
    lvgl_port_unlock();

    printf("Display initialised (LVGL on Waveshare ESP32-S3-Touch-LCD-7, 800x480)\n");
    Heap_Log("after Display_Init");
}

void Display_Tick(void)
{
    KmbSettingsView_Tick();
}

void Display_ShowWiFiSettings(void)
{
    if (!s_infoOverlay) return;
    WITH_LVGL();
    s_dashboardVisible = false;
    lv_disp_load_scr(ui_Main);
    s_selectedPage = 3;
    lv_obj_clear_flag(s_infoOverlay, LV_OBJ_FLAG_HIDDEN);
    refreshInfoOverlay();
    lv_obj_move_foreground(s_infoOverlay);
    if (s_originalOverviewButton)
        lv_obj_move_foreground(s_originalOverviewButton);
}

bool Display_IsOverviewVisible(void)
{
    return s_dashboardVisible;
}

static std::vector<ScheduleItem> todaysAgenda()
{
    time_t now = time(nullptr);
    struct tm local{};
    localtime_r(&now, &local);
    const Config cfg = ConfigMgr.getConfig();
    return ScheduleModel::forWeekday(cfg.agenda_items, static_cast<uint8_t>(local.tm_wday));
}

static std::string todayIsoDate()
{
    time_t now = time(nullptr);
    struct tm local {};
    localtime_r(&now, &local);
    char date[11];
    snprintf(date, sizeof(date), "%04d-%02d-%02d",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return date;
}

#if 0 // replaced below with UTF-8-safe strings for the 7-inch Chinese menu
static void refreshInfoOverlay()
{
    if (!s_infoOverlay || s_selectedPage == 0) return;
    const bool wifiPage = s_selectedPage == 3;
    if (s_wifiSsid) {
        const Config cfg = ConfigMgr.getConfig();
        if (wifiPage) {
            lv_textarea_set_text(s_wifiSsid, cfg.wifi_ssid.c_str());
            lv_textarea_set_text(s_wifiPass, cfg.wifi_pass.c_str());
            lv_obj_clear_flag(s_wifiSsid, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_wifiPass, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_wifiSave, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_wifiStatus, LV_OBJ_FLAG_HIDDEN);
            if (s_wifiKeyboard) lv_obj_add_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_wifiStatus, WiFi_IsConnected() ? "目前已連線；可更改後重新連線。" : "請輸入家中 Wi-Fi 資料。");
            lv_obj_add_flag(s_infoTitle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_infoBody, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_add_flag(s_wifiSsid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiPass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiSave, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiStatus, LV_OBJ_FLAG_HIDDEN);
        if (s_wifiKeyboard) lv_obj_add_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_infoTitle, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_infoBody, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_selectedPage == 1) {
        const std::string body = ScheduleModel::formatForDisplay(todaysAgenda());
        const std::string safeBody = DashboardModel::sanitizeForDisplay(body);
        lv_label_set_text(s_infoTitle, "今日日程");
        lv_label_set_text(s_infoBody, safeBody.c_str());
        return;
    }
    char weather[160];
    snprintf(weather, sizeof(weather), "%s\n\n氣溫：%.1f°C\n濕度：%u%%\n\n資料會定時自動更新",
             current_weather_desc, current_temperature, current_humidity);
    lv_label_set_text(s_infoTitle, "今日天氣");
    lv_label_set_text(s_infoBody, weather);
}

#endif

static void refreshInfoOverlay()
{
    if (!s_infoOverlay || s_selectedPage == 0) return;
    const bool wifiPage = s_selectedPage == 3;
    if (s_wifiSsid) {
        if (wifiPage) {
            const Config cfg = ConfigMgr.getConfig();
            lv_textarea_set_text(s_wifiSsid, cfg.wifi_ssid.c_str());
            lv_textarea_set_text(s_wifiPass, cfg.wifi_pass.c_str());
            lv_obj_clear_flag(s_wifiSsid, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_wifiPass, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_wifiSave, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_wifiStatus, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(
                s_wifiStatus,
                WiFi_IsConnected()
                    ? "目前已連線；可更改後重新連線。"
                    : "請輸入家中 Wi-Fi 資料。");
            lv_obj_add_flag(s_infoTitle, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_infoBody, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_weatherGrid, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_add_flag(s_wifiSsid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiPass, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiSave, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiStatus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_infoTitle, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_selectedPage == 1) {
        lv_obj_clear_flag(s_infoBody, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_weatherGrid, LV_OBJ_FLAG_HIDDEN);
        std::string body = ScheduleModel::formatForDisplay(todaysAgenda());
        const std::string google = GoogleTaskModel::formatForDisplay(
            GoogleTasks_GetForDate(todayIsoDate()));
        if (!google.empty()) {
            body += "\n\n";
            body += google;
        } else if (GoogleTasks_GetStatus() == GoogleTasksStatus::Error) {
            body += "\n\nGoogle Tasks：暫時無法同步";
        }
        const std::string safeBody = DashboardModel::sanitizeForDisplay(body);
        lv_label_set_text(s_infoTitle, "今日日程");
        lv_label_set_text(s_infoBody, safeBody.c_str());
        return;
    }
    if (s_selectedPage == 2) {
        lv_obj_add_flag(s_infoBody, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_weatherGrid, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_infoTitle, "香港天文台七日預報");
        refreshWeatherCards();
    }
}

static const char *weatherSummary(int icon)
{
    if (icon >= 50 && icon <= 52) return "天晴";
    if (icon >= 53 && icon <= 59) return "多雲";
    if (icon >= 60 && icon <= 69) return "有雨";
    if (icon >= 70 && icon <= 79) return "驟雨";
    if (icon >= 80) return "雷暴";
    return "天氣";
}

static void canvasCircle(lv_obj_t *canvas, int x, int y, int diameter,
                         lv_color_t color)
{
    lv_draw_rect_dsc_t shape;
    lv_draw_rect_dsc_init(&shape);
    shape.bg_color = color;
    shape.bg_opa = LV_OPA_COVER;
    shape.radius = LV_RADIUS_CIRCLE;
    lv_canvas_draw_rect(canvas, x, y, diameter, diameter, &shape);
}

static void canvasRect(lv_obj_t *canvas, int x, int y, int width, int height,
                       int radius, lv_color_t color)
{
    lv_draw_rect_dsc_t shape;
    lv_draw_rect_dsc_init(&shape);
    shape.bg_color = color;
    shape.bg_opa = LV_OPA_COVER;
    shape.radius = radius;
    lv_canvas_draw_rect(canvas, x, y, width, height, &shape);
}

static void canvasLines(lv_obj_t *canvas, const lv_point_t *points,
                        std::size_t count, int width, lv_color_t color)
{
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = color;
    line.width = width;
    line.round_start = true;
    line.round_end = true;
    lv_canvas_draw_line(canvas, points, static_cast<uint32_t>(count), &line);
}

static void drawSun(lv_obj_t *canvas)
{
    const lv_color_t yellow = lv_color_hex(0xFFD447);
    canvasCircle(canvas, 14, 9, 26, yellow);
    const lv_point_t rays[][2] = {
        {{27, 2}, {27, 7}}, {{27, 37}, {27, 43}},
        {{7, 22}, {12, 22}}, {{42, 22}, {48, 22}},
        {{11, 7}, {15, 11}}, {{39, 34}, {43, 38}},
        {{10, 38}, {15, 33}}, {{39, 11}, {43, 7}},
    };
    for (const auto &ray : rays) canvasLines(canvas, ray, 2, 3, yellow);
}

static void drawCloud(lv_obj_t *canvas)
{
    const lv_color_t cloud = lv_color_hex(0xF5F7FA);
    const lv_color_t shade = lv_color_hex(0xB8C5D6);
    canvasCircle(canvas, 14, 28, 26, shade);
    canvasCircle(canvas, 30, 20, 34, cloud);
    canvasCircle(canvas, 49, 30, 20, cloud);
    canvasRect(canvas, 14, 35, 55, 21, 10, cloud);
}

static void drawRain(lv_obj_t *canvas, bool heavy)
{
    const lv_color_t blue = lv_color_hex(0x2878D0);
    const lv_point_t drops[][2] = {
        {{24, 56}, {20, 65}}, {{39, 56}, {35, 65}},
        {{54, 56}, {50, 65}},
    };
    const int count = heavy ? 3 : 2;
    for (int index = 0; index < count; ++index)
        canvasLines(canvas, drops[index], 2, 3, blue);
}

static void drawWeatherIcon(lv_obj_t *canvas, int hkoIcon, uint32_t background)
{
    if (!canvas) return;
    const lv_color_t bg = lv_color_hex(background);
    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);

    const WeatherIconKind kind = HkoForecastModel::iconKind(hkoIcon);
    if (kind == WeatherIconKind::Sun || kind == WeatherIconKind::SunCloud ||
        kind == WeatherIconKind::SunCloudRain)
        drawSun(canvas);

    if (kind == WeatherIconKind::Cloud || kind == WeatherIconKind::Rain ||
        kind == WeatherIconKind::Thunder || kind == WeatherIconKind::Fog ||
        kind == WeatherIconKind::SunCloud || kind == WeatherIconKind::SunCloudRain)
        drawCloud(canvas);

    if (kind == WeatherIconKind::Rain || kind == WeatherIconKind::SunCloudRain)
        drawRain(canvas, hkoIcon >= 63);

    if (kind == WeatherIconKind::Thunder) {
        lv_draw_rect_dsc_t bolt;
        lv_draw_rect_dsc_init(&bolt);
        bolt.bg_color = lv_color_hex(0xFFD447);
        bolt.bg_opa = LV_OPA_COVER;
        const lv_point_t points[] = {
            {37, 51}, {30, 62}, {38, 60}, {34, 68}, {48, 55}, {40, 56}
        };
        lv_canvas_draw_polygon(canvas, points, 6, &bolt);
    }

    if (kind == WeatherIconKind::Moon) {
        canvasCircle(canvas, 20, 10, 42, lv_color_hex(0xFFE58A));
        canvasCircle(canvas, 34, 3, 38, bg);
    }

    if (kind == WeatherIconKind::Wind) {
        const lv_color_t wind = lv_color_hex(0xF5F7FA);
        const lv_point_t lines[][3] = {
            {{8, 20}, {46, 20}, {54, 15}},
            {{16, 34}, {60, 34}, {66, 29}},
            {{7, 48}, {42, 48}, {48, 53}},
        };
        for (const auto &line : lines) canvasLines(canvas, line, 3, 4, wind);
    }

    if (kind == WeatherIconKind::Fog) {
        const lv_color_t fog = lv_color_hex(0x718096);
        const lv_point_t lines[][2] = {
            {{7, 54}, {63, 54}}, {{14, 62}, {57, 62}}
        };
        for (const auto &line : lines) canvasLines(canvas, line, 2, 3, fog);
    }
}

static void refreshWeatherCards()
{
    const std::vector<HkoForecastDay> days = Weather_GetForecast();
    for (std::size_t index = 0; index < HkoForecastModel::kMaxDays; ++index) {
        if (!s_weatherLabels[index]) continue;
        if (index >= days.size()) {
            lv_label_set_text(s_weatherLabels[index], "等待\n資料");
            if (s_weatherDetails[index]) lv_label_set_text(s_weatherDetails[index], "");
            if (s_weatherIcons[index])
                lv_canvas_fill_bg(s_weatherIcons[index], lv_color_hex(0xDDE3E8),
                                  LV_OPA_COVER);
            lv_obj_set_style_bg_color(s_weatherCards[index],
                                      lv_color_hex(0xDDE3E8), 0);
            continue;
        }

        const HkoForecastDay &day = days[index];
        char heading[48];
        char details[64];
        snprintf(heading, sizeof(heading), "%s\n%s",
                 HkoForecastModel::displayDate(day.date).c_str(),
                 day.week.c_str());
        const std::string temperature =
            HkoForecastModel::temperatureRange(day.minTemp, day.maxTemp);
        snprintf(details, sizeof(details), "%s\n%s",
                 weatherSummary(day.icon), temperature.c_str());
        lv_label_set_text(s_weatherLabels[index], heading);
        if (s_weatherDetails[index]) lv_label_set_text(s_weatherDetails[index], details);
        const uint32_t cardColor = HkoForecastModel::cardColor(day.icon);
        lv_obj_set_style_bg_color(
            s_weatherCards[index],
            lv_color_hex(cardColor), 0);
        drawWeatherIcon(s_weatherIcons[index], day.icon, cardColor);
    }
}

static void openOriginalView(lv_event_t *)
{
    if (!ui_Main) return;
    s_dashboardVisible = false;
    lv_disp_load_scr(ui_Main);
    if (s_originalOverviewButton)
        lv_obj_move_foreground(s_originalOverviewButton);
}

static void openDashboardView(lv_event_t *)
{
    if (!s_dashboardScreen) return;
    KmbSettingsView_Close();
    s_dashboardVisible = true;
    refreshDashboardAgenda();
    refreshDashboardBus();
    refreshDashboardWeather();
    lv_disp_load_scr(s_dashboardScreen);
}

static lv_obj_t *createDashboardPanel(lv_obj_t *parent, int x, int y,
                                      int width, int height,
                                      uint32_t color)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

static lv_obj_t *createDashboardLabel(lv_obj_t *parent, const char *text,
                                      int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, &ui_font_NSTC28bold, 0);
    lv_label_set_text(label, text);
    return label;
}

static void createDashboardView()
{
    s_originalOverviewButton = lv_btn_create(ui_Main);
    lv_obj_set_size(s_originalOverviewButton, 92, 44);
    lv_obj_align(s_originalOverviewButton, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_bg_color(s_originalOverviewButton,
                              lv_color_hex(0x20252B), 0);
    lv_obj_add_event_cb(s_originalOverviewButton, openDashboardView,
                        LV_EVENT_CLICKED, nullptr);
    lv_obj_t *returnLabel = lv_label_create(s_originalOverviewButton);
    lv_label_set_text(returnLabel, "總覽");
    lv_obj_set_style_text_font(returnLabel, &ui_font_NSTC28bold, 0);
    lv_obj_center(returnLabel);

    s_dashboardScreen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_dashboardScreen, lv_color_hex(0xEAF0F4), 0);
    lv_obj_set_style_bg_opa(s_dashboardScreen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_dashboardScreen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *agendaPanel = createDashboardPanel(
        s_dashboardScreen, 8, 8, 382, 220, 0xFFF7E0);
    lv_obj_t *agendaTitle = createDashboardLabel(agendaPanel, "今日待辦", 350);
    lv_obj_align(agendaTitle, LV_ALIGN_TOP_LEFT, 2, 0);
    s_dashboardAgenda = createDashboardLabel(agendaPanel, "正在載入...", 350);
    lv_obj_set_style_text_line_space(s_dashboardAgenda, 5, 0);
    lv_obj_align(s_dashboardAgenda, LV_ALIGN_TOP_LEFT, 2, 42);

    lv_obj_t *busPanel = createDashboardPanel(
        s_dashboardScreen, 398, 8, 394, 220, 0xE4F2FF);
    lv_obj_t *busTitle = createDashboardLabel(busPanel, "巴士到站", 260);
    lv_obj_align(busTitle, LV_ALIGN_TOP_LEFT, 2, 0);
    s_dashboardBus = createDashboardLabel(busPanel, "正在載入...", 366);
    lv_obj_set_style_text_line_space(s_dashboardBus, 4, 0);
    lv_obj_align(s_dashboardBus, LV_ALIGN_TOP_LEFT, 2, 44);

    lv_obj_t *originalButton = lv_btn_create(busPanel);
    lv_obj_set_size(originalButton, 88, 42);
    lv_obj_align(originalButton, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_style_bg_color(originalButton, lv_color_hex(0x20252B), 0);
    lv_obj_add_event_cb(originalButton, openOriginalView, LV_EVENT_CLICKED,
                        nullptr);
    lv_obj_t *originalLabel = lv_label_create(originalButton);
    lv_label_set_text(originalLabel, "原版");
    lv_obj_set_style_text_font(originalLabel, &ui_font_NSTC28bold, 0);
    lv_obj_center(originalLabel);

    lv_obj_t *weatherPanel = createDashboardPanel(
        s_dashboardScreen, 8, 236, 784, 236, 0xFFFFFF);

    lv_obj_t *weatherRow = lv_obj_create(weatherPanel);
    lv_obj_set_size(weatherRow, 764, 216);
    lv_obj_center(weatherRow);
    lv_obj_set_style_bg_opa(weatherRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(weatherRow, 0, 0);
    lv_obj_set_style_pad_all(weatherRow, 0, 0);
    lv_obj_set_style_pad_column(weatherRow, 4, 0);
    lv_obj_set_flex_flow(weatherRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weatherRow, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(weatherRow, LV_OBJ_FLAG_SCROLLABLE);

    for (std::size_t index = 0; index < HkoForecastModel::kMaxDays; ++index) {
        lv_obj_t *card = createDashboardPanel(weatherRow, 0, 0, 104, 212,
                                              0xDDE3E8);
        lv_obj_set_style_pad_all(card, 4, 0);
        s_dashboardWeatherCards[index] = card;

        s_dashboardWeatherDates[index] = createDashboardLabel(card, "等待", 94);
        lv_obj_set_style_text_align(s_dashboardWeatherDates[index],
                                    LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_dashboardWeatherDates[index], LV_ALIGN_TOP_MID, 0, 0);

        s_dashboardWeatherBuffers[index] = heap_caps_malloc(
            LV_CANVAS_BUF_SIZE_TRUE_COLOR(kWeatherIconWidth, kWeatherIconHeight),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_dashboardWeatherBuffers[index]) {
            s_dashboardWeatherIcons[index] = lv_canvas_create(card);
            lv_canvas_set_buffer(s_dashboardWeatherIcons[index],
                                 s_dashboardWeatherBuffers[index],
                                 kWeatherIconWidth, kWeatherIconHeight,
                                 LV_IMG_CF_TRUE_COLOR);
            lv_obj_align(s_dashboardWeatherIcons[index], LV_ALIGN_CENTER, 0, -1);
        }

        s_dashboardWeatherDetails[index] = createDashboardLabel(card, "資料", 94);
        lv_obj_set_style_text_align(s_dashboardWeatherDetails[index],
                                    LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_dashboardWeatherDetails[index], LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    refreshDashboardAgenda();
    refreshDashboardBus();
    refreshDashboardWeather();
    s_dashboardVisible = true;
    lv_disp_load_scr(s_dashboardScreen);
}

static void refreshDashboardAgenda()
{
    if (!s_dashboardAgenda) return;
    const std::string agenda = DashboardModel::sanitizeForDisplay(
        ScheduleModel::formatForDisplay(todaysAgenda(), 4));
    lv_label_set_text(s_dashboardAgenda, agenda.c_str());
}

static void refreshDashboardBus()
{
    if (!s_dashboardBus) return;
    std::vector<DashboardBusItem> snapshot;
    BusData_Lock();
    snapshot.reserve(displayRoutes.size());
    for (const BusInfo &route : displayRoutes) {
        snapshot.push_back({route.route, route.destination,
                            route.etaDisplay1, route.etaDisplay2});
    }
    BusData_Unlock();

    const std::vector<DashboardBusItem> buses =
        DashboardModel::visibleBuses(snapshot);
    if (buses.empty()) {
        lv_label_set_text(s_dashboardBus, DashboardModel::emptyBusMessage());
        return;
    }
    std::string body;
    for (const DashboardBusItem &bus : buses) {
        if (!body.empty()) body += "\n";
        body += DashboardModel::formatBusEta(bus);
    }
    lv_label_set_text(s_dashboardBus, body.c_str());
}

static void refreshDashboardWeather()
{
    const std::vector<HkoForecastDay> days = Weather_GetForecast();
    for (std::size_t index = 0; index < HkoForecastModel::kMaxDays; ++index) {
        if (!s_dashboardWeatherDates[index]) continue;
        if (index >= days.size()) {
            lv_label_set_text(s_dashboardWeatherDates[index], "等待");
            lv_label_set_text(s_dashboardWeatherDetails[index], "資料");
            lv_obj_set_style_bg_color(s_dashboardWeatherCards[index],
                                      lv_color_hex(0xDDE3E8), 0);
            if (s_dashboardWeatherIcons[index])
                lv_canvas_fill_bg(s_dashboardWeatherIcons[index],
                                  lv_color_hex(0xDDE3E8), LV_OPA_COVER);
            continue;
        }
        const HkoForecastDay &day = days[index];
        const std::string date = HkoForecastModel::displayDate(day.date);
        char details[48];
        snprintf(details, sizeof(details), "%s\n%d-%d度",
                 weatherSummary(day.icon), day.minTemp, day.maxTemp);
        lv_label_set_text(s_dashboardWeatherDates[index], date.c_str());
        lv_label_set_text(s_dashboardWeatherDetails[index], details);
        const uint32_t color = HkoForecastModel::cardColor(day.icon);
        lv_obj_set_style_bg_color(s_dashboardWeatherCards[index],
                                  lv_color_hex(color), 0);
        drawWeatherIcon(s_dashboardWeatherIcons[index], day.icon, color);
    }
}

void Update_Schedule_Display(void)
{
    WITH_LVGL();
    refreshDashboardAgenda();
    if (s_selectedPage == 1) refreshInfoOverlay();
}

void Display_BacklightOff(void)
{
    if (!s_board) return;
    auto bl = s_board->getBacklight();
    if (bl) bl->off();
}

// ============================================================================
// Time / date
// ============================================================================
void Update_Time()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    char s[6];
    snprintf(s, sizeof(s), "%02d:%02d", t.tm_hour, t.tm_min);

    // HH:MM only changes once per minute; skip ~59 of every 60 calls.
    static char last[6] = "";
    if (strcmp(s, last) == 0) return;
    strcpy(last, s);
    WITH_LVGL();
    lv_label_set_text(ui_lblNowTime, s);
}

void Update_Date_And_Weekday()
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    static const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    char s[40];
    snprintf(s, sizeof(s), "%04d年%02d月%02d日(%s)",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, weekdays[t.tm_wday]);

    // Date string only changes once per day.
    static char last[40] = "";
    if (strcmp(s, last) == 0) return;
    strcpy(last, s);

    WITH_LVGL();
    lv_label_set_text(ui_lblNowDate, s);
}

// ============================================================================
// Weather
// ============================================================================
void Update_Weather()
{
    char temp[16];
    snprintf(temp, sizeof(temp), "%.1f", current_temperature);

    static char lastTemp[16] = "";
    static char lastDesc[64] = "";
    bool tempChanged = strcmp(temp, lastTemp) != 0;
    bool descChanged = strcmp(current_weather_desc, lastDesc) != 0;
    if (tempChanged) strcpy(lastTemp, temp);
    if (descChanged) {
        strncpy(lastDesc, current_weather_desc, sizeof(lastDesc) - 1);
        lastDesc[sizeof(lastDesc) - 1] = '\0';
    }
    if (tempChanged || descChanged)
        printf("Weather updated → %s, %.1f°C\n", current_weather_desc, current_temperature);
    WITH_LVGL();
    if (tempChanged) lv_label_set_text(ui_lblTemp, temp);
    if (descChanged) lv_label_set_text(ui_lblWeather, current_weather_desc);
    refreshDashboardWeather();
    if (s_selectedPage == 2) refreshInfoOverlay();
}

// ============================================================================
// Holiday — logic ported from former Nextion.cpp:27-101
// ============================================================================
void Update_Holiday_Display()
{
    char info[96]        = "";
    char dateDisplay[40] = "-";
    String nextHolidayName;
    int minDays = 999;

    if (holidayDoc["holidays"].isNull()) {
        snprintf(info, sizeof(info), "公眾假期資料載入失敗");
        printf("Holiday data not loaded - using fallback\n");
    } else if (time(nullptr) < 1577836800) {
        // Wall clock not synced (system still on 1970 epoch). Without this
        // guard, every 2026 holiday is ~20 000 days away → no `daysLeft`
        // beats the sentinel, `minDays` stays at 999, and the label reads
        // "還有999天". The networkTask re-invokes us once SNTP delivers
        // a real timestamp.
        snprintf(info, sizeof(info), "等待網絡時間同步...");
        printf("Holiday display deferred — wall clock not synced\n");
    } else {
        time_t now = time(nullptr);
        struct tm today;
        localtime_r(&now, &today);
        // Normalise today to local midnight so the diff against holiday-midnight
        // produces a whole-day count instead of fractional hours rounded toward
        // zero. Without this, "today 14:30 vs holiday tomorrow 00:00" yields
        // 9.5 h / 86400 = 0 days and `今天是X` would fire one day early.
        today.tm_hour = 0;
        today.tm_min  = 0;
        today.tm_sec  = 0;
        time_t todaySeconds = mktime(&today);

        String nextHolidayDate;

        JsonArray holidays = holidayDoc["holidays"].as<JsonArray>();
        for (JsonObject h : holidays) {
            const char *dateStr = h["date"]    | "";
            const char *name    = h["name_tc"] | "";

            struct tm holidayTm = {};
            sscanf(dateStr, "%4d-%2d-%2d", &holidayTm.tm_year, &holidayTm.tm_mon, &holidayTm.tm_mday);
            holidayTm.tm_year -= 1900;
            holidayTm.tm_mon  -= 1;
            time_t holidaySeconds = mktime(&holidayTm);

            int daysLeft = (int)difftime(holidaySeconds, todaySeconds) / (60 * 60 * 24);
            if (daysLeft >= 0 && daysLeft < minDays) {
                minDays = daysLeft;
                nextHolidayName = name;
                nextHolidayDate = dateStr;
            }
        }

        if (minDays == 0) {
            snprintf(info, sizeof(info), "今天是%s", nextHolidayName.c_str());
        } else if (minDays == 1) {
            snprintf(info, sizeof(info), "明天是%s", nextHolidayName.c_str());
        } else {
            snprintf(info, sizeof(info), "%s還有%d天", nextHolidayName.c_str(), minDays);
        }

        if (nextHolidayDate.length() > 0) {
            struct tm nextDate = {};
            sscanf(nextHolidayDate.c_str(), "%4d-%2d-%2d",
                   &nextDate.tm_year, &nextDate.tm_mon, &nextDate.tm_mday);
            nextDate.tm_year -= 1900;
            nextDate.tm_mon  -= 1;
            mktime(&nextDate);
            static const char *weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
            snprintf(dateDisplay, sizeof(dateDisplay), "%04d年%02d月%02d日(%s)",
                     nextDate.tm_year + 1900,
                     nextDate.tm_mon + 1,
                     nextDate.tm_mday,
                     weekdays[nextDate.tm_wday]);
        }
    }

    static char lastInfo[96] = "";
    static char lastDate[40] = "";
    bool infoChanged = strcmp(info, lastInfo) != 0;
    bool dateChanged = strcmp(dateDisplay, lastDate) != 0;
    if (!infoChanged && !dateChanged) return;
    if (infoChanged) strcpy(lastInfo, info);
    if (dateChanged) strcpy(lastDate, dateDisplay);

    {
        WITH_LVGL();
        if (infoChanged) lv_label_set_text(ui_lblHolidayInfo, info);
        if (dateChanged) lv_label_set_text(ui_lblHolidayDate, dateDisplay);
    }
    printf("Holiday updated → %s 還有 %d 天\n", nextHolidayName.c_str(), minDays);
}

// ============================================================================
// Bus list (4 slots per page, 2 ETA columns each)
// ============================================================================
void Update_Bus_List()
{
    lv_obj_t *route[4] = { ui_lblRoute1, ui_lblRoute2, ui_lblRoute3, ui_lblRoute4 };
    lv_obj_t *eta1[4]  = { ui_lblETA11,  ui_lblETA21,  ui_lblETA31,  ui_lblETA41 };
    lv_obj_t *eta2[4]  = { ui_lblETA12,  ui_lblETA22,  ui_lblETA32,  ui_lblETA42 };
    lv_obj_t *dest[4]  = { ui_lblDest1,  ui_lblDest2,  ui_lblDest3,  ui_lblDest4 };

    // Format the per-slot log lines into stack buffers while locked, then
    // print AFTER releasing both locks. USB-CDC printf takes 1-10 ms each
    // and previously held WITH_LVGL for ~50-200 ms, blocking the LVGL render
    // task and triggering a contention burst when the lock finally released.
    char lines[4][128];
    int  lineCount = 0;
    int  page      = 0;
    int  total     = 0;

    {
        // Lock order rule: WITH_LVGL first, then BusData_Lock — never the reverse.
        WITH_LVGL();
        BusData_Lock();
        page      = currentPage;
        int start = page * 4;
        total     = (int)displayRoutes.size();
        for (int s = 0; s < 4; s++) {
            int i = start + s;
            bool has = (i < total);
            const char *r  = has ? displayRoutes[i].route        : "";
            const char *e1 = has ? displayRoutes[i].etaDisplay1  : "";
            const char *e2 = has ? displayRoutes[i].etaDisplay2  : "";
            const char *d  = has ? displayRoutes[i].destination  : "";
            // Prepend "往 " (UTF-8 E5 BE 80 + space) only when the destination
            // is non-empty; empty stays empty (no orphan "往 ").
            char destBuf[96];
            const char *destText = "";
            if (total == 0 && s == 0) {
                destText = "尚未設定九巴收藏";
            } else if (d && d[0]) {
                snprintf(destBuf, sizeof(destBuf), "往 %s", d);
                destText = destBuf;
            }
            lv_label_set_text(route[s], r);
            lv_label_set_text(eta1[s],  e1);
            lv_label_set_text(eta2[s],  e2);
            lv_label_set_text(dest[s],  destText);
            if (has) {
                snprintf(lines[lineCount], sizeof(lines[lineCount]),
                         "Displayed Route %d | %s | %s | %s | %s",
                         s + 1, r, e1, e2, destText);
                lineCount++;
            }
        }
        BusData_Unlock();
    }

    for (int i = 0; i < lineCount; i++) {
        printf("%s\n", lines[i]);
    }
    {
        WITH_LVGL();
        refreshDashboardBus();
    }
    printf("Bus list updated (Page %d, Total: %d routes)\n", page, total);
}

// ============================================================================
// Background switch by time of day:
//   06:00 - 16:59 -> Day
//   17:00 - 18:29 -> Sunset
//   18:30 - 05:59 -> Night
// ============================================================================
void Update_Background()
{
    if (!Storage_Begin()) return;

    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    int minutes = t.tm_hour * 60 + t.tm_min;
    int pic;
    if (minutes >= 6 * 60 && minutes <= 16 * 60 + 59)
        pic = 0;
    else if (minutes >= 17 * 60 && minutes <= 18 * 60 + 29)
        pic = 1;
    else
        pic = 2;

    static int lastPic = -1;
    if (pic == lastPic) return;

    static uint8_t *pixels = nullptr;
    static lv_img_dsc_t image = {};
    constexpr size_t imageBytes = 483U * 480U * 2U;
    const char *path = (pic == 0) ? "/assets/background-day.rgb565"
                     : (pic == 1) ? "/assets/background-sunset.rgb565"
                                  : "/assets/background-night.rgb565";

    if (!pixels)
    {
        pixels = static_cast<uint8_t *>(
            heap_caps_malloc(imageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!pixels)
        {
            printf("Background PSRAM allocation failed\n");
            return;
        }
        image.header.always_zero = 0;
        image.header.w = 483;
        image.header.h = 480;
        image.header.cf = LV_IMG_CF_TRUE_COLOR;
        image.data_size = imageBytes;
        image.data = pixels;
    }

    File file = LittleFS.open(path, "r");
    if (!file || file.size() != imageBytes)
    {
        printf("Background asset unavailable: %s (%u bytes)\n",
               path, file ? (unsigned)file.size() : 0U);
        if (file) file.close();
        return;
    }

    {
        // Prevent LVGL from reading the PSRAM image while its pixels change.
        WITH_LVGL();
        size_t read = file.readBytes(reinterpret_cast<char *>(pixels), imageBytes);
        if (read != imageBytes)
        {
            printf("Background asset short read: %s (%u/%u)\n", path,
                   (unsigned)read, (unsigned)imageBytes);
            file.close();
            return;
        }
        lv_img_set_src(ui_imgBackgroundDay, &image);
        lv_obj_invalidate(ui_imgBackgroundDay);
    }
    file.close();
    lastPic = pic;
    printf("Background switched to pic=%d\n", pic);
}

// ============================================================================
// Wi-Fi info banner (replaces Nextion gWifiInfo scrolling text)
// ============================================================================
// Cycle state for the wifi-info banner. The label is too narrow for the full
// "AP:<ssid> P:<pwd> IP:<ip>" string and we don't want the scrolling animation
// (PSRAM-bandwidth contention with the LCD EDMA). Instead we rotate through
// short frames; the loop calls Cycle_Wifi_Info() every few seconds to advance.
static int  s_wifiFrame          = 0;
static char s_wifiLastText[80]   = "";

static void paintWifiFrame(int frameIndex)
{
    char buf[80];
    GetWiFiInfoFrame(frameIndex, buf, sizeof(buf));
    if (strcmp(buf, s_wifiLastText) == 0) return;
    strncpy(s_wifiLastText, buf, sizeof(s_wifiLastText) - 1);
    s_wifiLastText[sizeof(s_wifiLastText) - 1] = '\0';

    WITH_LVGL();
    lv_label_set_text(ui_lblWifiInfo, buf);
}

void ShowWifiInfo()
{
    s_wifiFrame       = 0;
    s_wifiLastText[0] = '\0';   // force first paint via cache miss

    char buf[80];
    GetWiFiInfoFrame(0, buf, sizeof(buf));
    strncpy(s_wifiLastText, buf, sizeof(s_wifiLastText) - 1);
    s_wifiLastText[sizeof(s_wifiLastText) - 1] = '\0';

    WITH_LVGL();
    lv_label_set_text(ui_lblWifiInfo, buf);
    // Translucent backdrop (alpha 150/255) so the SSID/IP stays readable
    // against any background image.
    lv_obj_set_style_bg_opa(ui_lblWifiInfo, 255, (uint32_t)LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_lblWifiInfo, LV_OBJ_FLAG_HIDDEN);
}

void Cycle_Wifi_Info()
{
    int count = GetWiFiInfoFrameCount();
    if (count <= 0) return;
    s_wifiFrame = (s_wifiFrame + 1) % count;
    paintWifiFrame(s_wifiFrame);
}

void HideWifiInfo()
{
    {
        WITH_LVGL();
        lv_label_set_text(ui_lblWifiInfo, "");
        lv_obj_set_style_bg_opa(ui_lblWifiInfo, 0, (uint32_t)LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(ui_lblWifiInfo, LV_OBJ_FLAG_HIDDEN);
    }
    // Logged outside the LVGL guard so we measure the heap *after* any
    // simple-layer buffer (LV_LAYER_SIMPLE_BUF_SIZE) has been freed.
    Heap_Log("after HideWifiInfo");
}

// ============================================================================
// Touch callback bridge — invoked from src/ui/ui_events.c::NextPage().
// Runs inside the LVGL task; recursive mutex makes Update_Bus_List safe.
// ============================================================================
extern "C" void OnNextPagePressed(void)
{
    Switch_To_Next_Page();
    Update_Bus_List();
}
