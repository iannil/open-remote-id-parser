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
