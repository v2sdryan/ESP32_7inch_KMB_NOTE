#include <unity.h>

#include "KmbFavoriteModel.h"
#include "KmbCatalogModel.h"
#include "KmbRouteCache.h"

void test_rules() {
    KmbFavorite favorite{"93A", "I", 1, "寶林", "A", "觀塘市中心"};
    std::vector<KmbFavorite> favorites;
    std::string error;

    TEST_ASSERT_TRUE(KmbFavoriteModel::add(favorites, favorite, error));
    TEST_ASSERT_FALSE(KmbFavoriteModel::add(favorites, favorite, error));

    for (int index = 1; index < 8; ++index) {
        auto item = favorite;
        item.route = std::to_string(index);
        item.stopId = std::to_string(index);
        TEST_ASSERT_TRUE(KmbFavoriteModel::add(favorites, item, error));
    }

    TEST_ASSERT_FALSE(KmbFavoriteModel::add(
        favorites, {"9", "O", 1, "終點", "9", "站"}, error));
    TEST_ASSERT_TRUE(KmbFavoriteModel::move(favorites, 2, 1));
    TEST_ASSERT_TRUE(KmbFavoriteModel::removeAt(favorites, 0));
    TEST_ASSERT_EQUAL_UINT32(2, KmbFavoriteModel::pageCount(favorites.size(), 4));
}

void test_replaces_a_favorite_stop_without_changing_its_position() {
    std::vector<KmbFavorite> favorites{
        {"93A", "I", 1, "寶林", "OLD", "舊站"},
        {"1A", "I", 1, "中秀茂坪", "KEEP", "保留站"},
    };
    std::string error;

    TEST_ASSERT_TRUE(KmbFavoriteModel::replaceAt(
        favorites, 0,
        {"93A", "I", 1, "寶林", "NEW", "新站"}, error));
    TEST_ASSERT_EQUAL_STRING("NEW", favorites[0].stopId.c_str());
    TEST_ASSERT_EQUAL_STRING("新站", favorites[0].stopNameTc.c_str());
    TEST_ASSERT_EQUAL_STRING("1A", favorites[1].route.c_str());
}

void test_parses_kmb_sequence_strings_from_official_api() {
    TEST_ASSERT_EQUAL_INT(1, KmbCatalogModel::parseSequence("1"));
    TEST_ASSERT_EQUAL_INT(34, KmbCatalogModel::parseSequence("34"));
    TEST_ASSERT_EQUAL_INT(0, KmbCatalogModel::parseSequence(""));
    TEST_ASSERT_EQUAL_INT(0, KmbCatalogModel::parseSequence("3A"));
}

void test_cached_catalog_contains_routes_that_failed_on_touch_screen() {
    const auto oneA = KmbRouteCache::find("1A");
    TEST_ASSERT_EQUAL_UINT32(2, oneA.size());
    TEST_ASSERT_EQUAL_STRING("O", oneA[0].direction.c_str());
    TEST_ASSERT_EQUAL_STRING("尖沙咀碼頭", oneA[0].destinationTc.c_str());
    TEST_ASSERT_EQUAL_STRING("I", oneA[1].direction.c_str());
    TEST_ASSERT_EQUAL_STRING("中秀茂坪", oneA[1].destinationTc.c_str());

    const auto ninetyThreeK = KmbRouteCache::find("93K");
    TEST_ASSERT_EQUAL_UINT32(2, ninetyThreeK.size());
    TEST_ASSERT_EQUAL_STRING("旺角東站", ninetyThreeK[0].destinationTc.c_str());
    TEST_ASSERT_EQUAL_STRING("寶林", ninetyThreeK[1].destinationTc.c_str());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rules);
    RUN_TEST(test_replaces_a_favorite_stop_without_changing_its_position);
    RUN_TEST(test_parses_kmb_sequence_strings_from_official_api);
    RUN_TEST(test_cached_catalog_contains_routes_that_failed_on_touch_screen);
    return UNITY_END();
}
