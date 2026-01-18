#include <gtest/gtest.h>
#include "orip/wifi_decoder.h"
#include "orip/astm_f3411.h"
#include "orip/parser.h"
#include <cmath>
#include <cstring>

using namespace orip;
using namespace orip::wifi;
using namespace orip::astm;

class WiFiDecoderTest : public ::testing::Test {
protected:
    WiFiDecoder decoder;
    ASTM_F3411_Decoder astm_decoder;

    // Create a Basic ID ODID message
    std::vector<uint8_t> createBasicIDMessage(const std::string& serial) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);

        // Byte 0: Message type (0x0) | Protocol version (0x2)
        msg[0] = 0x02;

        // Byte 1: ID type (Serial Number = 0x1) | UA type (Multirotor = 0x2)
        msg[1] = 0x12;

        // Bytes 2-21: Serial number (20 chars)
        for (size_t i = 0; i < serial.size() && i < BASIC_ID_LENGTH; i++) {
            msg[2 + i] = static_cast<uint8_t>(serial[i]);
        }

        return msg;
    }

    // Create a WiFi Vendor Specific IE with ASTM OUI
    std::vector<uint8_t> createVendorIE(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> ie;

        // OUI (FA:0B:BC)
        ie.push_back(WIFI_OUI_FA[0]);
        ie.push_back(WIFI_OUI_FA[1]);
        ie.push_back(WIFI_OUI_FA[2]);

        // Vendor Type (0x0D)
        ie.push_back(WIFI_VENDOR_TYPE);

        // ODID message
        ie.insert(ie.end(), odid_message.begin(), odid_message.end());

        return ie;
    }

    // Create a minimal WiFi Beacon frame
    std::vector<uint8_t> createBeaconFrame(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> frame;

        // 802.11 Management frame header (24 bytes)
        // Frame Control (2 bytes) - Beacon = 0x0080
        frame.push_back(0x80);
        frame.push_back(0x00);

        // Duration (2 bytes)
        frame.push_back(0x00);
        frame.push_back(0x00);

        // Destination Address (6 bytes) - broadcast
        for (int i = 0; i < 6; i++) frame.push_back(0xFF);

        // Source Address (6 bytes)
        for (int i = 0; i < 6; i++) frame.push_back(0x00);

        // BSSID (6 bytes)
        for (int i = 0; i < 6; i++) frame.push_back(0x00);

        // Sequence Control (2 bytes)
        frame.push_back(0x00);
        frame.push_back(0x00);

        // Fixed beacon parameters (12 bytes)
        // Timestamp (8 bytes)
        for (int i = 0; i < 8; i++) frame.push_back(0x00);

        // Beacon interval (2 bytes)
        frame.push_back(0x64);
        frame.push_back(0x00);

        // Capability info (2 bytes)
        frame.push_back(0x01);
        frame.push_back(0x00);

        // Vendor Specific IE
        uint8_t ie_len = static_cast<uint8_t>(4 + odid_message.size());
        frame.push_back(221);  // IE ID (Vendor Specific)
        frame.push_back(ie_len);
        frame.push_back(WIFI_OUI_FA[0]);
        frame.push_back(WIFI_OUI_FA[1]);
        frame.push_back(WIFI_OUI_FA[2]);
        frame.push_back(WIFI_VENDOR_TYPE);

        frame.insert(frame.end(), odid_message.begin(), odid_message.end());

        return frame;
    }

    // Create NAN frame with Service ID
    std::vector<uint8_t> createNANFrame(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> frame;

        // NAN Service ID (6 bytes)
        frame.insert(frame.end(), NAN_SERVICE_ID, NAN_SERVICE_ID + 6);

        // ODID message follows
        frame.insert(frame.end(), odid_message.begin(), odid_message.end());

        return frame;
    }
};

TEST_F(WiFiDecoderTest, IsRemoteID_ValidVendorIE) {
    auto msg = createBasicIDMessage("WIFI_DRONE_001");
    auto beacon = createBeaconFrame(msg);

    EXPECT_TRUE(decoder.isRemoteID(beacon));
}

TEST_F(WiFiDecoderTest, IsRemoteID_ValidNAN) {
    auto msg = createBasicIDMessage("NAN_DRONE_001");
    auto nan_frame = createNANFrame(msg);

    EXPECT_TRUE(decoder.isRemoteID(nan_frame));
}

TEST_F(WiFiDecoderTest, IsRemoteID_InvalidPayload) {
    std::vector<uint8_t> invalid = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_FALSE(decoder.isRemoteID(invalid));
}

TEST_F(WiFiDecoderTest, IsRemoteID_WrongOUI) {
    std::vector<uint8_t> wrong_oui = {
        0x00, 0x00, 0x00, 0x0D,  // Wrong OUI
        0x02, 0x12  // Partial message
    };
    EXPECT_FALSE(decoder.isRemoteID(wrong_oui));
}

TEST_F(WiFiDecoderTest, DecodeVendorIE_BasicID) {
    std::string serial = "WIFI_DRONE_12345";
    auto msg = createBasicIDMessage(serial);
    auto ie = createVendorIE(msg);

    UAVObject uav;
    auto result = decoder.decodeVendorIE(ie, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(uav.transport, TransportType::WIFI_BEACON);
}

TEST_F(WiFiDecoderTest, DecodeVendorIE_WrongOUI) {
    std::vector<uint8_t> wrong_ie = {
        0x00, 0x00, 0x00,  // Wrong OUI
        0x0D  // Vendor type
    };

    UAVObject uav;
    auto result = decoder.decodeVendorIE(wrong_ie, uav);

    EXPECT_FALSE(result.success);
}

TEST_F(WiFiDecoderTest, DecodeVendorIE_TooShort) {
    std::vector<uint8_t> short_ie = {0xFA, 0x0B};

    UAVObject uav;
    auto result = decoder.decodeVendorIE(short_ie, uav);

    EXPECT_FALSE(result.success);
}

TEST_F(WiFiDecoderTest, DecodeBeacon_Valid) {
    std::string serial = "BEACON_DRONE_001";
    auto msg = createBasicIDMessage(serial);
    auto beacon = createBeaconFrame(msg);

    UAVObject uav;
    auto result = decoder.decodeBeacon(beacon, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(uav.transport, TransportType::WIFI_BEACON);
}

TEST_F(WiFiDecoderTest, DecodeBeacon_TooShort) {
    std::vector<uint8_t> short_frame(10, 0);

    UAVObject uav;
    auto result = decoder.decodeBeacon(short_frame, uav);

    EXPECT_FALSE(result.success);
}

TEST_F(WiFiDecoderTest, DecodeNAN_Valid) {
    std::string serial = "NAN_DRONE_12345";
    auto msg = createBasicIDMessage(serial);
    auto nan_frame = createNANFrame(msg);

    UAVObject uav;
    auto result = decoder.decodeNAN(nan_frame, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(uav.transport, TransportType::WIFI_NAN);
}

TEST_F(WiFiDecoderTest, DecodeNAN_TooShort) {
    std::vector<uint8_t> short_frame = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};

    UAVObject uav;
    auto result = decoder.decodeNAN(short_frame, uav);

    EXPECT_FALSE(result.success);
}

TEST_F(WiFiDecoderTest, DecodeNAN_WithOUI) {
    std::string serial = "OUI_NAN_DRONE";
    auto msg = createBasicIDMessage(serial);

    std::vector<uint8_t> nan_frame;
    // Add some padding before OUI
    nan_frame.push_back(0x00);
    nan_frame.push_back(0x00);
    // OUI + vendor type
    nan_frame.push_back(WIFI_OUI_FA[0]);
    nan_frame.push_back(WIFI_OUI_FA[1]);
    nan_frame.push_back(WIFI_OUI_FA[2]);
    nan_frame.push_back(WIFI_VENDOR_TYPE);
    nan_frame.insert(nan_frame.end(), msg.begin(), msg.end());

    UAVObject uav;
    auto result = decoder.decodeNAN(nan_frame, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
}

TEST_F(WiFiDecoderTest, ParserIntegration_Beacon) {
    RemoteIDParser parser;
    parser.init();

    std::string serial = "PARSER_WIFI_001";
    auto msg = createBasicIDMessage(serial);
    auto beacon = createBeaconFrame(msg);

    auto result = parser.parse(beacon, -55, TransportType::WIFI_BEACON);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.is_remote_id);
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(result.uav.id, serial);
    EXPECT_EQ(result.uav.rssi, -55);
}

TEST_F(WiFiDecoderTest, ParserIntegration_NAN) {
    RemoteIDParser parser;
    parser.init();

    std::string serial = "PARSER_NAN_001";
    auto msg = createBasicIDMessage(serial);
    auto nan_frame = createNANFrame(msg);

    auto result = parser.parse(nan_frame, -65, TransportType::WIFI_NAN);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.is_remote_id);
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(result.uav.id, serial);
    EXPECT_EQ(result.uav.rssi, -65);
}

// Test BT5 Extended Advertising
class BT5ExtendedTest : public ::testing::Test {
protected:
    ASTM_F3411_Decoder decoder;

    std::vector<uint8_t> createBasicIDMessage(const std::string& serial) {
        std::vector<uint8_t> msg(MESSAGE_SIZE, 0);
        msg[0] = 0x02;  // Basic ID, version 2
        msg[1] = 0x12;  // Serial Number, Multirotor

        for (size_t i = 0; i < serial.size() && i < BASIC_ID_LENGTH; i++) {
            msg[2 + i] = static_cast<uint8_t>(serial[i]);
        }

        return msg;
    }

    // Create BT5 Extended Advertising payload
    std::vector<uint8_t> createExtendedAdv(const std::vector<uint8_t>& odid_message) {
        std::vector<uint8_t> adv;

        // Extended advertising header (simplified)
        adv.push_back(0x00);  // Advertising mode
        adv.push_back(0x00);  // Reserved

        // AD structure with ODID
        uint8_t len = static_cast<uint8_t>(3 + 1 + odid_message.size());
        adv.push_back(len);
        adv.push_back(0x16);  // Service Data AD type
        adv.push_back(0xFA);  // UUID low (0xFFFA)
        adv.push_back(0xFF);  // UUID high
        adv.push_back(0x00);  // Message counter

        adv.insert(adv.end(), odid_message.begin(), odid_message.end());

        return adv;
    }
};

TEST_F(BT5ExtendedTest, IsExtendedAdvertising_Valid) {
    auto msg = createBasicIDMessage("BT5_DRONE_001");
    auto adv = createExtendedAdv(msg);

    EXPECT_TRUE(decoder.isRemoteID(adv));
    EXPECT_TRUE(decoder.isExtendedAdvertising(adv));
}

TEST_F(BT5ExtendedTest, DecodeExtended_BasicID) {
    std::string serial = "BT5_EXT_DRONE_01";
    auto msg = createBasicIDMessage(serial);
    auto adv = createExtendedAdv(msg);

    UAVObject uav;
    auto result = decoder.decodeExtended(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(uav.transport, TransportType::BT_EXTENDED);
}

TEST_F(BT5ExtendedTest, DecodeExtended_TooShort) {
    std::vector<uint8_t> short_adv = {0x00, 0x00, 0x03, 0x16, 0xFA, 0xFF};

    UAVObject uav;
    auto result = decoder.decodeExtended(short_adv, uav);

    EXPECT_FALSE(result.success);
}

TEST_F(BT5ExtendedTest, AutoDetect_Extended) {
    std::string serial = "AUTO_BT5_001";
    auto msg = createBasicIDMessage(serial);
    auto adv = createExtendedAdv(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.transport, TransportType::BT_EXTENDED);
}

TEST_F(BT5ExtendedTest, ParserIntegration_Extended) {
    RemoteIDParser parser;
    parser.init();

    std::string serial = "PARSER_BT5_001";
    auto msg = createBasicIDMessage(serial);
    auto adv = createExtendedAdv(msg);

    auto result = parser.parse(adv, -50, TransportType::BT_EXTENDED);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.is_remote_id);
    EXPECT_EQ(result.protocol, ProtocolType::ASTM_F3411);
    EXPECT_EQ(result.uav.id, serial);
    EXPECT_EQ(result.uav.transport, TransportType::BT_EXTENDED);
}
