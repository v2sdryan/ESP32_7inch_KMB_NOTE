#include <unity.h>

#include "GoogleTaskModel.h"

void test_filters_today_and_completed_tasks()
{
    std::vector<GoogleTaskItem> items = {
        {"買餸", "2026-07-21T00:00:00.000Z", false},
        {"已完成", "2026-07-21T00:00:00.000Z", true},
        {"明日工作", "2026-07-22T00:00:00.000Z", false},
    };
    const auto today = GoogleTaskModel::forDate(items, "2026-07-21");
    TEST_ASSERT_EQUAL_UINT32(1, today.size());
    TEST_ASSERT_EQUAL_STRING("買餸", today[0].title.c_str());
}

void test_rejects_invalid_and_caps_results()
{
    std::vector<GoogleTaskItem> items;
    std::string error;
    TEST_ASSERT_FALSE(GoogleTaskModel::append(
        items, {"", "2026-07-21T00:00:00.000Z", false}, error));
    for (int index = 0; index < 12; ++index) {
        TEST_ASSERT_TRUE(GoogleTaskModel::append(
            items, {"工作" + std::to_string(index),
                    "2026-07-21T00:00:00.000Z", false}, error));
    }
    TEST_ASSERT_FALSE(GoogleTaskModel::append(
        items, {"第十三項", "2026-07-21T00:00:00.000Z", false}, error));
}

void test_formats_google_section()
{
    const std::vector<GoogleTaskItem> items = {
        {"買餸", "2026-07-21T00:00:00.000Z", false},
        {"交文件", "2026-07-21T00:00:00.000Z", false},
    };
    TEST_ASSERT_EQUAL_STRING(
        "Google Tasks\n- 買餸\n- 交文件",
        GoogleTaskModel::formatForDisplay(items).c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_filters_today_and_completed_tasks);
    RUN_TEST(test_rejects_invalid_and_caps_results);
    RUN_TEST(test_formats_google_section);
    return UNITY_END();
}
