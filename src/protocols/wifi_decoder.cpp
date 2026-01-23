#include "orip/wifi_decoder.h"
#include "orip/astm_f3411.h"
#include "orip/bit_reader.h"
#include <cstring>

namespace orip {
namespace wifi {

// 802.11 frame control field bits
constexpr uint16_t FC_TYPE_MASK = 0x000C;
constexpr uint16_t FC_SUBTYPE_MASK = 0x00F0;
constexpr uint16_t FC_TYPE_MGMT = 0x0000;
constexpr uint16_t FC_SUBTYPE_BEACON = 0x0080;
constexpr uint16_t FC_SUBTYPE_PROBE_RESP = 0x0050;
constexpr uint16_t FC_SUBTYPE_ACTION = 0x00D0;

// Information Element IDs
constexpr uint8_t IE_VENDOR_SPECIFIC = 221;

// Minimum frame sizes
constexpr size_t MIN_MGMT_HEADER = 24;  // 802.11 management frame header
constexpr size_t MIN_BEACON_BODY = 12;  // Timestamp(8) + Interval(2) + Capability(2)

bool WiFiDecoder::isRemoteID(const std::vector<uint8_t>& payload) const {
    if (payload.size() < 10) {
        return false;
    }

    // Check for ASTM OUI in payload
    for (size_t i = 0; i + 6 < payload.size(); i++) {
        if (payload[i] == WIFI_OUI_FA[0] &&
            payload[i + 1] == WIFI_OUI_FA[1] &&
            payload[i + 2] == WIFI_OUI_FA[2] &&
            payload[i + 3] == WIFI_VENDOR_TYPE) {
            return true;
        }
    }

    // Check for NAN Service ID
    for (size_t i = 0; i + 6 < payload.size(); i++) {
        if (std::memcmp(&payload[i], NAN_SERVICE_ID, 6) == 0) {
            return true;
        }
    }

    return false;
}

WiFiDecodeResult WiFiDecoder::decodeBeacon(const std::vector<uint8_t>& payload, UAVObject& uav) {
    WiFiDecodeResult result;

    if (payload.size() < MIN_MGMT_HEADER + MIN_BEACON_BODY) {
        result.error = "Frame too short for beacon";
        return result;
    }

    // Parse 802.11 header
    size_t offset = 0;
    if (!parseFrameHeader(payload.data(), payload.size(), offset)) {
        result.error = "Invalid 802.11 header";
        return result;
    }

    // Skip fixed beacon parameters (timestamp, interval, capability)
    offset += MIN_BEACON_BODY;

    // Search for Vendor Specific IE with ASTM OUI
    const uint8_t* ie_data = nullptr;
    size_t ie_len = 0;

    if (!findVendorIE(payload.data() + offset, payload.size() - offset,
                      WIFI_OUI_FA, 3, ie_data, ie_len)) {
        result.error = "No Remote ID vendor IE found";
        return result;
    }

    // Decode the ASTM payload (skip vendor type byte)
    // Check for underflow before subtraction
    if (ie_len > 1) {
        if (decodeASTMPayload(ie_data + 1, ie_len - 1, uav)) {
            result.success = true;
            uav.transport = TransportType::WIFI_BEACON;
            uav.protocol = ProtocolType::ASTM_F3411;
        } else {
            result.error = "Failed to decode ASTM payload";
        }
    } else {
        result.error = "Vendor IE data too short";
    }

    return result;
}

WiFiDecodeResult WiFiDecoder::decodeNAN(const std::vector<uint8_t>& payload, UAVObject& uav) {
    WiFiDecodeResult result;

    if (payload.size() < 10) {
        result.error = "NAN frame too short";
        return result;
    }

    // NAN frames can have different structures
    // Look for the Service ID followed by Remote ID data

    const uint8_t* data = payload.data();
    size_t len = payload.size();

    // Try to find NAN Service ID
    for (size_t i = 0; i + 6 + astm::MESSAGE_SIZE <= len; i++) {
        if (std::memcmp(data + i, NAN_SERVICE_ID, 6) == 0) {
            // Found service ID, decode following data
            if (decodeASTMPayload(data + i + 6, len - i - 6, uav)) {
                result.success = true;
                uav.transport = TransportType::WIFI_NAN;
                uav.protocol = ProtocolType::ASTM_F3411;
                return result;
            }
        }
    }

    // Try direct ASTM OUI lookup
    for (size_t i = 0; i + 4 + astm::MESSAGE_SIZE <= len; i++) {
        if (data[i] == WIFI_OUI_FA[0] &&
            data[i + 1] == WIFI_OUI_FA[1] &&
            data[i + 2] == WIFI_OUI_FA[2] &&
            data[i + 3] == WIFI_VENDOR_TYPE) {
            if (decodeASTMPayload(data + i + 4, len - i - 4, uav)) {
                result.success = true;
                uav.transport = TransportType::WIFI_NAN;
                uav.protocol = ProtocolType::ASTM_F3411;
                return result;
            }
        }
    }

    result.error = "No valid NAN Remote ID data found";
    return result;
}

WiFiDecodeResult WiFiDecoder::decodeVendorIE(const std::vector<uint8_t>& payload, UAVObject& uav) {
    WiFiDecodeResult result;

    if (payload.size() < 4) {
        result.error = "Vendor IE too short";
        return result;
    }

    // Check OUI
    if (payload[0] != WIFI_OUI_FA[0] ||
        payload[1] != WIFI_OUI_FA[1] ||
        payload[2] != WIFI_OUI_FA[2]) {
        result.error = "Invalid OUI";
        return result;
    }

    // Check vendor type
    if (payload[3] != WIFI_VENDOR_TYPE) {
        result.error = "Invalid vendor type";
        return result;
    }

    // Decode ASTM payload
    if (decodeASTMPayload(payload.data() + 4, payload.size() - 4, uav)) {
        result.success = true;
        uav.transport = TransportType::WIFI_BEACON;
        uav.protocol = ProtocolType::ASTM_F3411;
    } else {
        result.error = "Failed to decode ASTM payload";
    }

    return result;
}

bool WiFiDecoder::parseFrameHeader(const uint8_t* data, size_t len, size_t& payload_offset) {
    if (len < MIN_MGMT_HEADER) {
        return false;
    }

    // Frame control (2 bytes, little endian)
    uint16_t fc = utils::readLE16(data);

    // Check if this is a management frame
    if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) {
        return false;
    }

    // Check subtype (beacon, probe response, or action)
    uint16_t subtype = fc & FC_SUBTYPE_MASK;
    if (subtype != FC_SUBTYPE_BEACON &&
        subtype != FC_SUBTYPE_PROBE_RESP &&
        subtype != FC_SUBTYPE_ACTION) {
        return false;
    }

    payload_offset = MIN_MGMT_HEADER;
    return true;
}

bool WiFiDecoder::findVendorIE(const uint8_t* data, size_t len,
                                const uint8_t* oui, size_t oui_len,
                                const uint8_t*& ie_data, size_t& ie_len) {
    size_t offset = 0;

    while (offset + 2 <= len) {
        uint8_t ie_id = data[offset];
        uint8_t ie_length = data[offset + 1];

        if (offset + 2 + ie_length > len) {
            break;
        }

        if (ie_id == IE_VENDOR_SPECIFIC && ie_length >= oui_len) {
            // Check OUI
            if (std::memcmp(data + offset + 2, oui, oui_len) == 0) {
                ie_data = data + offset + 2 + oui_len;
                ie_len = ie_length - oui_len;
                return true;
            }
        }

        offset += 2 + ie_length;
    }

    return false;
}

bool WiFiDecoder::decodeASTMPayload(const uint8_t* data, size_t len, UAVObject& uav) {
    if (len < astm::MESSAGE_SIZE) {
        return false;
    }

    // Use ASTM decoder for the actual message parsing
    astm::ASTM_F3411_Decoder decoder;
    auto result = decoder.decodeMessage(data, len, uav);

    return result.success;
}

} // namespace wifi
} // namespace orip
