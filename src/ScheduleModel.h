#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ScheduleItem
{
    std::string time;
    std::string title;
    uint8_t daysMask = 0;
};

class ScheduleModel
{
public:
    static bool isValid(const ScheduleItem &item);
    static std::vector<ScheduleItem> forWeekday(const std::vector<ScheduleItem> &items,
                                                uint8_t weekday);
    static std::string formatForDisplay(const std::vector<ScheduleItem> &items,
                                        size_t maximumItems = 6);
};
