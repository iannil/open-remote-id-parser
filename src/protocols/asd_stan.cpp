#include "orip/asd_stan.h"
#include <regex>
#include <algorithm>
#include <cctype>

namespace orip {
namespace asdstan {

// EU country codes (ISO 3166-1 alpha-3)
static const char* EU_COUNTRY_CODES[] = {
    "AUT", "BEL", "BGR", "HRV", "CYP", "CZE", "DNK", "EST", "FIN", "FRA",
    "DEU", "GRC", "HUN", "IRL", "ITA", "LVA", "LTU", "LUX", "MLT", "NLD",
    "POL", "PRT", "ROU", "SVK", "SVN", "ESP", "SWE",
    // EEA/EFTA countries
    "ISL", "LIE", "NOR", "CHE",
    // UK (post-Brexit, may use similar format)
    "GBR"
};
static const size_t EU_COUNTRY_COUNT = sizeof(EU_COUNTRY_CODES) / sizeof(EU_COUNTRY_CODES[0]);

bool ASD_STAN_Decoder::isRemoteID(const std::vector<uint8_t>& payload) const {
    // ASD-STAN uses the same format as ASTM F3411
    return astm_decoder_.isRemoteID(payload);
}

ASDDecodeResult ASD_STAN_Decoder::decode(const std::vector<uint8_t>& payload, UAVObject& uav) {
    ASDDecodeResult result;

    // Use ASTM decoder for base parsing
    auto astm_result = astm_decoder_.decode(payload, uav);

    result.success = astm_result.success;
    result.type = astm_result.type;
    result.error = astm_result.error;

    if (result.success) {
        // Override protocol type to ASD-STAN
        uav.protocol = ProtocolType::ASD_STAN;

        // Validate EU Operator ID if present
        if (uav.operator_id.valid && !uav.operator_id.id.empty()) {
            if (!validateEUOperatorId(uav.operator_id.id)) {
                // Not strictly an error, but note it
                // Some drones may use ASTM format in EU airspace
            }
        }
    }

    return result;
}

ASDDecodeResult ASD_STAN_Decoder::decodeMessage(const uint8_t* data, size_t len, UAVObject& uav) {
    ASDDecodeResult result;

    auto astm_result = astm_decoder_.decodeMessage(data, len, uav);

    result.success = astm_result.success;
    result.type = astm_result.type;
    result.error = astm_result.error;

    if (result.success) {
        uav.protocol = ProtocolType::ASD_STAN;

        // Parse EU extensions from system message if present
        if (result.type == astm::MessageType::SYSTEM && len >= astm::MESSAGE_SIZE) {
            parseEUExtensions(data, result.eu_info);
        }
    }

    return result;
}

bool ASD_STAN_Decoder::validateEUOperatorId(const std::string& operator_id) const {
    if (operator_id.empty() || operator_id.length() < 5) {
        return false;
    }

    return isValidEUFormat(operator_id);
}

std::string ASD_STAN_Decoder::extractCountryCode(const std::string& operator_id) const {
    // EU format: [3-letter country code]-[rest]
    // Example: "FRA-OP-12345678" -> "FRA"

    if (operator_id.length() < 3) {
        return "";
    }

    // Check for 3-letter country code at start
    std::string code = operator_id.substr(0, 3);

    // Verify it's all uppercase letters
    for (char c : code) {
        if (!std::isupper(static_cast<unsigned char>(c))) {
            return "";
        }
    }

    // Verify it's a valid EU/EEA country code
    for (size_t i = 0; i < EU_COUNTRY_COUNT; i++) {
        if (code == EU_COUNTRY_CODES[i]) {
            return code;
        }
    }

    return "";
}

EUClassification ASD_STAN_Decoder::getClassification(const UAVObject& uav) const {
    // Classification is typically encoded in the operator ID or system message
    // This is a simplified implementation

    if (!uav.system.valid) {
        return EUClassification::UNDEFINED;
    }

    // In practice, classification would be extracted from specific fields
    // For now, return UNDEFINED as we need more spec details
    return EUClassification::UNDEFINED;
}

bool ASD_STAN_Decoder::parseEUExtensions(const uint8_t* data, EUSystemInfo& eu_info) {
    // System message format (bytes after standard fields):
    // ASD-STAN reserves bytes 21-24 for EU-specific extensions
    // Byte 21: Classification type (bits 7-6), Category class (bits 5-3), reserved (bits 2-0)
    // Byte 22: Flags (geo-awareness, remote pilot ID, etc.)

    if (data == nullptr) {
        return false;
    }

    eu_info.valid = true;

    // Parse classification from byte 21 (if available in future spec updates)
    // Currently using reserved bytes - actual implementation depends on final spec

    // Byte 21 bits 7-6: Classification
    uint8_t class_byte = data[21];
    uint8_t classification = (class_byte >> 6) & 0x03;
    eu_info.classification = static_cast<EUClassification>(classification);

    // Byte 21 bits 5-3: Category class
    uint8_t category = (class_byte >> 3) & 0x07;
    eu_info.category_class = static_cast<EUCategoryClass>(category);

    // Byte 22: Flags
    uint8_t flags = data[22];
    eu_info.geo_awareness = (flags & 0x01) != 0;
    eu_info.remote_pilot_id = (flags & 0x02) != 0;

    return true;
}

bool ASD_STAN_Decoder::isValidEUFormat(const std::string& id) const {
    // EU Operator ID format variations:
    // 1. [3-letter country]-[CAA code]-[number] e.g., "FRA-OP-12345678"
    // 2. [3-letter country][alphanumeric registration] e.g., "FRA1234567890"
    // 3. Direct registration number format

    if (id.length() < 3) {
        return false;
    }

    // Extract and validate country code
    std::string country = extractCountryCode(id);
    if (country.empty()) {
        // Not a standard EU format, but might still be valid
        // (some countries use different formats)
        return false;
    }

    // Check for separator format (FRA-XX-NNNN)
    if (id.length() > 3 && id[3] == '-') {
        // Format with separators
        // At minimum: XXX-X-X (7 chars)
        if (id.length() < 7) {
            return false;
        }

        // Find second separator
        size_t second_sep = id.find('-', 4);
        if (second_sep == std::string::npos) {
            return false;
        }

        // Verify registration number exists
        if (second_sep + 1 >= id.length()) {
            return false;
        }

        return true;
    }

    // Compact format (FRAxxxxxxxxxx)
    // Verify remaining characters are alphanumeric
    for (size_t i = 3; i < id.length(); i++) {
        char c = id[i];
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    return true;
}

} // namespace asdstan
} // namespace orip
