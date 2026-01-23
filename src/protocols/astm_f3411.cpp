#include "orip/astm_f3411.h"
#include "orip/bit_reader.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace orip {
namespace astm {

// Constants for decoding
constexpr double LAT_LON_MULTIPLIER = 1e-7;
constexpr float ALTITUDE_OFFSET = -1000.0f;
constexpr float ALTITUDE_MULTIPLIER = 0.5f;
constexpr float SPEED_MULTIPLIER = 0.25f;
constexpr float SPEED_MULTIPLIER_HIGH = 0.75f;
constexpr float SPEED_OFFSET_HIGH = 255 * 0.25f;
constexpr float VERTICAL_SPEED_MULTIPLIER = 0.5f;
constexpr float DIRECTION_MULTIPLIER = 1.0f;  // degrees

// BT5 Extended Advertising constants
constexpr uint8_t EXT_ADV_LEGACY_PDU = 0x10;     // Legacy PDU flag
constexpr size_t EXT_ADV_MIN_HEADER = 2;         // Minimum extended header

bool ASTM_F3411_Decoder::isRemoteID(const std::vector<uint8_t>& payload) const {
    if (payload.size() < 5) {
        return false;
    }

    // Check for extended advertising first
    if (isExtendedAdvertising(payload)) {
        return true;
    }

    // Look for ODID AD Type and Service UUID in legacy BLE advertisement
    for (size_t i = 0; i + 4 < payload.size(); ) {
        uint8_t len = payload[i];
        if (len == 0 || i + len >= payload.size()) {
            break;
        }

        if (len >= 4 && payload[i + 1] == ODID_AD_TYPE) {
            uint16_t uuid = static_cast<uint16_t>(payload[i + 2]) |
                           (static_cast<uint16_t>(payload[i + 3]) << 8);
            if (uuid == ODID_SERVICE_UUID) {
                return true;
            }
        }

        i += len + 1;
    }

    return false;
}

bool ASTM_F3411_Decoder::isExtendedAdvertising(const std::vector<uint8_t>& payload) const {
    if (payload.size() < EXT_ADV_MIN_HEADER + 5) {
        return false;
    }

    // Extended advertising can have different structures
    // Look for ODID UUID anywhere in the extended payload
    for (size_t i = 0; i + 4 < payload.size(); i++) {
        // Check for Service Data AD Type with ODID UUID
        if (payload[i] == ODID_AD_TYPE) {
            if (i + 3 < payload.size()) {
                uint16_t uuid = static_cast<uint16_t>(payload[i + 1]) |
                               (static_cast<uint16_t>(payload[i + 2]) << 8);
                if (uuid == ODID_SERVICE_UUID) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool ASTM_F3411_Decoder::findODIDData(const std::vector<uint8_t>& payload,
                                       const uint8_t*& data, size_t& len) const {
    // Search for ODID Service UUID in the payload
    for (size_t i = 0; i + 4 < payload.size(); ) {
        uint8_t ad_len = payload[i];
        if (ad_len == 0 || i + ad_len >= payload.size()) {
            break;
        }

        if (ad_len >= 4 && payload[i + 1] == ODID_AD_TYPE) {
            uint16_t uuid = static_cast<uint16_t>(payload[i + 2]) |
                           (static_cast<uint16_t>(payload[i + 3]) << 8);

            if (uuid == ODID_SERVICE_UUID) {
                // Found ODID data - check for underflow before subtraction
                if (ad_len < 3) {
                    return false;  // Insufficient data for ODID
                }
                data = payload.data() + i + 4;
                len = ad_len - 3;  // Subtract AD type and UUID

                // Skip message counter if present
                if (len > 0) {
                    data++;
                    len--;
                }
                return true;
            }
        }

        i += ad_len + 1;
    }

    return false;
}

DecodeResult ASTM_F3411_Decoder::decode(const std::vector<uint8_t>& payload, UAVObject& uav) {
    // Try extended advertising first (BT5)
    if (isExtendedAdvertising(payload)) {
        auto result = decodeExtended(payload, uav);
        if (result.success) {
            uav.transport = TransportType::BT_EXTENDED;
            return result;
        }
    }

    // Fall back to legacy advertising
    return decodeLegacy(payload, uav);
}

DecodeResult ASTM_F3411_Decoder::decodeExtended(const std::vector<uint8_t>& payload, UAVObject& uav) {
    DecodeResult result;
    result.success = false;

    const uint8_t* odid_data = nullptr;
    size_t odid_len = 0;

    if (!findODIDData(payload, odid_data, odid_len)) {
        result.error = "No ODID data found in extended advertisement";
        return result;
    }

    if (odid_len < MESSAGE_SIZE) {
        result.error = "ODID data too short";
        return result;
    }

    result = decodeMessage(odid_data, odid_len, uav);
    if (result.success) {
        uav.protocol = ProtocolType::ASTM_F3411;
        uav.transport = TransportType::BT_EXTENDED;
    }

    return result;
}

DecodeResult ASTM_F3411_Decoder::decodeLegacy(const std::vector<uint8_t>& payload, UAVObject& uav) {
    DecodeResult result;
    result.success = false;

    if (payload.size() < 5) {
        result.error = "Payload too short";
        return result;
    }

    // Parse BLE advertisement structure to find ODID data
    for (size_t i = 0; i + 4 < payload.size(); ) {
        uint8_t len = payload[i];
        if (len == 0 || i + len >= payload.size()) {
            break;
        }

        if (len >= 4 && payload[i + 1] == ODID_AD_TYPE) {
            uint16_t uuid = static_cast<uint16_t>(payload[i + 2]) |
                           (static_cast<uint16_t>(payload[i + 3]) << 8);

            if (uuid == ODID_SERVICE_UUID) {
                // Found ODID data, decode the message(s)
                // Check for underflow before subtraction
                if (len < 3) {
                    i += len + 1;
                    continue;  // Insufficient data for ODID
                }
                const uint8_t* msg_data = payload.data() + i + 4;
                size_t msg_len = len - 3;  // Subtract AD type and UUID

                // Skip message counter byte if present
                if (msg_len > 0) {
                    msg_data++;
                    msg_len--;
                }

                if (msg_len >= MESSAGE_SIZE) {
                    result = decodeMessage(msg_data, msg_len, uav);
                    if (result.success) {
                        uav.protocol = ProtocolType::ASTM_F3411;
                        uav.transport = TransportType::BT_LEGACY;
                        return result;
                    }
                }
            }
        }

        i += len + 1;
    }

    result.error = "No valid ODID message found";
    return result;
}

DecodeResult ASTM_F3411_Decoder::decodeMessage(const uint8_t* data, size_t len, UAVObject& uav) {
    DecodeResult result;
    result.success = false;

    if (len < MESSAGE_SIZE) {
        result.error = "Message too short";
        return result;
    }

    // First byte contains message type (upper 4 bits) and protocol version (lower 4 bits)
    uint8_t header = data[0];
    uint8_t msg_type = (header >> 4) & 0x0F;
    // uint8_t proto_version = header & 0x0F;  // Currently unused

    result.type = static_cast<MessageType>(msg_type);

    bool decode_ok = false;
    switch (result.type) {
        case MessageType::BASIC_ID:
            decode_ok = decodeBasicID(data, uav);
            break;
        case MessageType::LOCATION:
            decode_ok = decodeLocation(data, uav);
            break;
        case MessageType::AUTH:
            decode_ok = decodeAuth(data, uav);
            break;
        case MessageType::SELF_ID:
            decode_ok = decodeSelfID(data, uav);
            break;
        case MessageType::SYSTEM:
            decode_ok = decodeSystem(data, uav);
            break;
        case MessageType::OPERATOR_ID:
            decode_ok = decodeOperatorID(data, uav);
            break;
        case MessageType::MESSAGE_PACK:
            decode_ok = decodeMessagePack(data, len, uav);
            break;
        default:
            result.error = "Unknown message type";
            return result;
    }

    if (decode_ok) {
        result.success = true;
        uav.message_count++;
    } else {
        result.error = "Failed to decode message";
    }

    return result;
}

bool ASTM_F3411_Decoder::decodeBasicID(const uint8_t* data, UAVObject& uav) {
    uint8_t type_byte = data[1];
    uav.id_type = static_cast<UAVIdType>((type_byte >> 4) & 0x0F);
    uav.uav_type = static_cast<UAVType>(type_byte & 0x0F);

    char id_buf[BASIC_ID_LENGTH + 1] = {0};
    std::memcpy(id_buf, data + 2, BASIC_ID_LENGTH);
    uav.id = std::string(id_buf);

    while (!uav.id.empty() && (uav.id.back() == ' ' || uav.id.back() == '\0')) {
        uav.id.pop_back();
    }

    return true;
}

bool ASTM_F3411_Decoder::decodeLocation(const uint8_t* data, UAVObject& uav) {
    LocationVector& loc = uav.location;
    loc.valid = true;

    uint8_t status_byte = data[1];
    loc.status = static_cast<UAVStatus>((status_byte >> 4) & 0x0F);
    loc.height_ref = static_cast<HeightReference>((status_byte >> 2) & 0x01);
    bool speed_mult = (status_byte & 0x01) != 0;

    loc.direction = decodeDirection(data[2]);
    loc.speed_horizontal = decodeSpeed(data[3], speed_mult);
    loc.speed_vertical = decodeVerticalSpeed(static_cast<int8_t>(data[4]));

    int32_t lat_encoded = utils::readLE32Signed(data + 5);
    int32_t lon_encoded = utils::readLE32Signed(data + 9);
    loc.latitude = decodeLatitude(lat_encoded);
    loc.longitude = decodeLongitude(lon_encoded);

    uint16_t alt_baro = utils::readLE16(data + 13);
    uint16_t alt_geo = utils::readLE16(data + 15);
    uint16_t height = utils::readLE16(data + 17);
    loc.altitude_baro = decodeAltitude(alt_baro);
    loc.altitude_geo = decodeAltitude(alt_geo);
    loc.height = decodeAltitude(height);

    uint8_t accuracy1 = data[19];
    loc.h_accuracy = static_cast<HorizontalAccuracy>((accuracy1 >> 4) & 0x0F);
    loc.v_accuracy = static_cast<VerticalAccuracy>(accuracy1 & 0x0F);

    uint8_t accuracy2 = data[20];
    loc.speed_accuracy = static_cast<SpeedAccuracy>(accuracy2 & 0x0F);

    loc.timestamp_offset = utils::readLE16(data + 21);

    return true;
}

bool ASTM_F3411_Decoder::decodeAuth(const uint8_t* data, UAVObject& uav) {
    uav.auth_data.assign(data + 1, data + MESSAGE_SIZE);
    return true;
}

bool ASTM_F3411_Decoder::decodeSelfID(const uint8_t* data, UAVObject& uav) {
    uav.self_id.valid = true;
    uav.self_id.description_type = data[1];

    char desc_buf[SELF_ID_LENGTH + 1] = {0};
    std::memcpy(desc_buf, data + 2, SELF_ID_LENGTH);
    uav.self_id.description = std::string(desc_buf);

    while (!uav.self_id.description.empty() &&
           (uav.self_id.description.back() == ' ' || uav.self_id.description.back() == '\0')) {
        uav.self_id.description.pop_back();
    }

    return true;
}

bool ASTM_F3411_Decoder::decodeSystem(const uint8_t* data, UAVObject& uav) {
    SystemInfo& sys = uav.system;
    sys.valid = true;

    uint8_t loc_type = (data[1] >> 4) & 0x03;
    sys.location_type = static_cast<OperatorLocationType>(loc_type);

    int32_t op_lat = utils::readLE32Signed(data + 2);
    int32_t op_lon = utils::readLE32Signed(data + 6);
    sys.operator_latitude = decodeLatitude(op_lat);
    sys.operator_longitude = decodeLongitude(op_lon);

    sys.area_count = utils::readLE16(data + 10);
    sys.area_radius = data[12] * 10;

    uint16_t ceiling = utils::readLE16(data + 13);
    uint16_t floor = utils::readLE16(data + 15);
    sys.area_ceiling = decodeAltitude(ceiling);
    sys.area_floor = decodeAltitude(floor);

    sys.timestamp = utils::readLE32(data + 17);

    return true;
}

bool ASTM_F3411_Decoder::decodeOperatorID(const uint8_t* data, UAVObject& uav) {
    uav.operator_id.valid = true;
    uav.operator_id.id_type = data[1];

    char id_buf[OPERATOR_ID_LENGTH + 1] = {0};
    std::memcpy(id_buf, data + 2, OPERATOR_ID_LENGTH);
    uav.operator_id.id = std::string(id_buf);

    while (!uav.operator_id.id.empty() &&
           (uav.operator_id.id.back() == ' ' || uav.operator_id.id.back() == '\0')) {
        uav.operator_id.id.pop_back();
    }

    return true;
}

bool ASTM_F3411_Decoder::decodeMessagePack(const uint8_t* data, size_t len, UAVObject& uav) {
    if (len < 2) {
        return false;
    }

    uint8_t pack_info = data[1];
    uint8_t msg_size = ((pack_info >> 4) & 0x0F) + 1;
    uint8_t msg_count = pack_info & 0x0F;

    if (msg_size != MESSAGE_SIZE) {
        return false;
    }

    size_t offset = 2;
    for (uint8_t i = 0; i < msg_count && offset + MESSAGE_SIZE <= len; i++) {
        decodeMessage(data + offset, MESSAGE_SIZE, uav);
        offset += MESSAGE_SIZE;
    }

    return true;
}

double ASTM_F3411_Decoder::decodeLatitude(int32_t encoded) const {
    return encoded * LAT_LON_MULTIPLIER;
}

double ASTM_F3411_Decoder::decodeLongitude(int32_t encoded) const {
    return encoded * LAT_LON_MULTIPLIER;
}

float ASTM_F3411_Decoder::decodeAltitude(uint16_t encoded) const {
    if (encoded == 0) {
        return 0.0f;
    }
    return (encoded * ALTITUDE_MULTIPLIER) + ALTITUDE_OFFSET;
}

float ASTM_F3411_Decoder::decodeSpeed(uint8_t encoded, bool multiplier) const {
    if (encoded == 255) {
        return std::nanf("");
    }
    if (multiplier) {
        return (encoded * SPEED_MULTIPLIER_HIGH) + SPEED_OFFSET_HIGH;
    }
    return encoded * SPEED_MULTIPLIER;
}

float ASTM_F3411_Decoder::decodeVerticalSpeed(int8_t encoded) const {
    if (encoded == 63) {
        return std::nanf("");
    }
    return encoded * VERTICAL_SPEED_MULTIPLIER;
}

float ASTM_F3411_Decoder::decodeDirection(uint8_t encoded) const {
    if (encoded > 360) {
        return std::nanf("");
    }
    return encoded * DIRECTION_MULTIPLIER;
}

} // namespace astm
} // namespace orip
