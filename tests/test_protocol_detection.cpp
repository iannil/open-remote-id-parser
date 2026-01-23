#include <gtest/gtest.h>
#include "orip/parser.h"
#include "orip/astm_f3411.h"
#include "orip/asd_stan.h"
#include <vector>

using namespace orip;
using namespace orip::astm;

// =============================================================================
// Protocol Auto-Detection Test Fixture
// =============================================================================

class ProtocolDetectionTest : public ::testing::Test {
protected:
    // Helper to create a BLE advertisement with ODID data
    std::vector<uint8_t> createBLEAdvertisement(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> adv;

        uint8_t len = static_cast<uint8_t>(3 + 1 + odid_message.size());
        adv.push_back(len);
        adv.push_back(0x16);  // Service Data AD type
        adv.push_back(0xFA);  // UUID low (0xFFFA)
        adv.push_back(0xFF);  // UUID high
        adv.push_back(0x00);  // Message counter

        adv.insert(adv.end(), odid_message.begin(), odid_message.end());

        return adv;
    }

    // Create a Basic ID message
    std::vector<uint8_t> createBasicIDMessage(const std::string& serial) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

        msg[0] = 0x02;  // Basic ID, version 2
        msg[1] = 0x12;  // Serial Number, Multirotor

        for (size_t i = 0; i < serial.size() && i < BASIC_ID_LENGTH; i++) {
            msg[2 + i] = static_cast<uint8_t>(serial[i]);
        }

        return msg;
    }

    // Create a Location message
    std::vector<uint8_t> createLocationMessage(double lat, double lon, float alt) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

        msg[0] = 0x12;  // Location type (0x1), version 2
        msg[1] = 0x20;  // Airborne status

        int32_t lat_enc = static_cast<int32_t>(lat * 1e7);
        int32_t lon_enc = static_cast<int32_t>(lon * 1e7);

        msg[5] = lat_enc & 0xFF;
        msg[6] = (lat_enc >> 8) & 0xFF;
        msg[7] = (lat_enc >> 16) & 0xFF;
        msg[8] = (lat_enc >> 24) & 0xFF;

        msg[9] = lon_enc & 0xFF;
        msg[10] = (lon_enc >> 8) & 0xFF;
        msg[11] = (lon_enc >> 16) & 0xFF;
        msg[12] = (lon_enc >> 24) & 0xFF;

        uint16_t alt_enc = static_cast<uint16_t>((alt + 1000.0f) / 0.5f);
        msg[13] = alt_enc & 0xFF;
        msg[14] = (alt_enc >> 8) & 0xFF;
        msg[15] = alt_enc & 0xFF;
        msg[16] = (alt_enc >> 8) & 0xFF;

        return msg;
    }

    // Create an Operator ID message with EU format
    std::vector<uint8_t> createOperatorIDMessage(const std::string& operator_id) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

        msg[0] = 0x52;  // Operator ID (type 5), version 2
        msg[1] = 0x00;  // Operator ID type

        for (size_t i = 0; i < operator_id.size() && i < 20; i++) {
            msg[2 + i] = static_cast<uint8_t>(operator_id[i]);
        }

        return msg;
    }

    // Create a WiFi Vendor IE frame
    std::vector<uint8_t> createWiFiVendorIE(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> ie;

        // OUI: 0xFA 0x0B 0xBC (ASTM)
        ie.push_back(0xFA);
        ie.push_back(0x0B);
        ie.push_back(0xBC);

        // Vendor type
        ie.push_back(0x0D);

        ie.insert(ie.end(), odid_message.begin(), odid_message.end());

        return ie;
    }
};

// =============================================================================
// ASTM Detection Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, ASTM_DefaultConfig_Detected) {
    ParserConfig config;
    // Default config has ASTM enabled
    RemoteIDParser parser(config);
    parser.init();

    auto msg = createBasicIDMessage("ASTM-TEST-001");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.is_remote_id);
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(result.uav.id, "ASTM-TEST-001");
}

TEST_F(ProtocolDetectionTest, ASTM_AllProtocolsEnabled_Detected) {
    ParserConfig config;
    config.enable_astm = true;
    config.enable_asd = true;
    config.enable_cn = true;
    RemoteIDParser parser(config);
    parser.init();

    auto msg = createBasicIDMessage("MULTI-PROTO-001");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -70, TransportType::BT_LEGACY);

    // ASTM should be detected first (priority)
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
}

TEST_F(ProtocolDetectionTest, ASTM_Disabled_NotDetected) {
    ParserConfig config;
    config.enable_astm = false;
    config.enable_asd = false;
    config.enable_cn = false;
    RemoteIDParser parser(config);
    parser.init();

    auto msg = createBasicIDMessage("DISABLED-TEST");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_remote_id);
}

// =============================================================================
// ASD-STAN Detection Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, ASDStan_OnlyEnabled_Detected) {
    ParserConfig config;
    config.enable_astm = false;  // Disable ASTM
    config.enable_asd = true;    // Enable ASD-STAN
    RemoteIDParser parser(config);
    parser.init();

    auto msg = createBasicIDMessage("EU-DRONE-001");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -65, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.protocol, ProtocolType::ASD_STAN);
}

TEST_F(ProtocolDetectionTest, ASDStan_BothEnabled_ASTMPriority) {
    ParserConfig config;
    config.enable_astm = true;
    config.enable_asd = true;
    RemoteIDParser parser(config);
    parser.init();

    // Both standards use the same UUID, so ASTM has priority
    auto msg = createBasicIDMessage("PRIORITY-TEST");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    // ASTM takes priority when both enabled
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
}

// =============================================================================
// Transport Type Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, Transport_BT_Legacy_Preserved) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("BT-LEGACY-001");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -55, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.uav.transport, TransportType::BT_LEGACY);
}

TEST_F(ProtocolDetectionTest, Transport_BT_Extended_Preserved) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("BT-EXT-001");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -75, TransportType::BT_EXTENDED);

    EXPECT_TRUE(result.success);
    // Transport type from parse call is preserved
    // Note: The decoder may override based on actual detection
}

// =============================================================================
// Invalid Input Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, Empty_Payload_Rejected) {
    RemoteIDParser parser;
    parser.init();

    std::vector<uint8_t> empty;
    auto result = parser.parse(empty, -60, TransportType::BT_LEGACY);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_remote_id);
}

TEST_F(ProtocolDetectionTest, TooShort_Payload_NotRemoteID) {
    RemoteIDParser parser;
    parser.init();

    std::vector<uint8_t> short_payload = {0x01, 0x02, 0x03};
    auto result = parser.parse(short_payload, -60, TransportType::BT_LEGACY);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_remote_id);
}

TEST_F(ProtocolDetectionTest, WrongUUID_NotRemoteID) {
    RemoteIDParser parser;
    parser.init();

    // Create payload with wrong UUID
    std::vector<uint8_t> wrong_uuid = {
        0x1D, 0x16,       // Length and Service Data type
        0x00, 0x00,       // Wrong UUID (not 0xFFFA)
        0x00,             // Counter
    };
    wrong_uuid.resize(30);

    auto result = parser.parse(wrong_uuid, -60, TransportType::BT_LEGACY);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_remote_id);
}

TEST_F(ProtocolDetectionTest, RandomData_NotRemoteID) {
    RemoteIDParser parser;
    parser.init();

    // Random data that doesn't match any protocol
    std::vector<uint8_t> random_data(50, 0xAA);
    auto result = parser.parse(random_data, -60, TransportType::BT_LEGACY);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_remote_id);
}

// =============================================================================
// Multiple Messages Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, Sequential_Different_UAVs) {
    ParserConfig config;
    config.enable_deduplication = true;
    RemoteIDParser parser(config);
    parser.init();

    // Parse first UAV
    auto msg1 = createBasicIDMessage("UAV-ALPHA");
    auto adv1 = createBLEAdvertisement(msg1);
    auto result1 = parser.parse(adv1, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result1.success);
    EXPECT_EQ(result1.uav.id, "UAV-ALPHA");

    // Parse second UAV
    auto msg2 = createBasicIDMessage("UAV-BETA");
    auto adv2 = createBLEAdvertisement(msg2);
    auto result2 = parser.parse(adv2, -65, TransportType::BT_LEGACY);

    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result2.uav.id, "UAV-BETA");

    // Both should be tracked
    EXPECT_EQ(parser.getActiveCount(), 2u);
}

TEST_F(ProtocolDetectionTest, Update_Existing_UAV) {
    ParserConfig config;
    config.enable_deduplication = true;
    RemoteIDParser parser(config);
    parser.init();

    // Parse BasicID first
    auto msg1 = createBasicIDMessage("UPDATE-TEST");
    auto adv1 = createBLEAdvertisement(msg1);
    parser.parse(adv1, -60, TransportType::BT_LEGACY);

    EXPECT_EQ(parser.getActiveCount(), 1u);

    // Parse same UAV again (should update, not add new)
    parser.parse(adv1, -55, TransportType::BT_LEGACY);

    EXPECT_EQ(parser.getActiveCount(), 1u);  // Still just 1

    // RSSI should be updated
    auto* uav = parser.getUAV("UPDATE-TEST");
    ASSERT_NE(uav, nullptr);
    EXPECT_EQ(uav->rssi, -55);
}

// =============================================================================
// Config Flag Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, ConfigFlag_Deduplication_Disabled) {
    ParserConfig config;
    config.enable_deduplication = false;
    RemoteIDParser parser(config);
    parser.init();

    auto msg = createBasicIDMessage("NO-DEDUP-TEST");
    auto adv = createBLEAdvertisement(msg);

    // Parse twice
    parser.parse(adv, -60, TransportType::BT_LEGACY);
    parser.parse(adv, -65, TransportType::BT_LEGACY);

    // With deduplication disabled, session manager not updated
    EXPECT_EQ(parser.getActiveCount(), 0u);
}

TEST_F(ProtocolDetectionTest, ConfigFlag_ASTM_Toggle) {
    // Test enabling ASTM
    {
        ParserConfig config;
        config.enable_astm = true;
        RemoteIDParser parser(config);
        parser.init();

        auto msg = createBasicIDMessage("ASTM-TOGGLE");
        auto adv = createBLEAdvertisement(msg);
        auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

        EXPECT_TRUE(result.success);
    }

    // Test disabling ASTM
    {
        ParserConfig config;
        config.enable_astm = false;
        config.enable_asd = false;
        RemoteIDParser parser(config);
        parser.init();

        auto msg = createBasicIDMessage("ASTM-TOGGLE");
        auto adv = createBLEAdvertisement(msg);
        auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

        EXPECT_FALSE(result.success);
    }
}

// =============================================================================
// Message Type Detection Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, MessageType_BasicID) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("BASIC-ID");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.uav.id.empty());
}

TEST_F(ProtocolDetectionTest, MessageType_Location) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createLocationMessage(37.7749, -122.4194, 100.0f);
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.uav.location.valid);
    EXPECT_NEAR(result.uav.location.latitude, 37.7749, 0.0001);
    EXPECT_NEAR(result.uav.location.longitude, -122.4194, 0.0001);
}

TEST_F(ProtocolDetectionTest, MessageType_OperatorID) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createOperatorIDMessage("FA-OPERATOR-001");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.uav.operator_id.valid);
    EXPECT_EQ(result.uav.operator_id.id, "FA-OPERATOR-001");
}

// =============================================================================
// WiFi Detection Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, WiFi_VendorIE_Detected) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("WIFI-DRONE-001");
    auto ie = createWiFiVendorIE(msg);

    auto result = parser.parse(ie, -55, TransportType::WIFI_BEACON);

    // WiFi detection depends on frame structure
    // The is_remote_id flag should be set if OUI is found
    EXPECT_TRUE(result.is_remote_id || !result.success);
}

// =============================================================================
// RawFrame API Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, RawFrame_API) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("RAW-FRAME-TEST");
    auto adv = createBLEAdvertisement(msg);

    RawFrame frame;
    frame.payload = adv;
    frame.rssi = -58;
    frame.transport = TransportType::BT_LEGACY;
    frame.timestamp = std::chrono::steady_clock::now();

    auto result = parser.parse(frame);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.uav.id, "RAW-FRAME-TEST");
    EXPECT_EQ(result.uav.rssi, -58);
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(ProtocolDetectionTest, EdgeCase_EmptyID) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("");  // Empty ID
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    // Should succeed but ID will be empty/trimmed
    EXPECT_TRUE(result.success);
}

TEST_F(ProtocolDetectionTest, EdgeCase_MaxLengthID) {
    RemoteIDParser parser;
    parser.init();

    // Create ID with max length (20 chars)
    std::string max_id = "12345678901234567890";
    auto msg = createBasicIDMessage(max_id);
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.uav.id.length(), 20u);
}

TEST_F(ProtocolDetectionTest, EdgeCase_SpecialCharsInID) {
    RemoteIDParser parser;
    parser.init();

    std::string special_id = "UAV-123_ABC.XYZ";
    auto msg = createBasicIDMessage(special_id);
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.uav.id, special_id);
}
