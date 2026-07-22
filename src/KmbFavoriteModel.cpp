#include "KmbFavoriteModel.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace KmbFavoriteModel {

std::string normalizeRoute(const std::string& route) {
    const auto first = std::find_if_not(route.begin(), route.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    const auto last = std::find_if_not(route.rbegin(), route.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();

    if (first >= last) {
        return {};
    }

    std::string normalized(first, last);
    std::transform(
        normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return normalized;
}

bool validate(const KmbFavorite& favorite, std::string& error) {
    const std::string normalizedRoute = normalizeRoute(favorite.route);
    if (normalizedRoute.empty() || normalizedRoute.size() > 6) {
        error = "路線編號無效";
        return false;
    }
    if (favorite.direction != "I" && favorite.direction != "O") {
        error = "行車方向無效";
        return false;
    }
    if (favorite.serviceType < 1 || favorite.serviceType > 3) {
        error = "服務類別無效";
        return false;
    }
    if (favorite.destinationTc.empty() || favorite.stopId.empty() || favorite.stopNameTc.empty()) {
        error = "車站資料不完整";
        return false;
    }

    error.clear();
    return true;
}

bool sameKey(const KmbFavorite& left, const KmbFavorite& right) {
    return normalizeRoute(left.route) == normalizeRoute(right.route) &&
           left.direction == right.direction && left.serviceType == right.serviceType &&
           left.stopId == right.stopId;
}

bool add(std::vector<KmbFavorite>& favorites, KmbFavorite favorite, std::string& error) {
    favorite.route = normalizeRoute(favorite.route);
    if (!validate(favorite, error)) {
        return false;
    }
    if (favorites.size() >= kMaxFavorites) {
        error = "最多只可收藏八個車站";
        return false;
    }
    if (std::any_of(favorites.begin(), favorites.end(), [&](const KmbFavorite& existing) {
            return sameKey(existing, favorite);
        })) {
        error = "呢個車站已經收藏";
        return false;
    }

    favorites.push_back(std::move(favorite));
    error.clear();
    return true;
}

bool removeAt(std::vector<KmbFavorite>& favorites, std::size_t index) {
    if (index >= favorites.size()) {
        return false;
    }
    favorites.erase(favorites.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool replaceAt(std::vector<KmbFavorite>& favorites, std::size_t index,
               KmbFavorite favorite, std::string& error) {
    favorite.route = normalizeRoute(favorite.route);
    if (index >= favorites.size()) {
        error = "收藏項目不存在";
        return false;
    }
    if (!validate(favorite, error)) {
        return false;
    }
    for (std::size_t other = 0; other < favorites.size(); ++other) {
        if (other != index && sameKey(favorites[other], favorite)) {
            error = "呢個車站已經收藏";
            return false;
        }
    }
    favorites[index] = std::move(favorite);
    error.clear();
    return true;
}

bool move(std::vector<KmbFavorite>& favorites, std::size_t from, std::size_t to) {
    if (from >= favorites.size() || to >= favorites.size()) {
        return false;
    }
    if (from == to) {
        return true;
    }

    KmbFavorite favorite = std::move(favorites[from]);
    favorites.erase(favorites.begin() + static_cast<std::ptrdiff_t>(from));
    favorites.insert(
        favorites.begin() + static_cast<std::ptrdiff_t>(to), std::move(favorite));
    return true;
}

std::size_t pageCount(std::size_t itemCount, std::size_t pageSize) {
    return pageSize == 0 ? 0 : (itemCount + pageSize - 1) / pageSize;
}

std::vector<KmbFavorite> page(
    const std::vector<KmbFavorite>& favorites,
    std::size_t pageIndex,
    std::size_t pageSize) {
    if (pageSize == 0 || pageIndex >= pageCount(favorites.size(), pageSize)) {
        return {};
    }

    const std::size_t first = pageIndex * pageSize;
    const std::size_t last = std::min(first + pageSize, favorites.size());
    return {favorites.begin() + static_cast<std::ptrdiff_t>(first),
            favorites.begin() + static_cast<std::ptrdiff_t>(last)};
}

}  // namespace KmbFavoriteModel
