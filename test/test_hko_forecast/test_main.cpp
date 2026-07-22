#include <unity.h>

#include "HkoForecastModel.h"

void test_accepts_only_seven_valid_days()
{
    std::vector<HkoForecastDay> days;
    std::string error;
    for (int index = 0; index < 7; ++index)
    {
        HkoForecastDay day{"2026072" + std::to_string(index + 1),
                           "星期二", "天晴", 28, 33, 50};
        TEST_ASSERT_TRUE(HkoForecastModel::append(days, day, error));
    }
    TEST_ASSERT_FALSE(HkoForecastModel::append(
        days, {"20260728", "星期二", "天晴", 28, 33, 50}, error));
    TEST_ASSERT_EQUAL_UINT32(7, days.size());
}

void test_formats_dates_and_rejects_bad_ranges()
{
    TEST_ASSERT_EQUAL_STRING("7/21", HkoForecastModel::displayDate("20260721").c_str());

    std::vector<HkoForecastDay> days;
    std::string error;
    TEST_ASSERT_FALSE(HkoForecastModel::append(
        days, {"2026-07-21", "星期二", "天晴", 28, 33, 50}, error));
    TEST_ASSERT_FALSE(HkoForecastModel::append(
        days, {"20260721", "星期二", "天晴", 35, 30, 50}, error));
}

void test_assigns_distinct_weather_colours()
{
    TEST_ASSERT_NOT_EQUAL(HkoForecastModel::cardColor(50),
                          HkoForecastModel::cardColor(60));
    TEST_ASSERT_NOT_EQUAL(HkoForecastModel::cardColor(60),
                          HkoForecastModel::cardColor(90));
}

void test_maps_hko_codes_to_drawable_icon_kinds()
{
    TEST_ASSERT_EQUAL(WeatherIconKind::Sun,
                      HkoForecastModel::iconKind(50));
    TEST_ASSERT_EQUAL(WeatherIconKind::SunCloudRain,
                      HkoForecastModel::iconKind(53));
    TEST_ASSERT_EQUAL(WeatherIconKind::Cloud,
                      HkoForecastModel::iconKind(60));
    TEST_ASSERT_EQUAL(WeatherIconKind::Rain,
                      HkoForecastModel::iconKind(63));
    TEST_ASSERT_EQUAL(WeatherIconKind::Thunder,
                      HkoForecastModel::iconKind(65));
    TEST_ASSERT_EQUAL(WeatherIconKind::Moon,
                      HkoForecastModel::iconKind(72));
    TEST_ASSERT_EQUAL(WeatherIconKind::Fog,
                      HkoForecastModel::iconKind(84));
}

void test_formats_temperature_range_without_missing_font_glyphs()
{
    TEST_ASSERT_EQUAL_STRING("27至32度",
                             HkoForecastModel::temperatureRange(27, 32).c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_only_seven_valid_days);
    RUN_TEST(test_formats_dates_and_rejects_bad_ranges);
    RUN_TEST(test_assigns_distinct_weather_colours);
    RUN_TEST(test_maps_hko_codes_to_drawable_icon_kinds);
    RUN_TEST(test_formats_temperature_range_without_missing_font_glyphs);
    return UNITY_END();
}
