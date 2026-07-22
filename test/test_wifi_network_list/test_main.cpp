#include <unity.h>

#include <string>
#include <vector>

#include "WiFiNetworkList.h"
#include "WiFiPasswordValidation.h"

struct TestWiFiCredential
{
    std::string ssid;
    std::string password;
};

struct TestWiFiUpdate
{
    std::string ssid;
    std::string password;
    bool keepPassword;
    bool clearPassword;
};

void test_new_wifi_becomes_first_and_keeps_existing_networks()
{
    std::vector<TestWiFiCredential> networks{
        {"HomeWiFi", "old-one"}, {"BackupWiFi", "old-two"}};

    WiFiNetworkList::upsert(networks, {"new-home", "new-password"}, 4);

    TEST_ASSERT_EQUAL_STRING("new-home", networks[0].ssid.c_str());
    TEST_ASSERT_EQUAL_UINT32(3, networks.size());
}

void test_blank_password_preserves_password_for_known_wifi()
{
    std::vector<TestWiFiCredential> networks{
        {"HomeWiFi", "existing-password"}, {"BackupWiFi", "other-password"}};

    WiFiNetworkList::upsert(networks, {"HomeWiFi", ""}, 4);

    TEST_ASSERT_EQUAL_STRING("existing-password", networks[0].password.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, networks.size());
}

void test_wifi_list_is_capped_at_four()
{
    std::vector<TestWiFiCredential> networks{
        {"one", "1"}, {"two", "2"}, {"three", "3"}, {"four", "4"}};

    WiFiNetworkList::upsert(networks, {"new-home", "new-password"}, 4);

    TEST_ASSERT_EQUAL_UINT32(4, networks.size());
    TEST_ASSERT_EQUAL_STRING("new-home", networks[0].ssid.c_str());
}

void test_explicit_clear_replaces_known_password_with_empty_password()
{
    std::vector<TestWiFiCredential> networks{
        {"guest", "existing-password"}};

    WiFiNetworkList::upsert(networks, {"guest", ""}, 4, false);

    TEST_ASSERT_EQUAL_STRING("", networks[0].password.c_str());
}

void test_replace_list_reorders_deletes_and_preserves_hidden_passwords()
{
    const std::vector<TestWiFiCredential> existing{
        {"HomeWiFi", "saved-one"}, {"BackupWiFi", "saved-two"},
        {"remove-me", "saved-three"}};
    const std::vector<TestWiFiUpdate> updates{
        {"BackupWiFi", "", true, false},
        {"new-home", "new-password", false, false},
        {"HomeWiFi", "", true, false}};

    const auto result =
        WiFiNetworkList::replaceFromUpdates<TestWiFiCredential>(existing,
                                                                 updates, 4);

    TEST_ASSERT_EQUAL_UINT32(3, result.size());
    TEST_ASSERT_EQUAL_STRING("BackupWiFi", result[0].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("saved-two", result[0].password.c_str());
    TEST_ASSERT_EQUAL_STRING("new-home", result[1].ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("HomeWiFi", result[2].ssid.c_str());
}

void test_replace_list_can_explicitly_clear_password()
{
    const std::vector<TestWiFiCredential> existing{{"guest", "old-secret"}};
    const std::vector<TestWiFiUpdate> updates{
        {"guest", "", false, true}};

    const auto result =
        WiFiNetworkList::replaceFromUpdates<TestWiFiCredential>(existing,
                                                                 updates, 4);

    TEST_ASSERT_EQUAL_STRING("", result[0].password.c_str());
}

void test_wifi_password_accepts_open_wpa_and_hex_psk()
{
    TEST_ASSERT_TRUE(WiFiPasswordValidation::isValid(std::string("")));
    TEST_ASSERT_TRUE(WiFiPasswordValidation::isValid(
        std::string("eight888")));
    TEST_ASSERT_TRUE(WiFiPasswordValidation::isValid(std::string(64, 'a')));
}

void test_wifi_password_rejects_short_and_non_hex_64_character_values()
{
    TEST_ASSERT_FALSE(WiFiPasswordValidation::isValid(
        std::string("short")));
    TEST_ASSERT_FALSE(WiFiPasswordValidation::isValid(
        std::string(63, 'a') + "z"));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_new_wifi_becomes_first_and_keeps_existing_networks);
    RUN_TEST(test_blank_password_preserves_password_for_known_wifi);
    RUN_TEST(test_wifi_list_is_capped_at_four);
    RUN_TEST(test_explicit_clear_replaces_known_password_with_empty_password);
    RUN_TEST(test_replace_list_reorders_deletes_and_preserves_hidden_passwords);
    RUN_TEST(test_replace_list_can_explicitly_clear_password);
    RUN_TEST(test_wifi_password_accepts_open_wpa_and_hex_psk);
    RUN_TEST(test_wifi_password_rejects_short_and_non_hex_64_character_values);
    return UNITY_END();
}
