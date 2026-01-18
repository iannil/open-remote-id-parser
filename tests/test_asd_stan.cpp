#include <gtest/gtest.h>
#include "orip/asd_stan.h"
#include "orip/astm_f3411.h"
#include "orip/parser.h"
#include "orip/cn_rid.h"

using namespace orip;
using namespace orip::asdstan;
using namespace orip::astm;

class ASDStanTest : public ::testing::Test {
protected:
    ASD_STAN_Decoder decoder;

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
};

TEST_F(ASDStanTest, IsRemoteID_Valid) {
    auto msg = createBasicIDMessage("EU_DRONE_001");
    auto adv = createBLEAdvertisement(msg);

    EXPECT_TRUE(decoder.isRemoteID(adv));
}

TEST_F(ASDStanTest, IsRemoteID_Invalid) {
    std::vector<uint8_t> invalid = {0x01, 0x02, 0x03};
    EXPECT_FALSE(decoder.isRemoteID(invalid));
}

TEST_F(ASDStanTest, Decode_BasicID) {
    std::string serial = "EU_MULTIROTOR_01";
    auto msg = createBasicIDMessage(serial);
    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, serial);
    EXPECT_EQ(uav.protocol, ProtocolType::ASD_STAN);
}

TEST_F(ASDStanTest, ValidateEUOperatorId_ValidFormat1) {
    // Format: XXX-XX-NNNNNNNN
    EXPECT_TRUE(decoder.validateEUOperatorId("FRA-OP-12345678"));
    EXPECT_TRUE(decoder.validateEUOperatorId("DEU-OP-ABCD1234"));
    EXPECT_TRUE(decoder.validateEUOperatorId("ESP-CA-00001234"));
}

TEST_F(ASDStanTest, ValidateEUOperatorId_ValidFormat2) {
    // Compact format: XXXNNNNNNNNNNNN
    EXPECT_TRUE(decoder.validateEUOperatorId("FRA1234567890AB"));
    EXPECT_TRUE(decoder.validateEUOperatorId("DEU0987654321XY"));
}

TEST_F(ASDStanTest, ValidateEUOperatorId_InvalidFormat) {
    // Too short
    EXPECT_FALSE(decoder.validateEUOperatorId("FR"));

    // Empty
    EXPECT_FALSE(decoder.validateEUOperatorId(""));

    // Invalid country code
    EXPECT_FALSE(decoder.validateEUOperatorId("XXX-OP-12345678"));

    // Lowercase country code
    EXPECT_FALSE(decoder.validateEUOperatorId("fra-OP-12345678"));
}

TEST_F(ASDStanTest, ExtractCountryCode_Valid) {
    EXPECT_EQ(decoder.extractCountryCode("FRA-OP-12345678"), "FRA");
    EXPECT_EQ(decoder.extractCountryCode("DEU1234567890"), "DEU");
    EXPECT_EQ(decoder.extractCountryCode("ESP-CA-00001234"), "ESP");
}

TEST_F(ASDStanTest, ExtractCountryCode_Invalid) {
    EXPECT_EQ(decoder.extractCountryCode("XXX-OP-12345678"), "");
    EXPECT_EQ(decoder.extractCountryCode("12"), "");
    EXPECT_EQ(decoder.extractCountryCode(""), "");
}

TEST_F(ASDStanTest, ExtractCountryCode_EEACountries) {
    // EEA/EFTA countries
    EXPECT_EQ(decoder.extractCountryCode("NOR-OP-12345678"), "NOR");
    EXPECT_EQ(decoder.extractCountryCode("CHE-OP-12345678"), "CHE");
    EXPECT_EQ(decoder.extractCountryCode("ISL-OP-12345678"), "ISL");
}

TEST_F(ASDStanTest, GetClassification_NoSystem) {
    UAVObject uav;
    uav.system.valid = false;

    EXPECT_EQ(decoder.getClassification(uav), EUClassification::UNDEFINED);
}

TEST_F(ASDStanTest, DecodeMessage_OperatorID) {
    std::string operator_id = "FRA-OP-12345678";
    auto msg = createOperatorIDMessage(operator_id);

    UAVObject uav;
    auto result = decoder.decodeMessage(msg.data(), msg.size(), uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.operator_id.id, operator_id);
    EXPECT_EQ(uav.protocol, ProtocolType::ASD_STAN);
}

TEST_F(ASDStanTest, ParserIntegration_ASDEnabled) {
    ParserConfig config;
    config.enable_asd = true;
    config.enable_astm = false;  // Disable ASTM to force ASD
    RemoteIDParser parser(config);
    parser.init();

    std::string serial = "EU_PARSER_TEST";
    auto msg = createBasicIDMessage(serial);
    auto adv = createBLEAdvertisement(msg);

    auto result = parser.parse(adv, -60, TransportType::BT_LEGACY);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.protocol, ProtocolType::ASD_STAN);
    EXPECT_EQ(result.uav.id, serial);
}

// CN-RID placeholder tests
class CNRIDTest : public ::testing::Test {
protected:
    cnrid::CN_RID_Decoder decoder;
};

TEST_F(CNRIDTest, IsImplemented) {
    EXPECT_FALSE(decoder.isImplemented());
}

TEST_F(CNRIDTest, GetStatusMessage) {
    std::string msg = decoder.getStatusMessage();
    EXPECT_FALSE(msg.empty());
    EXPECT_NE(msg.find("placeholder"), std::string::npos);
}

TEST_F(CNRIDTest, IsRemoteID_AlwaysFalse) {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_FALSE(decoder.isRemoteID(payload));
}

TEST_F(CNRIDTest, Decode_NotImplemented) {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    UAVObject uav;

    auto result = decoder.decode(payload, uav);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error.find("not implemented"), std::string::npos);
}

TEST_F(CNRIDTest, ValidateCNOperatorId) {
    // Currently returns false for all (not implemented)
    EXPECT_FALSE(decoder.validateCNOperatorId("CN123456789"));
    EXPECT_FALSE(decoder.validateCNOperatorId(""));
}

TEST_F(CNRIDTest, ParserIntegration_CNEnabled) {
    ParserConfig config;
    config.enable_cn = true;
    config.enable_astm = false;
    config.enable_asd = false;
    RemoteIDParser parser(config);
    parser.init();

    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto result = parser.parse(payload, -70, TransportType::BT_LEGACY);

    // Should fail because CN-RID is not implemented
    EXPECT_FALSE(result.success);
}
