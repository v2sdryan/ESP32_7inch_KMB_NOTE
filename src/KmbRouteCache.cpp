#include "KmbRouteCache.h"

#include <algorithm>
#include <cctype>

namespace
{
struct RouteRecord
{
    const char *route;
    const char *direction;
    int serviceType;
    const char *destinationTc;
};

constexpr RouteRecord kRoutes[] = {
#include "KmbRouteCacheData.inc"
};

std::string normalizeRoute(std::string route)
{
    route.erase(route.begin(), std::find_if(route.begin(), route.end(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }));
    route.erase(std::find_if(route.rbegin(), route.rend(), [](unsigned char character) {
        return std::isspace(character) == 0;
    }).base(), route.end());
    std::transform(route.begin(), route.end(), route.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return route;
}
}

namespace KmbRouteCache
{
std::vector<KmbCachedDirection> find(const std::string &route)
{
    const std::string normalized = normalizeRoute(route);
    std::vector<KmbCachedDirection> result;
    for (const RouteRecord &record : kRoutes)
    {
        if (normalized == record.route)
            result.push_back({record.direction, record.serviceType,
                              record.destinationTc});
    }
    return result;
}
}
