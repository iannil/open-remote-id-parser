#ifndef ORIP_TYPES_H
#define ORIP_TYPES_H

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace orip {

// Protocol types supported by the parser
enum class ProtocolType : uint8_t {
    UNKNOWN = 0,
    ASTM_F3411 = 1,  // USA/International standard
    ASD_STAN = 2,    // EU standard
    CN_RID = 3       // China standard (reserved)
};

// Transport layer type
enum class TransportType : uint8_t {
    UNKNOWN = 0,
    BT_LEGACY = 1,      // Bluetooth 4.x Legacy Advertising
    BT_EXTENDED = 2,    // Bluetooth 5.x Extended Advertising
    WIFI_BEACON = 3,    // WiFi Beacon
    WIFI_NAN = 4        // WiFi Neighbor Awareness Networking
};

// Raw frame input from radio layer
struct RawFrame {
    std::vector<uint8_t> payload;   // Raw advertisement/beacon data
    int8_t rssi;                    // Received Signal Strength Indicator (dBm)
    TransportType transport;        // Source transport type
    std::chrono::steady_clock::time_point timestamp;

    RawFrame() : rssi(0), transport(TransportType::UNKNOWN),
                 timestamp(std::chrono::steady_clock::now()) {}
};

// UAV identification type (ASTM F3411)
enum class UAVIdType : uint8_t {
    NONE = 0,
    SERIAL_NUMBER = 1,      // Manufacturer serial number
    CAA_REGISTRATION = 2,   // Civil Aviation Authority registration
    UTM_ASSIGNED = 3,       // UTM system assigned ID
    SPECIFIC_SESSION = 4    // Specific session ID
};

// UAV type classification
enum class UAVType : uint8_t {
    NONE = 0,
    AEROPLANE = 1,
    HELICOPTER_OR_MULTIROTOR = 2,
    GYROPLANE = 3,
    HYBRID_LIFT = 4,        // Fixed wing + VTOL
    ORNITHOPTER = 5,
    GLIDER = 6,
    KITE = 7,
    FREE_BALLOON = 8,
    CAPTIVE_BALLOON = 9,
    AIRSHIP = 10,
    FREE_FALL_PARACHUTE = 11,
    ROCKET = 12,
    TETHERED_POWERED = 13,
    GROUND_OBSTACLE = 14,
    OTHER = 15
};

// Operator location type
enum class OperatorLocationType : uint8_t {
    TAKEOFF = 0,            // Take-off location
    LIVE_GNSS = 1,          // Dynamic/Live GNSS
    FIXED = 2               // Fixed location
};

// Height reference type
enum class HeightReference : uint8_t {
    TAKEOFF = 0,            // Above takeoff
    GROUND = 1              // Above ground level (AGL)
};

// Horizontal accuracy (encoded as per ASTM F3411)
enum class HorizontalAccuracy : uint8_t {
    UNKNOWN = 0,
    LESS_THAN_10NM = 1,
    LESS_THAN_4NM = 2,
    LESS_THAN_2NM = 3,
    LESS_THAN_1NM = 4,
    LESS_THAN_0_5NM = 5,
    LESS_THAN_0_3NM = 6,
    LESS_THAN_0_1NM = 7,
    LESS_THAN_0_05NM = 8,
    LESS_THAN_30M = 9,
    LESS_THAN_10M = 10,
    LESS_THAN_3M = 11,
    LESS_THAN_1M = 12
};

// Vertical accuracy (encoded as per ASTM F3411)
enum class VerticalAccuracy : uint8_t {
    UNKNOWN = 0,
    LESS_THAN_150M = 1,
    LESS_THAN_45M = 2,
    LESS_THAN_25M = 3,
    LESS_THAN_10M = 4,
    LESS_THAN_3M = 5,
    LESS_THAN_1M = 6
};

// Speed accuracy
enum class SpeedAccuracy : uint8_t {
    UNKNOWN = 0,
    LESS_THAN_10MPS = 1,
    LESS_THAN_3MPS = 2,
    LESS_THAN_1MPS = 3,
    LESS_THAN_0_3MPS = 4
};

// UAV status
enum class UAVStatus : uint8_t {
    UNDECLARED = 0,
    GROUND = 1,
    AIRBORNE = 2,
    EMERGENCY = 3,
    REMOTE_ID_FAILURE = 4
};

// Location/Vector data
struct LocationVector {
    bool valid = false;
    double latitude = 0.0;          // Degrees (-90 to 90)
    double longitude = 0.0;         // Degrees (-180 to 180)
    float altitude_baro = 0.0f;     // Barometric altitude (meters)
    float altitude_geo = 0.0f;      // Geodetic altitude (meters)
    float height = 0.0f;            // Height above reference (meters)
    HeightReference height_ref = HeightReference::TAKEOFF;
    float speed_horizontal = 0.0f;  // m/s
    float speed_vertical = 0.0f;    // m/s (positive = up)
    float direction = 0.0f;         // Track direction (degrees, 0-360)
    HorizontalAccuracy h_accuracy = HorizontalAccuracy::UNKNOWN;
    VerticalAccuracy v_accuracy = VerticalAccuracy::UNKNOWN;
    SpeedAccuracy speed_accuracy = SpeedAccuracy::UNKNOWN;
    UAVStatus status = UAVStatus::UNDECLARED;
    uint16_t timestamp_offset = 0;  // Offset from full hour (0.1s units)
};

// System message data (operator/pilot info)
struct SystemInfo {
    bool valid = false;
    OperatorLocationType location_type = OperatorLocationType::TAKEOFF;
    double operator_latitude = 0.0;
    double operator_longitude = 0.0;
    float area_ceiling = 0.0f;      // Area operation ceiling (meters)
    float area_floor = 0.0f;        // Area operation floor (meters)
    uint16_t area_count = 1;        // Number of aircraft in area
    uint16_t area_radius = 0;       // Area radius (meters)
    uint32_t timestamp = 0;         // Unix timestamp
};

// Self-ID message
struct SelfId {
    bool valid = false;
    uint8_t description_type = 0;
    std::string description;        // Free-form text (max 23 chars)
};

// Operator ID message
struct OperatorId {
    bool valid = false;
    uint8_t id_type = 0;
    std::string id;                 // Operator ID string
};

// Complete UAV object containing all parsed data
struct UAVObject {
    // Identification
    std::string id;                 // Primary ID (serial number or registration)
    UAVIdType id_type = UAVIdType::NONE;
    UAVType uav_type = UAVType::NONE;

    // Protocol info
    ProtocolType protocol = ProtocolType::UNKNOWN;
    TransportType transport = TransportType::UNKNOWN;

    // Signal info
    int8_t rssi = 0;
    std::chrono::steady_clock::time_point last_seen;

    // Message data
    LocationVector location;
    SystemInfo system;
    SelfId self_id;
    OperatorId operator_id;

    // Authentication (raw bytes, interpretation depends on auth type)
    std::vector<uint8_t> auth_data;

    // Tracking
    uint32_t message_count = 0;     // Number of messages received

    UAVObject() : last_seen(std::chrono::steady_clock::now()) {}
};

// Parse result returned by parser
struct ParseResult {
    bool success = false;           // Whether parsing succeeded
    bool is_remote_id = false;      // Whether this is a Remote ID frame
    ProtocolType protocol = ProtocolType::UNKNOWN;
    std::string error;              // Error message if failed

    // Parsed UAV data (valid only if success && is_remote_id)
    UAVObject uav;
};

} // namespace orip

#endif // ORIP_TYPES_H
