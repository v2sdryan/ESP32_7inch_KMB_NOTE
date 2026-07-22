#pragma once

#include <Arduino.h>

#include <cstddef>
#include <vector>

#include "KmbFavoriteModel.h"

class KmbFavoriteRepository {
public:
  const std::vector<KmbFavorite> &list() const;
  bool add(const KmbFavorite &favorite, String &error);
  bool remove(std::size_t index, String &error);
  bool replace(std::size_t index, const KmbFavorite &favorite, String &error);
  bool moveUp(std::size_t index, String &error);
  bool moveDown(std::size_t index, String &error);
};

extern KmbFavoriteRepository KmbFavorites;
