#pragma once

#include <string>
#include <vector>

struct KmbCachedDirection
{
    std::string direction;
    int serviceType = 1;
    std::string destinationTc;
};

namespace KmbRouteCache
{
std::vector<KmbCachedDirection> find(const std::string &route);
}
