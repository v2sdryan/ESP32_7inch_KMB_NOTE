#include "KmbSettingsView.h"

#include <Arduino.h>
#include <lvgl.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "DashboardModel.h"
#include "KmbCatalogService.h"
#include "KmbFavoriteModel.h"
#include "KmbFavoriteRepository.h"
#include "lvgl_v8_port.h"
#include "ui/ui.h"

namespace {

constexpr uint32_t kKmbRed = 0xE2231A;
constexpr uint32_t kInk = 0x15202B;
constexpr uint32_t kCoolPanel = 0xF4F7FA;
constexpr uint32_t kAmber = 0xF5A623;

enum class ViewMode {
  List,
  Route,
  Directions,
  Stops,
  Error,
};

enum class RetryKind {
  None,
  Directions,
  Stops,
};

lv_obj_t *s_root = nullptr;
lv_obj_t *s_title = nullptr;
lv_obj_t *s_back = nullptr;
lv_obj_t *s_add = nullptr;
lv_obj_t *s_content = nullptr;
lv_obj_t *s_routeInput = nullptr;
bool s_open = false;
ViewMode s_mode = ViewMode::List;
RetryKind s_retryKind = RetryKind::None;
uint32_t s_seenGeneration = 0;
std::string s_route;
KmbDirectionOption s_direction;
std::vector<KmbDirectionOption> s_directions;
std::vector<KmbStopOption> s_stops;
constexpr std::size_t kStopsPerPage = 5;
std::size_t s_stopPage = 0;
constexpr std::size_t kNoEdit = std::numeric_limits<std::size_t>::max();
std::size_t s_editIndex = kNoEdit;

void showList();
void showRouteEntry();
void showLoading(const char *message);
void showError(const char *message, bool canRetry);
void showStopPage();

lv_obj_t *makeButton(lv_obj_t *parent,
                     const char *text,
                     int width,
                     int height,
                     lv_event_cb_t callback,
                     void *userData = nullptr,
                     uint32_t color = kKmbRed)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_radius(button, 8, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, userData);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &ui_font_NSTC28bold, 0);
  lv_obj_center(label);
  return button;
}

void setHeader(const char *title, bool showAdd)
{
  lv_label_set_text(s_title, title);
  if (showAdd)
    lv_obj_clear_flag(s_add, LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(s_add, LV_OBJ_FLAG_HIDDEN);
}

void onBack(lv_event_t *)
{
  if (s_mode == ViewMode::List)
    KmbSettingsView_Close();
  else
    showList();
}

void onAdd(lv_event_t *)
{
  if (KmbFavorites.list().size() >= KmbFavoriteModel::kMaxFavorites)
  {
    showError("最多只可收藏八個車站", false);
    return;
  }
  s_editIndex = kNoEdit;
  showRouteEntry();
}

void onEditStop(lv_event_t *event)
{
  const std::size_t index = reinterpret_cast<std::uintptr_t>(
      lv_event_get_user_data(event));
  const auto &favorites = KmbFavorites.list();
  if (index >= favorites.size())
    return;

  const KmbFavorite favorite = favorites[index];
  s_editIndex = index;
  s_route = favorite.route;
  s_direction = {favorite.direction, favorite.serviceType,
                 favorite.destinationTc};
  s_retryKind = RetryKind::Stops;
  if (!KmbCatalog.requestStops(s_route, s_direction.direction,
                               s_direction.serviceType))
  {
    showError("系統忙碌，請稍後再試", true);
    return;
  }
  s_mode = ViewMode::Stops;
  setHeader("選擇新車站", false);
  showLoading("正在載入車站…");
}

void onRemove(lv_event_t *event)
{
  const std::size_t index = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event));
  String error;
  if (!KmbFavorites.remove(index, error))
    showError(error.c_str(), false);
  else
    showList();
}

void onMoveUp(lv_event_t *event)
{
  const std::size_t index = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event));
  String error;
  if (KmbFavorites.moveUp(index, error))
    showList();
}

void onMoveDown(lv_event_t *event)
{
  const std::size_t index = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event));
  String error;
  if (KmbFavorites.moveDown(index, error))
    showList();
}

void showList()
{
  s_mode = ViewMode::List;
  s_retryKind = RetryKind::None;
  setHeader("九巴收藏設定", true);
  lv_obj_clean(s_content);
  lv_obj_add_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_content, 8, 0);

  const auto &favorites = KmbFavorites.list();
  if (favorites.empty())
  {
    lv_obj_t *empty = lv_label_create(s_content);
    lv_label_set_text(empty, "尚未設定九巴收藏\n請按「＋新增路線」開始");
    lv_obj_set_style_text_font(empty, &ui_font_NSTC28bold, 0);
    lv_obj_set_style_text_color(empty, lv_color_hex(kInk), 0);
    lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(empty, 100, 0);
    return;
  }

  for (std::size_t index = 0; index < favorites.size(); ++index)
  {
    const auto &favorite = favorites[index];
    lv_obj_t *row = lv_obj_create(s_content);
    lv_obj_set_size(row, 748, 78);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(row, lv_color_white(), 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0xD9E1E8), 0);
    lv_obj_set_style_radius(row, 10, 0);

    lv_obj_t *badge = lv_label_create(row);
    lv_label_set_text(badge, favorite.route.c_str());
    lv_obj_set_size(badge, 94, 50);
    lv_obj_set_style_bg_color(badge, lv_color_hex(kKmbRed), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 8, 0);
    lv_obj_set_style_text_color(badge, lv_color_white(), 0);
    lv_obj_set_style_text_font(badge, &ui_font_NSTC28bold, 0);
    lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(badge, 8, 0);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 2, 0);

    std::string details = DashboardModel::sanitizeForDisplay(
        "往 " + favorite.destinationTc + "\n" + favorite.stopNameTc);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, details.c_str());
    lv_obj_set_width(label, 430);
    lv_obj_set_style_text_font(label, &ui_font_NSTC28bold, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(kInk), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 110, 0);

    const auto userData = reinterpret_cast<void *>(static_cast<std::uintptr_t>(index));
    lv_obj_t *edit = makeButton(row, "改站", 84, 44, onEditStop, userData, kInk);
    lv_obj_align(edit, LV_ALIGN_RIGHT_MID, -92, 0);
    lv_obj_t *remove = makeButton(row, "刪除", 84, 44, onRemove, userData);
    lv_obj_align(remove, LV_ALIGN_RIGHT_MID, -2, 0);
  }
}

void onRouteCancel(lv_event_t *)
{
  showList();
}

void requestDirections()
{
  if (!s_routeInput)
    return;
  s_route = KmbFavoriteModel::normalizeRoute(lv_textarea_get_text(s_routeInput));
  if (s_route.empty() || s_route.size() > 6)
  {
    showError("請輸入有效九巴路線", false);
    return;
  }
  s_retryKind = RetryKind::Directions;
  if (!KmbCatalog.requestDirections(s_route))
  {
    showError("系統忙碌，請稍後再試", true);
    return;
  }
  showLoading("正在查詢九巴路線…");
}

void onRouteNext(lv_event_t *)
{
  requestDirections();
}

void showRouteEntry()
{
  s_mode = ViewMode::Route;
  setHeader("1/3 輸入路線", false);
  lv_obj_clean(s_content);
  lv_obj_set_layout(s_content, 0);

  s_routeInput = lv_textarea_create(s_content);
  lv_obj_set_size(s_routeInput, 520, 58);
  lv_obj_align(s_routeInput, LV_ALIGN_TOP_MID, 0, 20);
  lv_textarea_set_one_line(s_routeInput, true);
  lv_textarea_set_max_length(s_routeInput, 6);
  lv_textarea_set_accepted_chars(s_routeInput, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ");
  lv_textarea_set_placeholder_text(s_routeInput, "例如：93A");
  lv_obj_set_style_text_font(s_routeInput, &ui_font_NSTC28bold, 0);

  lv_obj_t *cancel = makeButton(s_content, "取消", 180, 52, onRouteCancel, nullptr, kInk);
  lv_obj_align(cancel, LV_ALIGN_TOP_LEFT, 170, 90);
  lv_obj_t *next = makeButton(s_content, "下一步", 180, 52, onRouteNext);
  lv_obj_align(next, LV_ALIGN_TOP_RIGHT, -170, 90);

  lv_obj_t *keyboard = lv_keyboard_create(s_content);
  lv_obj_set_size(keyboard, 800, 250);
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER);
  lv_keyboard_set_textarea(keyboard, s_routeInput);
  lv_group_focus_obj(s_routeInput);
}

void showLoading(const char *message)
{
  lv_obj_clean(s_content);
  lv_obj_set_layout(s_content, 0);
  lv_obj_t *label = lv_label_create(s_content);
  lv_label_set_text(label, message);
  lv_obj_set_style_text_font(label, &ui_font_NSTC28bold, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(kInk), 0);
  lv_obj_center(label);
}

void onDirectionSelected(lv_event_t *event)
{
  const std::size_t index = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event));
  if (index >= s_directions.size())
    return;
  s_direction = s_directions[index];
  s_retryKind = RetryKind::Stops;
  if (!KmbCatalog.requestStops(s_route, s_direction.direction,
                               s_direction.serviceType))
  {
    showError("系統忙碌，請稍後再試", true);
    return;
  }
  s_mode = ViewMode::Stops;
  setHeader("3/3 選擇車站", false);
  showLoading("正在載入車站…");
}

void showDirections()
{
  s_mode = ViewMode::Directions;
  setHeader("2/3 選擇方向", false);
  lv_obj_clean(s_content);
  lv_obj_add_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(s_content, 10, 0);
  for (std::size_t index = 0; index < s_directions.size(); ++index)
  {
    const std::string text = DashboardModel::sanitizeForDisplay(
        "往 " + s_directions[index].destinationTc);
    lv_obj_t *button = makeButton(
        s_content, text.c_str(), 700, 62, onDirectionSelected,
        reinterpret_cast<void *>(static_cast<std::uintptr_t>(index)), kInk);
    lv_obj_set_style_text_align(button, LV_TEXT_ALIGN_LEFT, 0);
  }
}

void onStopSelected(lv_event_t *event)
{
  const std::size_t index = reinterpret_cast<std::uintptr_t>(lv_event_get_user_data(event));
  if (index >= s_stops.size())
    return;
  printf("KMB stop selected: route=%s index=%u stop=%s\n", s_route.c_str(),
         static_cast<unsigned>(index), s_stops[index].stopId.c_str());
  lv_obj_add_state(static_cast<lv_obj_t *>(lv_event_get_target(event)),
                   LV_STATE_DISABLED);
  const auto &stop = s_stops[index];
  KmbFavorite favorite{s_route,
                       s_direction.direction,
                       s_direction.serviceType,
                       s_direction.destinationTc,
                       stop.stopId,
                       stop.stopNameTc};
  String error;
  const bool saved = s_editIndex == kNoEdit
                         ? KmbFavorites.add(favorite, error)
                         : KmbFavorites.replace(s_editIndex, favorite, error);
  if (!saved)
    showError(error.c_str(), false);
  else
  {
    s_editIndex = kNoEdit;
    showList();
  }
}

void onPreviousStopPage(lv_event_t *)
{
  if (s_stopPage > 0) {
    --s_stopPage;
    showStopPage();
  }
}

void onNextStopPage(lv_event_t *)
{
  const std::size_t pageCount =
      (s_stops.size() + kStopsPerPage - 1) / kStopsPerPage;
  if (s_stopPage + 1 < pageCount) {
    ++s_stopPage;
    showStopPage();
  }
}

void showStopPage()
{
  const std::size_t pageCount =
      (s_stops.size() + kStopsPerPage - 1) / kStopsPerPage;
  if (pageCount == 0) {
    showError("未能載入車站資料", true);
    return;
  }
  if (s_stopPage >= pageCount) s_stopPage = pageCount - 1;
  const std::string title = "3/3 選擇車站 " +
                            std::to_string(s_stopPage + 1) + "/" +
                            std::to_string(pageCount);
  setHeader(title.c_str(), false);
  lv_obj_clean(s_content);
  lv_obj_set_layout(s_content, 0);
  lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

  const std::size_t first = s_stopPage * kStopsPerPage;
  const std::size_t last = std::min(first + kStopsPerPage, s_stops.size());
  for (std::size_t index = first; index < last; ++index)
  {
    const std::string text = DashboardModel::sanitizeForDisplay(
        std::to_string(s_stops[index].sequence) + ". " +
        s_stops[index].stopNameTc);
    lv_obj_t *button = makeButton(
        s_content, text.c_str(), 720, 54, onStopSelected,
        reinterpret_cast<void *>(static_cast<std::uintptr_t>(index)), kInk);
    lv_obj_align(button, LV_ALIGN_TOP_MID, 0,
                 static_cast<int>((index - first) * 60));
  }

  lv_obj_t *previous = makeButton(s_content, "上一頁", 170, 48,
                                  onPreviousStopPage, nullptr, kInk);
  lv_obj_align(previous, LV_ALIGN_BOTTOM_LEFT, 80, 0);
  if (s_stopPage == 0) lv_obj_add_state(previous, LV_STATE_DISABLED);
  lv_obj_t *next = makeButton(s_content, "下一頁", 170, 48,
                              onNextStopPage, nullptr, kKmbRed);
  lv_obj_align(next, LV_ALIGN_BOTTOM_RIGHT, -80, 0);
  if (s_stopPage + 1 >= pageCount) lv_obj_add_state(next, LV_STATE_DISABLED);
}

void showStops()
{
  s_mode = ViewMode::Stops;
  s_stopPage = 0;
  showStopPage();
}

void retryRequest()
{
  bool accepted = false;
  if (s_retryKind == RetryKind::Directions)
    accepted = KmbCatalog.requestDirections(s_route);
  else if (s_retryKind == RetryKind::Stops)
    accepted = KmbCatalog.requestStops(s_route, s_direction.direction,
                                       s_direction.serviceType);
  if (!accepted)
  {
    showError("系統忙碌，請稍後再試", true);
    return;
  }
  showLoading(s_retryKind == RetryKind::Directions
                  ? "正在查詢九巴路線…"
                  : "正在載入車站…");
}

void onRetry(lv_event_t *)
{
  retryRequest();
}

void onErrorBack(lv_event_t *)
{
  showList();
}

void showError(const char *message, bool canRetry)
{
  s_mode = ViewMode::Error;
  setHeader("未能完成", false);
  lv_obj_clean(s_content);
  lv_obj_set_layout(s_content, 0);
  lv_obj_t *label = lv_label_create(s_content);
  lv_label_set_text(label, message && message[0] ? message : "發生未知錯誤");
  lv_obj_set_width(label, 700);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, &ui_font_NSTC28bold, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(kAmber), 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 90);

  if (canRetry)
  {
    lv_obj_t *retry = makeButton(s_content, "重試", 200, 56, onRetry);
    lv_obj_align(retry, LV_ALIGN_BOTTOM_LEFT, 170, -90);
  }
  lv_obj_t *back = makeButton(s_content, "返回", 200, 56, onErrorBack, nullptr, kInk);
  lv_obj_align(back, canRetry ? LV_ALIGN_BOTTOM_RIGHT : LV_ALIGN_BOTTOM_MID,
               canRetry ? -170 : 0, -90);
}

}  // namespace

void KmbSettingsView_Init()
{
  if (s_root)
    return;

  s_root = lv_obj_create(ui_Main);
  lv_obj_set_size(s_root, 800, 480);
  lv_obj_align(s_root, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(s_root, 0, 0);
  lv_obj_set_style_border_width(s_root, 0, 0);
  lv_obj_set_style_bg_color(s_root, lv_color_hex(kCoolPanel), 0);
  lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *header = lv_obj_create(s_root);
  lv_obj_set_size(header, 800, 64);
  lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(kKmbRed), 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  s_back = makeButton(header, "返回", 118, 48, onBack, nullptr, kInk);
  lv_obj_align(s_back, LV_ALIGN_LEFT_MID, -6, 0);
  s_title = lv_label_create(header);
  lv_label_set_text(s_title, "九巴收藏設定");
  lv_obj_set_style_text_font(s_title, &ui_font_NSTC28bold, 0);
  lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
  lv_obj_center(s_title);
  s_add = makeButton(header, "新增路線", 190, 48, onAdd, nullptr, kInk);
  lv_obj_align(s_add, LV_ALIGN_RIGHT_MID, 6, 0);

  s_content = lv_obj_create(s_root);
  lv_obj_set_size(s_content, 800, 416);
  lv_obj_align(s_content, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(s_content, lv_color_hex(kCoolPanel), 0);
  lv_obj_set_style_border_width(s_content, 0, 0);
  lv_obj_set_style_radius(s_content, 0, 0);
  lv_obj_set_style_pad_all(s_content, 12, 0);

  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

void KmbSettingsView_Open()
{
  if (!s_root)
    return;
  s_open = true;
  s_seenGeneration = KmbCatalog.snapshot().generation;
  showList();
  lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(s_root);
}

void KmbSettingsView_Close()
{
  if (!s_root)
    return;
  s_open = false;
  lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}

void KmbSettingsView_Tick()
{
  if (!s_open)
    return;
  const KmbCatalogSnapshot snapshot = KmbCatalog.snapshot();
  if (snapshot.generation == s_seenGeneration)
    return;
  s_seenGeneration = snapshot.generation;
  if (s_mode == ViewMode::List)
    return;

  lvgl_port_lock(-1);
  if (snapshot.status == KmbCatalogStatus::Loading)
  {
    showLoading(snapshot.message.c_str());
  }
  else if (snapshot.status == KmbCatalogStatus::Error)
  {
    showError(snapshot.message.c_str(), true);
  }
  else if (snapshot.status == KmbCatalogStatus::Ready)
  {
    if (!snapshot.directions.empty())
    {
      s_directions = snapshot.directions;
      showDirections();
    }
    else
    {
      s_stops = snapshot.stops;
      showStops();
    }
  }
  lvgl_port_unlock();
}

bool KmbSettingsView_IsOpen()
{
  return s_open;
}
