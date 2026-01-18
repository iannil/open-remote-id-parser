#ifndef ORIP_ASTM_F3411_H
#define ORIP_ASTM_F3411_H

#include "orip/types.h"
#include <cstdint>
#include <vector>

namespace orip {
namespace astm {

// ASTM F3411 Message Types
enum class MessageType : uint8_t {
    BASIC_ID = 0x0,
    LOCATION = 0x1,
    AUTH = 0x2,
    SELF_ID = 0x3,
    SYSTEM = 0x4,
    OPERATOR_ID = 0x5,
    MESSAGE_PACK = 0xF
};

// Message size constants
constexpr size_t MESSAGE_SIZE = 25;         // Standard message size
constexpr size_t MESSAGE_PACK_SIZE = 250;   // Message pack max size
constexpr size_t BASIC_ID_LENGTH = 20;      // ID string length
constexpr size_t SELF_ID_LENGTH = 23;       // Self-ID description length
constexpr size_t OPERATOR_ID_LENGTH = 20;   // Operator ID length

// Open Drone ID Bluetooth AD Type
constexpr uint8_t ODID_AD_TYPE = 0x16;      // Service Data - 16 bit UUID
constexpr uint16_t ODID_SERVICE_UUID = 0xFFFA;  // ASTM Remote ID UUID

// BT 5.x Long Range / Extended Advertising
constexpr uint8_t ODID_AD_TYPE_LONG_RANGE = 0x16;  // Same AD type
constexpr uint8_t BT5_EXT_ADV_DATA_TYPE = 0xFF;    // Manufacturer Specific Data

// Bluetooth 5 Extended Advertising header structure
struct BT5ExtHeader {
    uint8_t adv_mode;           // Advertising mode
    uint8_t adv_addr[6];        // Advertiser address
    uint8_t target_addr[6];     // Target address (if directed)
    uint8_t adv_sid;            // Advertising Set ID
    int8_t tx_power;            // TX power
    uint8_t rssi;               // RSSI
    uint16_t periodic_interval; // Periodic advertising interval
    uint8_t data_status;        // Data status
};

// Decode result
struct DecodeResult {
    bool success = false;
    MessageType type = MessageType::BASIC_ID;
    std::string error;
};

/**
 * ASTM F3411 Decoder class
 *
 * Supports:
 * - Bluetooth 4.x Legacy Advertising
 * - Bluetooth 5.x Extended Advertising
 * - Bluetooth 5.x Long Range (Coded PHY)
 */
class ASTM_F3411_Decoder {
public:
    ASTM_F3411_Decoder() = default;

    /**
     * Check if payload looks like ASTM F3411 Remote ID
     * Supports both legacy and extended advertising
     */
    bool isRemoteID(const std::vector<uint8_t>& payload) const;

    /**
     * Check if payload is BT5 Extended Advertising
     */
    bool isExtendedAdvertising(const std::vector<uint8_t>& payload) const;

    /**
     * Decode a complete advertisement/beacon payload
     * Auto-detects legacy vs extended advertising
     */
    DecodeResult decode(const std::vector<uint8_t>& payload, UAVObject& uav);

    /**
     * Decode BT5 Extended Advertising payload
     */
    DecodeResult decodeExtended(const std::vector<uint8_t>& payload, UAVObject& uav);

    /**
     * Decode legacy advertising payload
     */
    DecodeResult decodeLegacy(const std::vector<uint8_t>& payload, UAVObject& uav);

    /**
     * Decode a single message (25 bytes)
     */
    DecodeResult decodeMessage(const uint8_t* data, size_t len, UAVObject& uav);

private:
    // Individual message type decoders
    bool decodeBasicID(const uint8_t* data, UAVObject& uav);
    bool decodeLocation(const uint8_t* data, UAVObject& uav);
    bool decodeAuth(const uint8_t* data, UAVObject& uav);
    bool decodeSelfID(const uint8_t* data, UAVObject& uav);
    bool decodeSystem(const uint8_t* data, UAVObject& uav);
    bool decodeOperatorID(const uint8_t* data, UAVObject& uav);
    bool decodeMessagePack(const uint8_t* data, size_t len, UAVObject& uav);

    // Helper functions
    double decodeLatitude(int32_t encoded) const;
    double decodeLongitude(int32_t encoded) const;
    float decodeAltitude(uint16_t encoded) const;
    float decodeSpeed(uint8_t encoded, bool multiplier) const;
    float decodeVerticalSpeed(int8_t encoded) const;
    float decodeDirection(uint8_t encoded) const;

    // Find ODID data in advertisement
    bool findODIDData(const std::vector<uint8_t>& payload,
                      const uint8_t*& data, size_t& len) const;
};

} // namespace astm
} // namespace orip

#endif // ORIP_ASTM_F3411_H
