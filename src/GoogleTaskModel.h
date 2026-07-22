#pragma once

#include <string>
#include <vector>

struct GoogleTaskItem
{
    std::string title;
    std::string due;
    bool completed = false;
};

namespace GoogleTaskModel
{
constexpr std::size_t kMaxItems = 12;

bool validate(const GoogleTaskItem &item, std::string &error);
bool append(std::vector<GoogleTaskItem> &items, GoogleTaskItem item,
            std::string &error);
std::vector<GoogleTaskItem> forDate(const std::vector<GoogleTaskItem> &items,
                                    const std::string &yyyyMmDd);
std::string formatForDisplay(const std::vector<GoogleTaskItem> &items,
                             std::size_t maximumItems = 6);
}
