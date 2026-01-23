#include <gtest/gtest.h>
#include "orip/astm_f3411.h"
#include "orip/parser.h"
#include "orip/wifi_decoder.h"
#include <cmath>
#include <limits>

using namespace orip;
using namespace orip::astm;

// =============================================================================
// Boundary Condition Tests for ORIP
// Tests for edge cases, limits, and error handling
// =============================================================================

class BoundaryTest : public ::testing::Test {
protected:
    ASTM_F3411_Decoder astm_decoder;
    WiFiDecoder wifi_decoder;

    // Helper to create minimal BLE advertisement
    std::vector<uint8_t> createBLEAdv(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> adv;
        uint8_t len = static_cast<uint8_t>(3 + 1 + odid_message.size());
        adv.push_back(len);
        adv.push_back(0x16);  // Service Data AD type
        adv.push_back(0xFA);  // UUID low
        adv.push_back(0xFF);  // UUID high
        adv.push_back(0x00);  // Counter
        adv.insert(adv.end(), odid_message.begin(), odid_message.end());
        return adv;
    }

    // Create location message with specific coordinates
    std::vector<uint8_t> createLocationMsg(double lat, double lon, float alt = 100.0f) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
        msg[0] = 0x12;  // Location message
        msg[1] = 0x20;  // Airborne

        int32_t lat_enc = static_cast<int32_t>(lat * 1e7);
        msg[5] = lat_enc & 0xFF;
        msg[6] = (lat_enc >> 8) & 0xFF;
        msg[7] = (lat_enc >> 16) & 0xFF;
        msg[8] = (lat_enc >> 24) & 0xFF;

        int32_t lon_enc = static_cast<int32_t>(lon * 1e7);
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
};

// =============================================================================
// Empty and Minimal Input Tests
// =============================================================================

TEST_F(BoundaryTest, EmptyPayload) {
    std::vector<uint8_t> empty;
    EXPECT_FALSE(astm_decoder.isRemoteID(empty));

    UAVObject uav;
    auto result = astm_decoder.decode(empty, uav);
    EXPECT_FALSE(result.success);
}

TEST_F(BoundaryTest, SingleBytePayload) {
    std::vector<uint8_t> single = {0x00};
    EXPECT_FALSE(astm_decoder.isRemoteID(single));
}

TEST_F(BoundaryTest, MinimalValidHeader) {
    // Just the header, no message data
    std::vector<uint8_t> header_only = {0x04, 0x16, 0xFA, 0xFF, 0x00};
    EXPECT_FALSE(astm_decoder.isRemoteID(header_only));
}

TEST_F(BoundaryTest, PayloadTooShortForMessage) {
    // Header + partial message (less than MESSAGE_SIZE)
    std::vector<uint8_t> partial = {0x08, 0x16, 0xFA, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00};

    UAVObject uav;
    auto result = astm_decoder.decode(partial, uav);
    EXPECT_FALSE(result.success);
}

// =============================================================================
// Coordinate Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, MaxLatitude_NorthPole) {
    auto msg = createLocationMsg(90.0, 0.0);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.latitude, 90.0, 0.00001);
}

TEST_F(BoundaryTest, MinLatitude_SouthPole) {
    auto msg = createLocationMsg(-90.0, 0.0);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.latitude, -90.0, 0.00001);
}

TEST_F(BoundaryTest, MaxLongitude_DateLine) {
    auto msg = createLocationMsg(0.0, 180.0);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.longitude, 180.0, 0.00001);
}

TEST_F(BoundaryTest, MinLongitude_DateLine) {
    auto msg = createLocationMsg(0.0, -180.0);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.longitude, -180.0, 0.00001);
}

TEST_F(BoundaryTest, ZeroCoordinates_GulfOfGuinea) {
    auto msg = createLocationMsg(0.0, 0.0);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.latitude, 0.0, 0.00001);
    EXPECT_NEAR(uav.location.longitude, 0.0, 0.00001);
}

// =============================================================================
// Altitude Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, MinAltitude_BelowSeaLevel) {
    // Dead Sea area (-430m)
    auto msg = createLocationMsg(31.5, 35.5, -430.0f);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.altitude_baro, -430.0f, 1.0f);
}

TEST_F(BoundaryTest, MaxAltitude_HighAltitude) {
    // Near max encoding value (~31000m)
    auto msg = createLocationMsg(0.0, 0.0, 30000.0f);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.altitude_baro, 30000.0f, 1.0f);
}

TEST_F(BoundaryTest, ZeroAltitude_SeaLevel) {
    auto msg = createLocationMsg(0.0, 0.0, 0.0f);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.altitude_baro, 0.0f, 0.5f);
}

// =============================================================================
// Speed Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, ZeroSpeed_Stationary) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x12;  // Location
    msg[1] = 0x00;  // On ground
    msg[3] = 0x00;  // Speed = 0

    auto adv = createBLEAdv(msg);
    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.location.speed_horizontal, 0.0f);
}

TEST_F(BoundaryTest, MaxSpeed_HighVelocity) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x12;  // Location
    msg[1] = 0x20;  // Airborne
    msg[3] = 0xFF;  // Speed = 255 * 0.25 = 63.75 m/s

    auto adv = createBLEAdv(msg);
    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_NEAR(uav.location.speed_horizontal, 63.75f, 0.25f);
}

// =============================================================================
// Invalid Message Type Tests
// =============================================================================

TEST_F(BoundaryTest, InvalidMessageType_0x0E) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0xE2;  // Invalid message type 0x0E

    auto adv = createBLEAdv(msg);
    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    // Should either fail or return unknown type
    if (result.success) {
        EXPECT_EQ(result.type, MessageType::UNKNOWN);
    }
}

TEST_F(BoundaryTest, AllZeroMessage) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    // All zeros should parse as Basic ID with empty values
    EXPECT_TRUE(result.success);
}

TEST_F(BoundaryTest, AllFFMessage) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0xFF);
    auto adv = createBLEAdv(msg);

    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    // 0xF is Message Pack type, should handle gracefully
    // Result depends on implementation
}

// =============================================================================
// Basic ID Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, BasicID_EmptySerialNumber) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;  // Basic ID
    msg[1] = 0x11;  // Serial number type, multirotor
    // All zeros for serial number

    auto adv = createBLEAdv(msg);
    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(uav.id.empty() || uav.id[0] == '\0');
}

TEST_F(BoundaryTest, BasicID_MaxLengthSerial) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;  // Basic ID
    msg[1] = 0x11;  // Serial number

    // Fill with max length serial (20 chars)
    for (int i = 0; i < 20; i++) {
        msg[2 + i] = 'A' + (i % 26);
    }

    auto adv = createBLEAdv(msg);
    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id.length(), 20u);
}

TEST_F(BoundaryTest, BasicID_NonPrintableChars) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;
    msg[1] = 0x11;

    // Include non-printable characters
    msg[2] = 0x01;  // SOH
    msg[3] = 0x7F;  // DEL
    msg[4] = 0x80;  // Extended ASCII

    auto adv = createBLEAdv(msg);
    UAVObject uav;
    auto result = astm_decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    // Should handle non-printable characters gracefully
}

// =============================================================================
// WiFi Decoder Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, WiFi_EmptyFrame) {
    std::vector<uint8_t> empty;
    EXPECT_FALSE(wifi_decoder.isRemoteID(empty));
}

TEST_F(BoundaryTest, WiFi_TruncatedVendorIE) {
    // Vendor IE with incomplete data
    std::vector<uint8_t> truncated = {0xDD, 0x03, 0xFA, 0x0B, 0x8C};
    EXPECT_FALSE(wifi_decoder.isRemoteID(truncated));
}

TEST_F(BoundaryTest, WiFi_InvalidOUI) {
    // Valid IE structure but wrong OUI
    std::vector<uint8_t> wrong_oui = {0xDD, 0x10, 0x00, 0x00, 0x00, 0x00};
    wrong_oui.resize(18, 0);

    EXPECT_FALSE(wifi_decoder.isRemoteID(wrong_oui));
}

// =============================================================================
// Parser Integration Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, Parser_NullInput) {
    RemoteIDParser parser;
    parser.init();

    std::vector<uint8_t> empty;
    auto result = parser.parse(empty, -70, TransportType::BT_LEGACY);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.is_remote_id);
}

TEST_F(BoundaryTest, Parser_ExtremeRSSI) {
    RemoteIDParser parser;
    parser.init();

    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;
    msg[1] = 0x11;
    for (int i = 0; i < 7; i++) msg[2 + i] = "TEST123"[i];

    auto adv = createBLEAdv(msg);

    // Test extreme RSSI values
    auto result1 = parser.parse(adv, 0, TransportType::BT_LEGACY);  // Max RSSI
    EXPECT_TRUE(result1.success);
    EXPECT_EQ(result1.uav.rssi, 0);

    auto result2 = parser.parse(adv, -127, TransportType::BT_LEGACY);  // Min RSSI
    EXPECT_TRUE(result2.success);
    EXPECT_EQ(result2.uav.rssi, -127);
}

TEST_F(BoundaryTest, Parser_DisabledProtocols) {
    ParserConfig config;
    config.enable_astm = false;
    config.enable_asd_stan = false;

    RemoteIDParser parser(config);
    parser.init();

    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;
    auto adv = createBLEAdv(msg);

    auto result = parser.parse(adv, -70, TransportType::BT_LEGACY);
    // Should not recognize as Remote ID with protocols disabled
    EXPECT_FALSE(result.is_remote_id);
}

// =============================================================================
// Session Manager Boundary Tests
// =============================================================================

TEST_F(BoundaryTest, Session_ManyUAVs) {
    ParserConfig config;
    config.enable_deduplication = true;
    RemoteIDParser parser(config);
    parser.init();

    // Add 100 different UAVs
    for (int i = 0; i < 100; i++) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
        msg[0] = 0x02;
        msg[1] = 0x11;

        std::string id = "UAV" + std::to_string(i);
        for (size_t j = 0; j < id.size(); j++) {
            msg[2 + j] = id[j];
        }

        auto adv = createBLEAdv(msg);
        parser.parse(adv, -70 - (i % 30), TransportType::BT_LEGACY);
    }

    EXPECT_EQ(parser.getActiveCount(), 100u);

    auto uavs = parser.getActiveUAVs();
    EXPECT_EQ(uavs.size(), 100u);
}

TEST_F(BoundaryTest, Session_DuplicateUpdates) {
    ParserConfig config;
    config.enable_deduplication = true;
    RemoteIDParser parser(config);
    parser.init();

    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
    msg[0] = 0x02;
    msg[1] = 0x11;
    for (int i = 0; i < 7; i++) msg[2 + i] = "SAME123"[i];

    auto adv = createBLEAdv(msg);

    // Send same UAV 100 times
    for (int i = 0; i < 100; i++) {
        parser.parse(adv, -70, TransportType::BT_LEGACY);
    }

    // Should still be only 1 UAV
    EXPECT_EQ(parser.getActiveCount(), 1u);
}
