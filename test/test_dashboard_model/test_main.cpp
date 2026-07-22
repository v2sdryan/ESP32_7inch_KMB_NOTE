#include <unity.h>

#include "DashboardModel.h"

void test_formats_bus_eta_for_compact_dashboard_row()
{
    DashboardBusItem item{"1A", "中秀茂坪", "3", "7"};
    TEST_ASSERT_EQUAL_STRING("1A 中秀茂坪 3 / 7 分",
                             DashboardModel::formatBusEta(item).c_str());
}

void test_caps_dashboard_bus_rows_at_four()
{
    std::vector<DashboardBusItem> items{
        {"1A", "A", "1", "2"}, {"2", "B", "3", "4"},
        {"3", "C", "5", "6"}, {"4", "D", "7", "8"},
        {"5", "E", "9", "10"},
    };
    TEST_ASSERT_EQUAL_UINT32(4, DashboardModel::visibleBuses(items).size());
}

void test_describes_empty_dashboard_bus_list()
{
    TEST_ASSERT_EQUAL_STRING("尚未設定巴士收藏",
                             DashboardModel::emptyBusMessage());
}

void test_replaces_punctuation_missing_from_the_display_font()
{
    TEST_ASSERT_EQUAL_STRING(
        "晚餐 豬手, 蒸魚, 西蘭花",
        DashboardModel::sanitizeForDisplay("晚餐 豬手、蒸魚，西蘭花").c_str());
}

void test_keeps_agenda_items_on_separate_lines()
{
    TEST_ASSERT_EQUAL_STRING(
        "08:45 依南長號\n17:00 汁盈英文",
        DashboardModel::sanitizeForDisplay(
            "08:45 依南長號\n17:00 汁盈英文").c_str());
}

void test_replaces_fullwidth_slash_used_by_default_agenda()
{
    TEST_ASSERT_EQUAL_STRING(
        "準備返學/返工",
        DashboardModel::sanitizeForDisplay("準備返學／返工").c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_formats_bus_eta_for_compact_dashboard_row);
    RUN_TEST(test_caps_dashboard_bus_rows_at_four);
    RUN_TEST(test_describes_empty_dashboard_bus_list);
    RUN_TEST(test_replaces_punctuation_missing_from_the_display_font);
    RUN_TEST(test_keeps_agenda_items_on_separate_lines);
    RUN_TEST(test_replaces_fullwidth_slash_used_by_default_agenda);
    return UNITY_END();
}
