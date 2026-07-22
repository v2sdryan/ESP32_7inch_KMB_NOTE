#include <unity.h>

#include "ScheduleModel.h"

void test_filters_items_for_today_and_sorts_by_time()
{
    std::vector<ScheduleItem> items = {
        {"15:30", "跆拳道", 1U << 1},
        {"09:00", "英文課", 1U << 1},
        {"12:00", "星期二限定", 1U << 2},
    };

    const auto today = ScheduleModel::forWeekday(items, 1);

    TEST_ASSERT_EQUAL_UINT32(2, today.size());
    TEST_ASSERT_EQUAL_STRING("09:00", today[0].time.c_str());
    TEST_ASSERT_EQUAL_STRING("英文課", today[0].title.c_str());
    TEST_ASSERT_EQUAL_STRING("15:30", today[1].time.c_str());
}

void test_rejects_invalid_schedule_items()
{
    TEST_ASSERT_FALSE(ScheduleModel::isValid({"24:00", "太遲", 0x7F}));
    TEST_ASSERT_FALSE(ScheduleModel::isValid({"09:60", "錯誤分鐘", 0x7F}));
    TEST_ASSERT_FALSE(ScheduleModel::isValid({"09:00", "", 0x7F}));
    TEST_ASSERT_FALSE(ScheduleModel::isValid({"09:00", "測試", 0}));
    TEST_ASSERT_TRUE(ScheduleModel::isValid({"09:05", "中文日程", 0x7F}));
}

void test_formats_empty_and_populated_day_in_traditional_chinese()
{
    TEST_ASSERT_EQUAL_STRING("今日暫無日程", ScheduleModel::formatForDisplay({}).c_str());

    const std::vector<ScheduleItem> items = {
        {"09:00", "英文課", 0x7F},
        {"15:30", "跆拳道", 0x7F},
    };
    TEST_ASSERT_EQUAL_STRING("09:00  英文課\n15:30  跆拳道",
                             ScheduleModel::formatForDisplay(items).c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_filters_items_for_today_and_sorts_by_time);
    RUN_TEST(test_rejects_invalid_schedule_items);
    RUN_TEST(test_formats_empty_and_populated_day_in_traditional_chinese);
    return UNITY_END();
}
