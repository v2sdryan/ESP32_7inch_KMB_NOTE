#include "HkoForecastModel.h"

#include <algorithm>
#include <cctype>

namespace HkoForecastModel
{
bool validate(const HkoForecastDay &day, std::string &error)
{
    if (day.date.size() != 8 ||
        !std::all_of(day.date.begin(), day.date.end(), [](unsigned char value) {
            return std::isdigit(value) != 0;
        }))
    {
        error = "預報日期無效";
        return false;
    }
    if (day.week.empty() || day.weather.empty())
    {
        error = "預報內容不完整";
        return false;
    }
    if (day.minTemp < -20 || day.maxTemp > 60 || day.minTemp > day.maxTemp)
    {
        error = "預報溫度範圍無效";
        return false;
    }
    return true;
}

bool append(std::vector<HkoForecastDay> &days, HkoForecastDay day,
            std::string &error)
{
    if (days.size() >= kMaxDays)
    {
        error = "只顯示首七日預報";
        return false;
    }
    if (!validate(day, error)) return false;
    days.push_back(std::move(day));
    error.clear();
    return true;
}

std::string displayDate(const std::string &yyyymmdd)
{
    if (yyyymmdd.size() != 8) return "-";
    int month = std::stoi(yyyymmdd.substr(4, 2));
    int day = std::stoi(yyyymmdd.substr(6, 2));
    return std::to_string(month) + "/" + std::to_string(day);
}

std::string temperatureRange(int minTemp, int maxTemp)
{
    return std::to_string(minTemp) + "至" + std::to_string(maxTemp) + "度";
}

uint32_t cardColor(int icon)
{
    if (icon >= 50 && icon <= 52) return 0xF6C85F;
    if (icon >= 53 && icon <= 59) return 0x8EC5FC;
    if (icon >= 60 && icon <= 69) return 0x66A6FF;
    if (icon >= 70 && icon <= 79) return 0x72C6A1;
    if (icon >= 80) return 0x8A7DE3;
    return 0xB8C4CE;
}

WeatherIconKind iconKind(int icon)
{
    if (icon == 50 || icon == 81 || icon == 90 || icon == 91)
        return WeatherIconKind::Sun;
    if (icon == 51 || icon == 52)
        return WeatherIconKind::SunCloud;
    if (icon == 53 || icon == 54)
        return WeatherIconKind::SunCloudRain;
    if (icon == 60 || icon == 61 || icon == 76 || icon == 77 ||
        icon == 82 || icon == 92 || icon == 93)
        return WeatherIconKind::Cloud;
    if (icon >= 62 && icon <= 64)
        return WeatherIconKind::Rain;
    if (icon == 65)
        return WeatherIconKind::Thunder;
    if (icon >= 70 && icon <= 75)
        return WeatherIconKind::Moon;
    if (icon == 80)
        return WeatherIconKind::Wind;
    if (icon >= 83 && icon <= 85)
        return WeatherIconKind::Fog;
    return WeatherIconKind::Unknown;
}
}
