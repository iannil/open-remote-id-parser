#ifndef ORIP_ASD_STAN_H
#define ORIP_ASD_STAN_H

#include "orip/types.h"
#include "orip/astm_f3411.h"
#include <cstdint>
#include <vector>

namespace orip {
namespace asdstan {

// ASD-STAN EN 4709-002 specific constants
// Note: ASD-STAN is largely compatible with ASTM F3411 but has EU-specific extensions

// EU Operator ID types (extends base operator ID types)
enum class EUOperatorIdType : uint8_t {
    OPERATOR_ID = 0,           // Operator registration ID
    CAA_DESIGNATED_ID = 1      // CAA designated ID
};

// EU Classification types
enum class EUClassification : uint8_t {
    UNDEFINED = 0,
    OPEN = 1,          // Open category
    SPECIFIC = 2,      // Specific category
    CERTIFIED = 3      // Certified category
};

// EU Category class (for Open category)
enum class EUCategoryClass : uint8_t {
    UNDEFINED = 0,
    C0 = 1,
    C1 = 2,
    C2 = 3,
    C3 = 4,
    C4 = 5,
    C5 = 6,
    C6 = 7
};

// Extended system info for EU
struct EUSystemInfo {
    bool valid = false;
    EUClassification classification = EUClassification::UNDEFINED;
    EUCategoryClass category_class = EUCategoryClass::UNDEFINED;
    bool geo_awareness = false;        // Geo-awareness capability
    bool remote_pilot_id = false;      // Remote pilot ID transmitted
};

// Decode result with EU-specific fields
struct ASDDecodeResult {
    bool success = false;
    astm::MessageType type = astm::MessageType::BASIC_ID;
    std::string error;
    EUSystemInfo eu_info;
};

/**
 * ASD-STAN EN 4709-002 Decoder
 *
 * The European standard for drone remote identification.
 * Based on ASTM F3411 with EU-specific extensions.
 *
 * Supports:
 * - All ASTM F3411 message types
 * - EU Operator ID format
 * - EU classification information
 * - EU-specific system message extensions
 */
class ASD_STAN_Decoder {
public:
    ASD_STAN_Decoder() = default;

    /**
     * Check if payload looks like ASD-STAN Remote ID
     * Uses same detection as ASTM F3411 (compatible format)
     */
    bool isRemoteID(const std::vector<uint8_t>& payload) const;

    /**
     * Decode a complete advertisement/beacon payload
     * @param payload Raw BLE/WiFi data
     * @param uav Output UAV object
     * @return Decode result
     */
    ASDDecodeResult decode(const std::vector<uint8_t>& payload, UAVObject& uav);

    /**
     * Decode a single message (25 bytes)
     */
    ASDDecodeResult decodeMessage(const uint8_t* data, size_t len, UAVObject& uav);

    /**
     * Validate EU Operator ID format
     * Format: [Country Code][CAA Code]-[Registration Number]
     * Example: FRA-OP-12345678
     */
    bool validateEUOperatorId(const std::string& operator_id) const;

    /**
     * Extract country code from EU Operator ID
     */
    std::string extractCountryCode(const std::string& operator_id) const;

    /**
     * Get EU classification from system message
     */
    EUClassification getClassification(const UAVObject& uav) const;

private:
    astm::ASTM_F3411_Decoder astm_decoder_;

    // Parse EU-specific extensions from system message
    bool parseEUExtensions(const uint8_t* data, EUSystemInfo& eu_info);

    // Validate operator ID format per EU regulations
    bool isValidEUFormat(const std::string& id) const;
};

} // namespace asdstan
} // namespace orip

#endif // ORIP_ASD_STAN_H
