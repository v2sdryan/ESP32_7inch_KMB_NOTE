#include "KmbFavoriteRepository.h"

#include "ConfigManager.h"

KmbFavoriteRepository KmbFavorites;

const std::vector<KmbFavorite> &KmbFavoriteRepository::list() const
{
  return ConfigMgr.getConfig().kmb_favorites;
}

bool KmbFavoriteRepository::add(const KmbFavorite &favorite, String &error)
{
  Config next = ConfigMgr.getConfig();
  std::string modelError;
  if (!KmbFavoriteModel::add(next.kmb_favorites, favorite, modelError))
  {
    error = modelError.c_str();
    return false;
  }
  return ConfigMgr.update(next, error);
}

bool KmbFavoriteRepository::remove(std::size_t index, String &error)
{
  Config next = ConfigMgr.getConfig();
  if (!KmbFavoriteModel::removeAt(next.kmb_favorites, index))
  {
    error = "收藏項目不存在";
    return false;
  }
  return ConfigMgr.update(next, error);
}

bool KmbFavoriteRepository::replace(std::size_t index,
                                    const KmbFavorite &favorite,
                                    String &error)
{
  Config next = ConfigMgr.getConfig();
  std::string modelError;
  if (!KmbFavoriteModel::replaceAt(next.kmb_favorites, index, favorite,
                                   modelError))
  {
    error = modelError.c_str();
    return false;
  }
  return ConfigMgr.update(next, error);
}

bool KmbFavoriteRepository::moveUp(std::size_t index, String &error)
{
  Config next = ConfigMgr.getConfig();
  if (index == 0 || !KmbFavoriteModel::move(next.kmb_favorites, index, index - 1))
  {
    error = "收藏項目已經喺最上面";
    return false;
  }
  return ConfigMgr.update(next, error);
}

bool KmbFavoriteRepository::moveDown(std::size_t index, String &error)
{
  Config next = ConfigMgr.getConfig();
  if (index >= next.kmb_favorites.size() || index + 1 >= next.kmb_favorites.size() ||
      !KmbFavoriteModel::move(next.kmb_favorites, index, index + 1))
  {
    error = "收藏項目已經喺最下面";
    return false;
  }
  return ConfigMgr.update(next, error);
}
