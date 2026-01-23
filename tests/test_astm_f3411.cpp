#include <gtest/gtest.h>
#include "orip/astm_f3411.h"
#include "orip/parser.h"
#include <cmath>

using namespace orip;
using namespace orip::astm;

class ASTM_F3411_Test : public ::testing::Test {
protected:
    ASTM_F3411_Decoder decoder;

    // Helper to create a BLE advertisement with ODID data
    std::vector<uint8_t> createBLEAdvertisement(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> adv;

        // AD structure: [length][type 0x16][UUID low][UUID high][counter][message...]
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
    std::vector<uint8_t> createBasicIDMessage(const std::string& serial,
                                               UAVIdType id_type = UAVIdType::SERIAL_NUMBER,
                                               UAVType uav_type = UAVType::HELICOPTER_OR_MULTIROTOR) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

        // Byte 0: Message type (0x0) | Protocol version (0x2)
        msg[0] = 0x02;

        // Byte 1: ID type | UA type
        msg[1] = (static_cast<uint8_t>(id_type) << 4) | static_cast<uint8_t>(uav_type);

        // Bytes 2-21: Serial number (20 chars)
        for (size_t i = 0; i < serial.size() && i < BASIC_ID_LENGTH; i++) {
            msg[2 + i] = static_cast<uint8_t>(serial[i]);
        }

        return msg;
    }

    // Create a Location message
    std::vector<uint8_t> createLocationMessage(double lat, double lon, float alt,
                                                float speed_h, float speed_v,
                                                float direction) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

        // Byte 0: Message type (0x1) | Protocol version
        msg[0] = 0x12;

        // Byte 1: Status (airborne) | Height type | EW | Speed mult
        msg[1] = 0x20;  // Airborne status

        // Byte 2: Direction (degrees)
        msg[2] = static_cast<uint8_t>(direction);

        // Byte 3: Horizontal speed (0.25 m/s units)
        msg[3] = static_cast<uint8_t>(speed_h / 0.25f);

        // Byte 4: Vertical speed (0.5 m/s units, signed)
        msg[4] = static_cast<uint8_t>(static_cast<int8_t>(speed_v / 0.5f));

        // Bytes 5-8: Latitude (1e-7 degrees)
        int32_t lat_enc = static_cast<int32_t>(lat * 1e7);
        msg[5] = lat_enc & 0xFF;
        msg[6] = (lat_enc >> 8) & 0xFF;
        msg[7] = (lat_enc >> 16) & 0xFF;
        msg[8] = (lat_enc >> 24) & 0xFF;

        // Bytes 9-12: Longitude
        int32_t lon_enc = static_cast<int32_t>(lon * 1e7);
        msg[9] = lon_enc & 0xFF;
        msg[10] = (lon_enc >> 8) & 0xFF;
        msg[11] = (lon_enc >> 16) & 0xFF;
        msg[12] = (lon_enc >> 24) & 0xFF;

        // Bytes 13-14: Barometric altitude (0.5m units, offset -1000m)
        uint16_t alt_enc = static_cast<uint16_t>((alt + 1000.0f) / 0.5f);
        msg[13] = alt_enc & 0xFF;
        msg[14] = (alt_enc >> 8) & 0xFF;

        // Bytes 15-16: Geodetic altitude
        msg[15] = alt_enc & 0xFF;
        msg[16] = (alt_enc >> 8) & 0xFF;

        // Bytes 17-18: Height
        msg[17] = alt_enc & 0xFF;
        msg[18] = (alt_enc >> 8) & 0xFF;

        return msg;
    }
};

TEST_F(ASTM_F3411_Test, IsRemoteID_ValidODID) {
    auto msg = createBasicIDMessage("DJI123456789012");
    auto adv = createBLEAdvertisement(msg);

    EXPECT_TRUE(decoder.isRemoteID(adv));
}

TEST_F(ASTM_F3411_Test, IsRemoteID_InvalidPayload) {
    std::vector<uint8_t> invalid = {0x01, 0x02, 0x03};
    EXPECT_FALSE(decoder.isRemoteID(invalid));
}

TEST_F(ASTM_F3411_Test, IsRemoteID_WrongUUID) {
    std::vector<uint8_t> wrong_uuid = {
        0x05, 0x16, 0x00, 0x00, 0x00, 0x00  // Wrong UUID
    };
    EXPECT_FALSE(decoder.isRemoteID(wrong_uuid));
}

TEST_F(ASTM_F3411_Test, DecodeBasicID) {
    std::string serial = "DJI1234567890ABC";
    auto msg = createBasicIDMessage(serial, UAVIdType::SERIAL_NUMBER,
                                     UAVType::HELICOPTER_OR_MULTIROTOR);
    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::BASIC_ID);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.id_type, UAVIdType::SERIAL_NUMBER);
    EXPECT_EQ(uav.uav_type, UAVType::HELICOPTER_OR_MULTIROTOR);
}

TEST_F(ASTM_F3411_Test, DecodeLocation) {
    double lat = 37.7749;
    double lon = -122.4194;
    float alt = 100.0f;
    float speed_h = 10.0f;
    float speed_v = 2.0f;
    float direction = 45.0f;

    auto msg = createLocationMessage(lat, lon, alt, speed_h, speed_v, direction);
    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::LOCATION);
    EXPECT_TRUE(uav.location.valid);

    // Check coordinates (within tolerance due to encoding)
    EXPECT_NEAR(uav.location.latitude, lat, 0.00001);
    EXPECT_NEAR(uav.location.longitude, lon, 0.00001);

    // Check altitude (within 0.5m due to encoding)
    EXPECT_NEAR(uav.location.altitude_baro, alt, 0.5f);

    // Check speed (within 0.25 m/s due to encoding)
    EXPECT_NEAR(uav.location.speed_horizontal, speed_h, 0.25f);
    EXPECT_NEAR(uav.location.speed_vertical, speed_v, 0.5f);

    // Check direction
    EXPECT_EQ(uav.location.direction, direction);
}

TEST_F(ASTM_F3411_Test, DecodeMessage_TooShort) {
    std::vector<uint8_t> short_msg = {0x00, 0x01, 0x02};

    UAVObject uav;
    auto result = decoder.decodeMessage(short_msg.data(), short_msg.size(), uav);

    EXPECT_FALSE(result.success);
}

TEST_F(ASTM_F3411_Test, ParserIntegration) {
    RemoteIDParser parser;
    parser.init();

    auto msg = createBasicIDMessage("TEST123");
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -70, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.is_remote_id);
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(result.uav.id, "TEST123");
    EXPECT_EQ(result.uav.rssi, -70);
}

TEST_F(ASTM_F3411_Test, SessionManagerTracking) {
    ParserConfig config;
    config.enable_deduplication = true;
    RemoteIDParser parser(config);
    parser.init();

    // Send first message
    auto msg1 = createBasicIDMessage("UAV001");
    auto adv1 = createBLEAdvertisement(msg1);
    parser.parse(adv1, -60, TransportType::BT_LEGACY);

    // Send second message from different UAV
    auto msg2 = createBasicIDMessage("UAV002");
    auto adv2 = createBLEAdvertisement(msg2);
    parser.parse(adv2, -70, TransportType::BT_LEGACY);

    EXPECT_EQ(parser.getActiveCount(), 2u);

    auto uavs = parser.getActiveUAVs();
    EXPECT_EQ(uavs.size(), 2u);

    // Check we can find specific UAV
    auto* uav1 = parser.getUAV("UAV001");
    ASSERT_NE(uav1, nullptr);
    EXPECT_EQ(uav1->rssi, -60);
}

// =============================================================================
// Authentication Message Tests (Type 0x2)
// =============================================================================

TEST_F(ASTM_F3411_Test, DecodeAuthentication_Basic) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    // Byte 0: Message type (0x2) | Protocol version (0x2)
    msg[0] = 0x22;

    // Byte 1: Auth type (0x0 = None) | Page number (0)
    msg[1] = 0x00;

    // Bytes 2-24: Auth data
    for (int i = 2; i < MESSAGE_SIZE; i++) {
        msg[i] = static_cast<uint8_t>(i - 2);
    }

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::AUTH);
    EXPECT_FALSE(uav.auth_data.empty());
}

TEST_F(ASTM_F3411_Test, DecodeAuthentication_WithTimestamp) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    // Message type AUTH
    msg[0] = 0x22;

    // Auth type 1 (UAS ID Signature), Page 0
    msg[1] = 0x10;

    // Page count in byte 2
    msg[2] = 0x01;

    // Timestamp (4 bytes, starting byte 3)
    uint32_t timestamp = 1234567890;
    msg[3] = timestamp & 0xFF;
    msg[4] = (timestamp >> 8) & 0xFF;
    msg[5] = (timestamp >> 16) & 0xFF;
    msg[6] = (timestamp >> 24) & 0xFF;

    // Auth data in remaining bytes
    for (int i = 7; i < MESSAGE_SIZE; i++) {
        msg[i] = 0xAA;
    }

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::AUTH);
}

// =============================================================================
// Self-ID Message Tests (Type 0x3)
// =============================================================================

TEST_F(ASTM_F3411_Test, DecodeSelfID_TextDescription) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    // Byte 0: Message type (0x3) | Protocol version
    msg[0] = 0x32;

    // Byte 1: Description type (0 = Text)
    msg[1] = 0x00;

    // Bytes 2-24: Description text (23 chars max)
    std::string desc = "Survey mission flight";
    for (size_t i = 0; i < desc.size() && i < 23; i++) {
        msg[2 + i] = static_cast<uint8_t>(desc[i]);
    }

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::SELF_ID);
    EXPECT_TRUE(uav.self_id.valid);
    EXPECT_EQ(uav.self_id.description_type, 0);
    EXPECT_EQ(uav.self_id.description, desc);
}

TEST_F(ASTM_F3411_Test, DecodeSelfID_EmergencyType) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    // Message type SELF_ID
    msg[0] = 0x32;

    // Description type 1 (Emergency)
    msg[1] = 0x01;

    // Emergency description
    std::string desc = "EMERGENCY - LOW BATTERY";
    for (size_t i = 0; i < desc.size() && i < 23; i++) {
        msg[2 + i] = static_cast<uint8_t>(desc[i]);
    }

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::SELF_ID);
    EXPECT_EQ(uav.self_id.description_type, 1);
}

TEST_F(ASTM_F3411_Test, DecodeSelfID_MaxLength) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    msg[0] = 0x32;
    msg[1] = 0x00;

    // Fill with max length (23 chars)
    for (int i = 0; i < 23; i++) {
        msg[2 + i] = 'A' + (i % 26);
    }

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.self_id.description.length(), 23u);
}

// =============================================================================
// System Message Tests (Type 0x4)
// =============================================================================

TEST_F(ASTM_F3411_Test, DecodeSystem_OperatorLocation) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    // Byte 0: Message type (0x4) | Protocol version
    msg[0] = 0x42;

    // Byte 1: Operator location type | Classification type
    msg[1] = 0x10;  // Location type = Takeoff

    // Bytes 2-5: Operator latitude (1e-7 degrees)
    double op_lat = 37.3861;
    int32_t lat_enc = static_cast<int32_t>(op_lat * 1e7);
    msg[2] = lat_enc & 0xFF;
    msg[3] = (lat_enc >> 8) & 0xFF;
    msg[4] = (lat_enc >> 16) & 0xFF;
    msg[5] = (lat_enc >> 24) & 0xFF;

    // Bytes 6-9: Operator longitude
    double op_lon = -122.0839;
    int32_t lon_enc = static_cast<int32_t>(op_lon * 1e7);
    msg[6] = lon_enc & 0xFF;
    msg[7] = (lon_enc >> 8) & 0xFF;
    msg[8] = (lon_enc >> 16) & 0xFF;
    msg[9] = (lon_enc >> 24) & 0xFF;

    // Bytes 10-11: Area count
    msg[10] = 0x01;
    msg[11] = 0x00;

    // Byte 12: Area radius (10m units)
    msg[12] = 0x0A;  // 100m

    // Bytes 13-14: Area ceiling
    uint16_t ceiling_enc = static_cast<uint16_t>((500.0f + 1000.0f) / 0.5f);
    msg[13] = ceiling_enc & 0xFF;
    msg[14] = (ceiling_enc >> 8) & 0xFF;

    // Bytes 15-16: Area floor
    uint16_t floor_enc = static_cast<uint16_t>((0.0f + 1000.0f) / 0.5f);
    msg[15] = floor_enc & 0xFF;
    msg[16] = (floor_enc >> 8) & 0xFF;

    // Bytes 17-20: Timestamp
    uint32_t ts = 1609459200;  // 2021-01-01 00:00:00 UTC
    msg[17] = ts & 0xFF;
    msg[18] = (ts >> 8) & 0xFF;
    msg[19] = (ts >> 16) & 0xFF;
    msg[20] = (ts >> 24) & 0xFF;

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::SYSTEM);
    EXPECT_TRUE(uav.system.valid);
    EXPECT_NEAR(uav.system.operator_latitude, op_lat, 0.00001);
    EXPECT_NEAR(uav.system.operator_longitude, op_lon, 0.00001);
    EXPECT_EQ(uav.system.area_count, 1);
    EXPECT_EQ(uav.system.area_radius, 100);
    EXPECT_NEAR(uav.system.area_ceiling, 500.0f, 0.5f);
    EXPECT_NEAR(uav.system.area_floor, 0.0f, 0.5f);
}

TEST_F(ASTM_F3411_Test, DecodeSystem_LiveGPS) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    msg[0] = 0x42;
    msg[1] = 0x20;  // Location type = Live GPS

    // Operator at Greenwich Observatory
    double op_lat = 51.4769;
    double op_lon = -0.0005;
    int32_t lat_enc = static_cast<int32_t>(op_lat * 1e7);
    int32_t lon_enc = static_cast<int32_t>(op_lon * 1e7);

    msg[2] = lat_enc & 0xFF;
    msg[3] = (lat_enc >> 8) & 0xFF;
    msg[4] = (lat_enc >> 16) & 0xFF;
    msg[5] = (lat_enc >> 24) & 0xFF;
    msg[6] = lon_enc & 0xFF;
    msg[7] = (lon_enc >> 8) & 0xFF;
    msg[8] = (lon_enc >> 16) & 0xFF;
    msg[9] = (lon_enc >> 24) & 0xFF;

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::SYSTEM);
    EXPECT_EQ(uav.system.location_type, OperatorLocationType::LIVE_GNSS);
}

// =============================================================================
// Operator ID Message Tests (Type 0x5)
// =============================================================================

TEST_F(ASTM_F3411_Test, DecodeOperatorID_FAANumber) {
    std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

    // Byte 0: Message type (0x5) | Protocol version
    msg[0] = 0x52;

    // Byte 1: Operator ID type (0 = Operator ID)
    msg[1] = 0x00;

    // Bytes 2-21: Operator ID (20 chars)
    std::string op_id = "FA12345678901234567";
    for (size_t i = 0; i < op_id.size() && i < 20; i++) {
        msg[2 + i] = static_cast<uint8_t>(op_id[i]);
    }

    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::OPERATOR_ID);
    EXPECT_TRUE(uav.operator_id.valid);
    EXPECT_EQ(uav.operator_id.id_type, 0);
    EXPECT_EQ(uav.operator_id.id, op_id);
}

// =============================================================================
// Message Pack Tests (Type 0xF)
// =============================================================================

TEST_F(ASTM_F3411_Test, DecodeMessagePack_TwoMessages) {
    // Message Pack header
    std::vector<uint8_t> pack;

    // Byte 0: Message type (0xF) | Protocol version
    pack.push_back(0xF2);

    // Byte 1: (Message size - 1) << 4 | Message count
    // Message size = 25 bytes, count = 2
    pack.push_back(((MESSAGE_SIZE - 1) << 4) | 0x02);

    // First message: Basic ID
    auto basic_id = createBasicIDMessage("PACK-UAV-001");
    pack.insert(pack.end(), basic_id.begin(), basic_id.end());

    // Second message: Location
    auto location = createLocationMessage(34.0522, -118.2437, 150.0f, 5.0f, 1.0f, 90.0f);
    pack.insert(pack.end(), location.begin(), location.end());

    // Pad to expected size
    while (pack.size() < MESSAGE_SIZE) {
        pack.push_back(0);
    }

    auto adv = createBLEAdvertisement(pack);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.type, MessageType::MESSAGE_PACK);
    // Verify both messages were processed
    EXPECT_EQ(uav.id, "PACK-UAV-001");
    EXPECT_TRUE(uav.location.valid);
}

TEST_F(ASTM_F3411_Test, DecodeMessagePack_AllTypes) {
    // Create a pack with Basic ID + Location + Self-ID
    std::vector<uint8_t> pack;

    pack.push_back(0xF2);  // Message Pack type
    pack.push_back(((MESSAGE_SIZE - 1) << 4) | 0x03);  // 3 messages

    // Basic ID
    auto basic_id = createBasicIDMessage("MULTI-MSG-UAV");
    pack.insert(pack.end(), basic_id.begin(), basic_id.end());

    // Location
    auto location = createLocationMessage(40.7128, -74.0060, 200.0f, 8.0f, -1.0f, 180.0f);
    pack.insert(pack.end(), location.begin(), location.end());

    // Self-ID
    std::vector<uint8_t> self_id(MESSAGE_SIZE, 0);
    self_id[0] = 0x32;
    self_id[1] = 0x00;
    std::string desc = "Multi-msg test";
    for (size_t i = 0; i < desc.size(); i++) {
        self_id[2 + i] = static_cast<uint8_t>(desc[i]);
    }
    pack.insert(pack.end(), self_id.begin(), self_id.end());

    while (pack.size() < MESSAGE_SIZE) {
        pack.push_back(0);
    }

    auto adv = createBLEAdvertisement(pack);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, "MULTI-MSG-UAV");
    EXPECT_TRUE(uav.location.valid);
    EXPECT_TRUE(uav.self_id.valid);
    EXPECT_EQ(uav.self_id.description, desc);
}

TEST_F(ASTM_F3411_Test, DecodeMessagePack_InvalidSize) {
    std::vector<uint8_t> pack;

    pack.push_back(0xF2);
    // Wrong message size (10 instead of 25)
    pack.push_back((9 << 4) | 0x01);

    // Partial data
    for (int i = 0; i < 20; i++) {
        pack.push_back(0);
    }

    auto adv = createBLEAdvertisement(pack);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    // Should handle invalid size gracefully
    EXPECT_FALSE(result.success);
}
