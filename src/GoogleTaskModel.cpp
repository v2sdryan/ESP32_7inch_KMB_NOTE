#include "GoogleTaskModel.h"

#include <utility>

#include <algorithm>
#include <cctype>

namespace
{
bool validDatePrefix(const std::string &value)
{
    if (value.size() < 10 || value[4] != '-' || value[7] != '-') return false;
    for (std::size_t index = 0; index < 10; ++index) {
        if (index == 4 || index == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(value[index]))) return false;
    }
    return true;
}
}

namespace GoogleTaskModel
{
bool validate(const GoogleTaskItem &item, std::string &error)
{
    if (item.title.empty() || item.title.size() > 72)
    {
        error = "Google Tasks 標題無效";
        return false;
    }
    if (!validDatePrefix(item.due))
    {
        error = "Google Tasks 到期日無效";
        return false;
    }
    return true;
}

bool append(std::vector<GoogleTaskItem> &items, GoogleTaskItem item,
            std::string &error)
{
    if (items.size() >= kMaxItems)
    {
        error = "Google Tasks 項目超過上限";
        return false;
    }
    if (!validate(item, error)) return false;
    items.push_back(std::move(item));
    error.clear();
    return true;
}

std::vector<GoogleTaskItem> forDate(const std::vector<GoogleTaskItem> &items,
                                    const std::string &yyyyMmDd)
{
    std::vector<GoogleTaskItem> result;
    if (yyyyMmDd.size() != 10) return result;
    for (const GoogleTaskItem &item : items) {
        if (!item.completed && validDatePrefix(item.due) &&
            item.due.compare(0, 10, yyyyMmDd) == 0) {
            result.push_back(item);
        }
    }
    return result;
}

std::string formatForDisplay(const std::vector<GoogleTaskItem> &items,
                             std::size_t maximumItems)
{
    if (items.empty() || maximumItems == 0) return "";
    std::string output = "Google Tasks";
    const std::size_t count = std::min(items.size(), maximumItems);
    for (std::size_t index = 0; index < count; ++index) {
        output += "\n- ";
        output += items[index].title;
    }
    return output;
}
}
