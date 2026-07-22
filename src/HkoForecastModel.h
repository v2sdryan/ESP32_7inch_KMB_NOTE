#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct HkoForecastDay
{
    std::string date;
    std::string week;
    std::string weather;
    int minTemp = 0;
    int maxTemp = 0;
    int icon = 0;
};

enum class WeatherIconKind
{
    Unknown,
    Sun,
    SunCloud,
    SunCloudRain,
    Cloud,
    Rain,
    Thunder,
    Moon,
    Wind,
    Fog,
};

namespace HkoForecastModel
{
constexpr std::size_t kMaxDays = 7;

bool validate(const HkoForecastDay &day, std::string &error);
bool append(std::vector<HkoForecastDay> &days, HkoForecastDay day,
            std::string &error);
std::string displayDate(const std::string &yyyymmdd);
std::string temperatureRange(int minTemp, int maxTemp);
uint32_t cardColor(int icon);
WeatherIconKind iconKind(int hkoIcon);
}
