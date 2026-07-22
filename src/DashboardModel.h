#pragma once

#include <string>
#include <vector>

struct DashboardBusItem
{
    std::string route;
    std::string destination;
    std::string eta1;
    std::string eta2;
};

namespace DashboardModel
{
constexpr std::size_t kMaxVisibleBuses = 4;

std::string formatBusEta(const DashboardBusItem &item);
std::vector<DashboardBusItem> visibleBuses(
    const std::vector<DashboardBusItem> &items);
const char *emptyBusMessage();
std::string sanitizeForDisplay(const std::string &text);
}
