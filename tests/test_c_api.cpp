#include <gtest/gtest.h>
#include "orip/orip_c.h"
#include <cstring>

class CAPITest : public ::testing::Test {
protected:
    orip_parser_t* parser = nullptr;

    void SetUp() override {
        parser = orip_create();
        ASSERT_NE(parser, nullptr);
    }

    void TearDown() override {
        if (parser) {
            orip_destroy(parser);
            parser = nullptr;
        }
    }

    // Helper to create a BLE advertisement with ODID Basic ID
    std::vector<uint8_t> createBasicIDAdvertisement(const char* serial) {
        std::vector<uint8_t> adv;

        // AD structure: [length][type 0x16][UUID low][UUID high][counter][message...]
        adv.push_back(30);   // Length
        adv.push_back(0x16); // Service Data AD type
        adv.push_back(0xFA); // UUID low (0xFFFA)
        adv.push_back(0xFF); // UUID high
        adv.push_back(0x00); // Message counter

        // Basic ID Message (25 bytes)
        adv.push_back(0x02); // Message type (0x0) | Proto version (0x2)
        adv.push_back(0x12); // ID type: Serial (1) | UA type: Multirotor (2)

        // Serial number (20 bytes)
        size_t len = std::strlen(serial);
        for (size_t i = 0; i < 20; i++) {
            adv.push_back(i < len ? static_cast<uint8_t>(serial[i]) : 0);
        }

        // Padding (3 bytes)
        adv.push_back(0);
        adv.push_back(0);
        adv.push_back(0);

        return adv;
    }
};

TEST_F(CAPITest, Version) {
    const char* version = orip_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STREQ(version, "0.1.0");
}

TEST_F(CAPITest, DefaultConfig) {
    orip_config_t config = orip_default_config();
    EXPECT_EQ(config.uav_timeout_ms, 30000u);
    EXPECT_EQ(config.enable_deduplication, 1);
    EXPECT_EQ(config.enable_astm, 1);
    EXPECT_EQ(config.enable_asd, 0);
    EXPECT_EQ(config.enable_cn, 0);
}

TEST_F(CAPITest, CreateDestroy) {
    orip_parser_t* p = orip_create();
    ASSERT_NE(p, nullptr);
    orip_destroy(p);
}

TEST_F(CAPITest, CreateWithConfig) {
    orip_config_t config = orip_default_config();
    config.uav_timeout_ms = 60000;

    orip_parser_t* p = orip_create_with_config(&config);
    ASSERT_NE(p, nullptr);
    orip_destroy(p);
}

TEST_F(CAPITest, ParseBasicID) {
    auto adv = createBasicIDAdvertisement("TEST123");

    orip_result_t result;
    int ret = orip_parse(parser, adv.data(), adv.size(), -70,
                         ORIP_TRANSPORT_BT_LEGACY, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.success, 1);
    EXPECT_EQ(result.is_remote_id, 1);
    EXPECT_EQ(result.protocol, ORIP_PROTOCOL_ASTM_F3411);
    EXPECT_STREQ(result.uav.id, "TEST123");
    EXPECT_EQ(result.uav.id_type, ORIP_ID_TYPE_SERIAL_NUMBER);
    EXPECT_EQ(result.uav.uav_type, ORIP_UAV_TYPE_HELICOPTER_OR_MULTIROTOR);
    EXPECT_EQ(result.uav.rssi, -70);
}

TEST_F(CAPITest, ParseInvalidPayload) {
    uint8_t invalid[] = {0x01, 0x02, 0x03};

    orip_result_t result;
    int ret = orip_parse(parser, invalid, sizeof(invalid), -50,
                         ORIP_TRANSPORT_BT_LEGACY, &result);

    EXPECT_EQ(ret, 0);  // Function succeeded
    EXPECT_EQ(result.success, 0);  // But parsing failed
    EXPECT_EQ(result.is_remote_id, 0);
}

TEST_F(CAPITest, ParseNullParams) {
    orip_result_t result;
    uint8_t data[] = {0x01};

    EXPECT_EQ(orip_parse(nullptr, data, 1, 0, ORIP_TRANSPORT_BT_LEGACY, &result), -1);
    EXPECT_EQ(orip_parse(parser, nullptr, 1, 0, ORIP_TRANSPORT_BT_LEGACY, &result), -1);
    EXPECT_EQ(orip_parse(parser, data, 1, 0, ORIP_TRANSPORT_BT_LEGACY, nullptr), -1);
}

TEST_F(CAPITest, ActiveUAVCount) {
    EXPECT_EQ(orip_get_active_count(parser), 0u);

    auto adv1 = createBasicIDAdvertisement("UAV001");
    auto adv2 = createBasicIDAdvertisement("UAV002");

    orip_result_t result;
    orip_parse(parser, adv1.data(), adv1.size(), -60, ORIP_TRANSPORT_BT_LEGACY, &result);
    EXPECT_EQ(orip_get_active_count(parser), 1u);

    orip_parse(parser, adv2.data(), adv2.size(), -70, ORIP_TRANSPORT_BT_LEGACY, &result);
    EXPECT_EQ(orip_get_active_count(parser), 2u);
}

TEST_F(CAPITest, GetActiveUAVs) {
    auto adv1 = createBasicIDAdvertisement("DRONE_A");
    auto adv2 = createBasicIDAdvertisement("DRONE_B");

    orip_result_t result;
    orip_parse(parser, adv1.data(), adv1.size(), -60, ORIP_TRANSPORT_BT_LEGACY, &result);
    orip_parse(parser, adv2.data(), adv2.size(), -70, ORIP_TRANSPORT_BT_LEGACY, &result);

    orip_uav_t uavs[10];
    size_t count = orip_get_active_uavs(parser, uavs, 10);

    EXPECT_EQ(count, 2u);

    // Check both UAVs are present (order may vary)
    bool found_a = false, found_b = false;
    for (size_t i = 0; i < count; i++) {
        if (std::strcmp(uavs[i].id, "DRONE_A") == 0) found_a = true;
        if (std::strcmp(uavs[i].id, "DRONE_B") == 0) found_b = true;
    }
    EXPECT_TRUE(found_a);
    EXPECT_TRUE(found_b);
}

TEST_F(CAPITest, GetUAVByID) {
    auto adv = createBasicIDAdvertisement("FINDME");

    orip_result_t result;
    orip_parse(parser, adv.data(), adv.size(), -55, ORIP_TRANSPORT_BT_LEGACY, &result);

    orip_uav_t uav;
    int ret = orip_get_uav(parser, "FINDME", &uav);

    EXPECT_EQ(ret, 0);
    EXPECT_STREQ(uav.id, "FINDME");
    EXPECT_EQ(uav.rssi, -55);

    // Not found
    ret = orip_get_uav(parser, "NOTEXIST", &uav);
    EXPECT_NE(ret, 0);
}

TEST_F(CAPITest, Clear) {
    auto adv = createBasicIDAdvertisement("TEMP");

    orip_result_t result;
    orip_parse(parser, adv.data(), adv.size(), -60, ORIP_TRANSPORT_BT_LEGACY, &result);
    EXPECT_EQ(orip_get_active_count(parser), 1u);

    orip_clear(parser);
    EXPECT_EQ(orip_get_active_count(parser), 0u);
}

// Callback test helper
static int g_callback_count = 0;
static char g_last_uav_id[64] = {0};

static void test_callback(const orip_uav_t* uav, void* user_data) {
    g_callback_count++;
    std::strncpy(g_last_uav_id, uav->id, sizeof(g_last_uav_id) - 1);
    if (user_data) {
        *static_cast<int*>(user_data) = 42;
    }
}

TEST_F(CAPITest, Callbacks) {
    g_callback_count = 0;
    g_last_uav_id[0] = '\0';
    int user_value = 0;

    orip_set_on_new_uav(parser, test_callback, &user_value);

    auto adv = createBasicIDAdvertisement("CALLBACK_TEST");
    orip_result_t result;
    orip_parse(parser, adv.data(), adv.size(), -60, ORIP_TRANSPORT_BT_LEGACY, &result);

    EXPECT_EQ(g_callback_count, 1);
    EXPECT_STREQ(g_last_uav_id, "CALLBACK_TEST");
    EXPECT_EQ(user_value, 42);

    // Clear callback
    orip_set_on_new_uav(parser, nullptr, nullptr);
}
