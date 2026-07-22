#include "ScheduleModel.h"

#include <algorithm>
#include <cctype>

namespace
{
bool validTime(const std::string &value)
{
    if (value.size() != 5 || value[2] != ':') return false;
    if (!std::isdigit(static_cast<unsigned char>(value[0])) ||
        !std::isdigit(static_cast<unsigned char>(value[1])) ||
        !std::isdigit(static_cast<unsigned char>(value[3])) ||
        !std::isdigit(static_cast<unsigned char>(value[4]))) return false;
    const int hour = (value[0] - '0') * 10 + value[1] - '0';
    const int minute = (value[3] - '0') * 10 + value[4] - '0';
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}
}

bool ScheduleModel::isValid(const ScheduleItem &item)
{
    return validTime(item.time) && !item.title.empty() && item.title.size() <= 72 &&
           (item.daysMask & 0x7F) != 0;
}

std::vector<ScheduleItem> ScheduleModel::forWeekday(const std::vector<ScheduleItem> &items,
                                                    uint8_t weekday)
{
    std::vector<ScheduleItem> result;
    if (weekday > 6) return result;
    for (const auto &item : items) {
        if (isValid(item) && (item.daysMask & (1U << weekday))) result.push_back(item);
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.time < b.time;
    });
    return result;
}

std::string ScheduleModel::formatForDisplay(const std::vector<ScheduleItem> &items,
                                            size_t maximumItems)
{
    if (items.empty() || maximumItems == 0) return "今日暫無日程";
    std::string output;
    const size_t count = std::min(items.size(), maximumItems);
    for (size_t i = 0; i < count; ++i) {
        if (i) output += '\n';
        output += items[i].time;
        output += "  ";
        output += items[i].title;
    }
    return output;
}
