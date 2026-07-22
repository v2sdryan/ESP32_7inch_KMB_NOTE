#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct KmbFavorite {
    std::string route;
    std::string direction;
    int serviceType = 1;
    std::string destinationTc;
    std::string stopId;
    std::string stopNameTc;
};

namespace KmbFavoriteModel {

constexpr std::size_t kMaxFavorites = 8;

std::string normalizeRoute(const std::string& route);
bool validate(const KmbFavorite& favorite, std::string& error);
bool sameKey(const KmbFavorite& left, const KmbFavorite& right);
bool add(std::vector<KmbFavorite>& favorites, KmbFavorite favorite, std::string& error);
bool removeAt(std::vector<KmbFavorite>& favorites, std::size_t index);
bool replaceAt(std::vector<KmbFavorite>& favorites, std::size_t index,
               KmbFavorite favorite, std::string& error);
bool move(std::vector<KmbFavorite>& favorites, std::size_t from, std::size_t to);
std::size_t pageCount(std::size_t itemCount, std::size_t pageSize);
std::vector<KmbFavorite> page(
    const std::vector<KmbFavorite>& favorites,
    std::size_t pageIndex,
    std::size_t pageSize);

}  // namespace KmbFavoriteModel
