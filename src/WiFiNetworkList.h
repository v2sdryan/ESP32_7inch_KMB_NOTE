#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace WiFiNetworkList
{
template <typename Credential>
void upsert(std::vector<Credential> &networks, Credential candidate,
            std::size_t maximumNetworks,
            bool preserveKnownPasswordWhenEmpty = true)
{
    const auto existing = std::find_if(
        networks.begin(), networks.end(), [&](const Credential &network) {
            return network.ssid == candidate.ssid;
        });
    if (preserveKnownPasswordWhenEmpty && existing != networks.end() &&
        candidate.password.length() == 0)
        candidate.password = existing->password;

    std::vector<Credential> updated;
    if (maximumNetworks > 0) updated.push_back(candidate);
    for (const Credential &network : networks) {
        if (network.ssid == candidate.ssid) continue;
        if (updated.size() >= maximumNetworks) break;
        updated.push_back(network);
    }
    networks = std::move(updated);
}

template <typename Credential, typename Update>
std::vector<Credential> replaceFromUpdates(
    const std::vector<Credential> &existing,
    const std::vector<Update> &updates, std::size_t maximumNetworks)
{
    std::vector<Credential> result;
    for (const Update &update : updates) {
        if (result.size() >= maximumNetworks) break;
        const auto duplicate = std::find_if(
            result.begin(), result.end(), [&](const Credential &network) {
                return network.ssid == update.ssid;
            });
        if (duplicate != result.end()) continue;

        Credential candidate{update.ssid, update.password};
        if (update.keepPassword && !update.clearPassword) {
            const auto saved = std::find_if(
                existing.begin(), existing.end(),
                [&](const Credential &network) {
                    return network.ssid == update.ssid;
                });
            if (saved != existing.end()) candidate.password = saved->password;
        }
        if (update.clearPassword)
            candidate.password = decltype(candidate.password){};
        result.push_back(candidate);
    }
    return result;
}
}
