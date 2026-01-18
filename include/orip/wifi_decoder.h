#ifndef ORIP_WIFI_DECODER_H
#define ORIP_WIFI_DECODER_H

#include "orip/types.h"
#include <cstdint>
#include <vector>

namespace orip {
namespace wifi {

// WiFi Remote ID constants
constexpr uint8_t WIFI_OUI_FA[] = {0xFA, 0x0B, 0xBC};  // ASTM designated OUI
constexpr uint8_t WIFI_VENDOR_TYPE = 0x0D;             // Remote ID vendor type

// WiFi frame types
enum class FrameType : uint8_t {
    BEACON = 0x80,
    PROBE_RESPONSE = 0x50,
    ACTION = 0xD0
};

// NAN Service ID for Remote ID (SHA-256 hash of "org.opendroneid.remoteid")
constexpr uint8_t NAN_SERVICE_ID[] = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};

// Decode result
struct WiFiDecodeResult {
    bool success = false;
    std::string error;
};

/**
 * WiFi Remote ID Decoder
 *
 * Supports:
 * - WiFi Beacon frames with Vendor Specific IE
 * - WiFi NAN (Neighbor Awareness Networking) frames
 * - WiFi Aware for Android
 */
class WiFiDecoder {
public:
    WiFiDecoder() = default;

    /**
     * Check if payload looks like WiFi Remote ID
     * @param payload Raw WiFi frame or extracted payload
     * @return true if this appears to be Remote ID data
     */
    bool isRemoteID(const std::vector<uint8_t>& payload) const;

    /**
     * Decode WiFi Beacon frame containing Remote ID
     * @param payload Raw 802.11 beacon frame
     * @param uav Output UAV object
     * @return Decode result
     */
    WiFiDecodeResult decodeBeacon(const std::vector<uint8_t>& payload, UAVObject& uav);

    /**
     * Decode WiFi NAN frame containing Remote ID
     * @param payload Raw NAN frame or service data
     * @param uav Output UAV object
     * @return Decode result
     */
    WiFiDecodeResult decodeNAN(const std::vector<uint8_t>& payload, UAVObject& uav);

    /**
     * Decode Vendor Specific IE payload
     * @param payload Vendor IE data (after OUI)
     * @param uav Output UAV object
     * @return Decode result
     */
    WiFiDecodeResult decodeVendorIE(const std::vector<uint8_t>& payload, UAVObject& uav);

private:
    // Parse 802.11 frame header
    bool parseFrameHeader(const uint8_t* data, size_t len, size_t& payload_offset);

    // Find Vendor Specific IE in beacon body
    bool findVendorIE(const uint8_t* data, size_t len,
                      const uint8_t* oui, size_t oui_len,
                      const uint8_t*& ie_data, size_t& ie_len);

    // Decode ASTM message from IE payload
    bool decodeASTMPayload(const uint8_t* data, size_t len, UAVObject& uav);
};

} // namespace wifi
} // namespace orip

#endif // ORIP_WIFI_DECODER_H
