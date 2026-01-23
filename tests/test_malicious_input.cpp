/**
 * Malicious Input Tests (P2-SEC-006)
 *
 * Tests the parser's resilience against various malicious or malformed inputs:
 * - All zero payloads
 * - All 0xFF payloads
 * - Random byte sequences
 * - Format string attack patterns
 * - Oversized payloads
 * - Truncated messages
 * - Boundary value testing
 */

#include <gtest/gtest.h>
#include "orip/parser.h"
#include "orip/astm_f3411.h"
#include "orip/asd_stan.h"
#include "orip/wifi_decoder.h"
#include <vector>
#include <random>
#include <string>
#include <cstring>

using namespace orip;

class MaliciousInputTest : public ::testing::Test {
protected:
    RemoteIDParser parser;

    void SetUp() override {
        parser.init();
    }

    // Helper to create a vector of repeated bytes
    std::vector<uint8_t> createRepeatedPayload(uint8_t byte, size_t size) {
        return std::vector<uint8_t>(size, byte);
    }

    // Helper to create random payload
    std::vector<uint8_t> createRandomPayload(size_t size, uint32_t seed = 42) {
        std::vector<uint8_t> payload(size);
        std::mt19937 gen(seed);
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        for (auto& byte : payload) {
            byte = dist(gen);
        }
        return payload;
    }
};

// ============================================
// All Zero Payloads
// ============================================

TEST_F(MaliciousInputTest, AllZeroPayload_Empty) {
    std::vector<uint8_t> payload;
    auto result = parser.parse(payload, -60);
    EXPECT_FALSE(result.success());
}

TEST_F(MaliciousInputTest, AllZeroPayload_SingleByte) {
    auto payload = createRepeatedPayload(0x00, 1);
    auto result = parser.parse(payload, -60);
    EXPECT_FALSE(result.success());
}

TEST_F(MaliciousInputTest, AllZeroPayload_MinLength) {
    auto payload = createRepeatedPayload(0x00, 25);  // ASTM message size
    auto result = parser.parse(payload, -60);
    // Should not crash, may or may not parse successfully
    SUCCEED();
}

TEST_F(MaliciousInputTest, AllZeroPayload_MaxLength) {
    auto payload = createRepeatedPayload(0x00, 1000);
    auto result = parser.parse(payload, -60);
    // Should handle gracefully without crash
    SUCCEED();
}

// ============================================
// All 0xFF Payloads
// ============================================

TEST_F(MaliciousInputTest, AllOnesPayload_MinLength) {
    auto payload = createRepeatedPayload(0xFF, 25);
    auto result = parser.parse(payload, -60);
    // Should handle gracefully
    SUCCEED();
}

TEST_F(MaliciousInputTest, AllOnesPayload_MaxLength) {
    auto payload = createRepeatedPayload(0xFF, 1000);
    auto result = parser.parse(payload, -60);
    // Should handle gracefully
    SUCCEED();
}

// ============================================
// Random Byte Sequences
// ============================================

TEST_F(MaliciousInputTest, RandomPayload_Small) {
    for (int i = 0; i < 100; i++) {
        auto payload = createRandomPayload(25, i);
        auto result = parser.parse(payload, -60);
        // Should not crash
    }
    SUCCEED();
}

TEST_F(MaliciousInputTest, RandomPayload_Medium) {
    for (int i = 0; i < 50; i++) {
        auto payload = createRandomPayload(100, i * 100);
        auto result = parser.parse(payload, -60);
        // Should not crash
    }
    SUCCEED();
}

TEST_F(MaliciousInputTest, RandomPayload_Large) {
    for (int i = 0; i < 10; i++) {
        auto payload = createRandomPayload(1000, i * 1000);
        auto result = parser.parse(payload, -60);
        // Should not crash
    }
    SUCCEED();
}

// ============================================
// Format String Attack Patterns
// ============================================

TEST_F(MaliciousInputTest, FormatStringPattern_PercentN) {
    // %n format string attack pattern
    std::string pattern = "%n%n%n%n%n%n%n%n";
    std::vector<uint8_t> payload(pattern.begin(), pattern.end());

    // Pad to valid message length
    payload.resize(50, 0x00);

    auto result = parser.parse(payload, -60);
    // Should not crash or cause memory corruption
    SUCCEED();
}

TEST_F(MaliciousInputTest, FormatStringPattern_PercentS) {
    // %s format string attack pattern
    std::string pattern = "%s%s%s%s%s%s%s%s";
    std::vector<uint8_t> payload(pattern.begin(), pattern.end());
    payload.resize(50, 0x00);

    auto result = parser.parse(payload, -60);
    SUCCEED();
}

TEST_F(MaliciousInputTest, FormatStringPattern_Mixed) {
    // Mixed format string pattern
    std::string pattern = "%x%x%x%n%s%d%p";
    std::vector<uint8_t> payload(pattern.begin(), pattern.end());
    payload.resize(50, 0x00);

    auto result = parser.parse(payload, -60);
    SUCCEED();
}

// ============================================
// Oversized Payloads
// ============================================

TEST_F(MaliciousInputTest, OversizedPayload_10KB) {
    auto payload = createRandomPayload(10 * 1024);
    auto result = parser.parse(payload, -60);
    // Should reject or handle gracefully
    SUCCEED();
}

TEST_F(MaliciousInputTest, OversizedPayload_1MB) {
    auto payload = createRandomPayload(1024 * 1024);
    auto result = parser.parse(payload, -60);
    // Should reject or handle gracefully without crash
    SUCCEED();
}

// ============================================
// Truncated Messages
// ============================================

TEST_F(MaliciousInputTest, TruncatedMessage_ASTMBasicID) {
    // ASTM Basic ID message header without full payload
    std::vector<uint8_t> payload = {
        0x16, 0xFF,  // BLE AD type and company ID
        0x0D, 0xFA,  // OpenDroneID company ID
        0x00,        // Message type: Basic ID
        // Missing: 24 bytes of Basic ID data
    };

    auto result = parser.parse(payload, -60);
    EXPECT_FALSE(result.success());
}

TEST_F(MaliciousInputTest, TruncatedMessage_ASTMLocation) {
    // ASTM Location message header without full payload
    std::vector<uint8_t> payload = {
        0x16, 0xFF,  // BLE AD type
        0x0D, 0xFA,  // OpenDroneID
        0x10,        // Message type: Location (0x1 << 4)
        0x00, 0x01,  // Partial location data
        // Missing: rest of location data
    };

    auto result = parser.parse(payload, -60);
    EXPECT_FALSE(result.success());
}

TEST_F(MaliciousInputTest, TruncatedMessage_OneByte) {
    for (uint8_t byte = 0; byte < 255; byte++) {
        std::vector<uint8_t> payload = {byte};
        auto result = parser.parse(payload, -60);
        EXPECT_FALSE(result.success());
    }
}

// ============================================
// Boundary Value Testing
// ============================================

TEST_F(MaliciousInputTest, BoundaryValues_MessageType) {
    // Test all possible message type values (4-bit field)
    for (uint8_t msg_type = 0; msg_type < 16; msg_type++) {
        std::vector<uint8_t> payload = {
            0x16, 0xFF,
            0x0D, 0xFA,
            static_cast<uint8_t>(msg_type << 4),  // Message type in upper nibble
        };
        // Pad with zeros
        payload.resize(30, 0x00);

        auto result = parser.parse(payload, -60);
        // Should not crash for any message type
    }
    SUCCEED();
}

TEST_F(MaliciousInputTest, BoundaryValues_Latitude) {
    // Test boundary latitude values encoded in payload
    // Normal ASTM Location message structure
    std::vector<uint8_t> base_payload = {
        0x16, 0xFF,
        0x0D, 0xFA,
        0x10,        // Message type: Location
        0x00,        // Status
    };

    // Test with max positive value
    std::vector<uint8_t> payload1 = base_payload;
    // Add max int32 value for latitude (4 bytes, little endian)
    payload1.push_back(0xFF);
    payload1.push_back(0xFF);
    payload1.push_back(0xFF);
    payload1.push_back(0x7F);
    payload1.resize(30, 0x00);

    auto result1 = parser.parse(payload1, -60);
    // Should handle boundary value

    // Test with min negative value
    std::vector<uint8_t> payload2 = base_payload;
    payload2.push_back(0x00);
    payload2.push_back(0x00);
    payload2.push_back(0x00);
    payload2.push_back(0x80);
    payload2.resize(30, 0x00);

    auto result2 = parser.parse(payload2, -60);
    // Should handle boundary value

    SUCCEED();
}

TEST_F(MaliciousInputTest, BoundaryValues_Speed) {
    // Test extreme speed values (0 and max)
    std::vector<uint8_t> payload_zero = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x10,        // Location message
        0x00,        // Status
    };
    payload_zero.resize(30, 0x00);

    std::vector<uint8_t> payload_max = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x10,
        0x00,
    };
    // Fill with 0xFF for max values
    payload_max.resize(30, 0xFF);

    auto result1 = parser.parse(payload_zero, -60);
    auto result2 = parser.parse(payload_max, -60);

    SUCCEED();
}

// ============================================
// Special Character Sequences
// ============================================

TEST_F(MaliciousInputTest, SpecialChars_NullBytes) {
    // Payload with embedded null bytes in string fields
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x30,        // Self-ID message (0x3 << 4)
        0x01,        // Description type
        // Description with embedded nulls
        'H', 'e', 'l', 'l', 'o', 0x00, 'W', 'o', 'r', 'l', 'd', 0x00, 0x00, 0x00,
    };
    payload.resize(30, 0x00);

    auto result = parser.parse(payload, -60);
    // Should handle embedded nulls gracefully
    SUCCEED();
}

TEST_F(MaliciousInputTest, SpecialChars_HighASCII) {
    // Payload with high ASCII/non-printable characters
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x30,        // Self-ID
        0x01,
    };

    // Add high ASCII characters
    for (int i = 128; i < 160; i++) {
        payload.push_back(static_cast<uint8_t>(i));
    }
    payload.resize(30, 0x00);

    auto result = parser.parse(payload, -60);
    SUCCEED();
}

TEST_F(MaliciousInputTest, SpecialChars_UTF8_Invalid) {
    // Invalid UTF-8 sequences
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x30,        // Self-ID
        0x01,
        // Invalid UTF-8 sequences
        0xC0, 0xC1,  // Invalid start bytes
        0x80, 0x81,  // Unexpected continuation
        0xFE, 0xFF,  // Invalid UTF-8 bytes
    };
    payload.resize(30, 0x00);

    auto result = parser.parse(payload, -60);
    SUCCEED();
}

// ============================================
// Protocol Confusion Attacks
// ============================================

TEST_F(MaliciousInputTest, ProtocolConfusion_MixedHeaders) {
    // Payload that looks like multiple protocols
    std::vector<uint8_t> payload = {
        // ASTM-like header
        0x16, 0xFF, 0x0D, 0xFA,
        // But then WiFi-like data
        0x0D, 0x00, 0x50, 0x6F,
        // Then ASD-STAN-like
        0x16, 0xFF, 0xFA, 0xFF,
    };
    payload.resize(100, 0xAA);

    auto result = parser.parse(payload, -60);
    // Parser should handle gracefully
    SUCCEED();
}

TEST_F(MaliciousInputTest, ProtocolConfusion_InvalidCompanyID) {
    // Valid structure but invalid company ID
    std::vector<uint8_t> payload = {
        0x16, 0xFF,
        0xDE, 0xAD,  // Invalid company ID
        0x00,        // Would be Basic ID
    };
    payload.resize(30, 0x00);

    auto result = parser.parse(payload, -60);
    EXPECT_FALSE(result.success());
}

// ============================================
// Repeated Packet Attacks
// ============================================

TEST_F(MaliciousInputTest, RepeatedPackets_SamePayload) {
    // Same exact payload repeated many times
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x00,  // Basic ID
    };
    payload.resize(30, 0x00);

    for (int i = 0; i < 1000; i++) {
        auto result = parser.parse(payload, -60);
        // Should not accumulate state in a problematic way
    }
    SUCCEED();
}

TEST_F(MaliciousInputTest, RepeatedPackets_SlightlyDifferent) {
    // Slightly different payloads
    for (int i = 0; i < 1000; i++) {
        std::vector<uint8_t> payload = {
            0x16, 0xFF, 0x0D, 0xFA,
            0x00,
            static_cast<uint8_t>(i & 0xFF),
            static_cast<uint8_t>((i >> 8) & 0xFF),
        };
        payload.resize(30, 0x00);

        auto result = parser.parse(payload, -60);
    }
    SUCCEED();
}

// ============================================
// Memory Stress Tests
// ============================================

TEST_F(MaliciousInputTest, MemoryStress_ManySmallPayloads) {
    // Rapid fire of small payloads
    for (int i = 0; i < 10000; i++) {
        std::vector<uint8_t> payload = createRandomPayload(10, i);
        parser.parse(payload, -60);
    }
    SUCCEED();
}

TEST_F(MaliciousInputTest, MemoryStress_AlternatingSize) {
    // Alternating between small and large payloads
    for (int i = 0; i < 100; i++) {
        size_t size = (i % 2 == 0) ? 10 : 1000;
        auto payload = createRandomPayload(size, i);
        parser.parse(payload, -60);
    }
    SUCCEED();
}

// ============================================
// RSSI Edge Cases
// ============================================

TEST_F(MaliciousInputTest, RSSI_Extreme_Values) {
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x00,
    };
    payload.resize(30, 0x00);

    // Test extreme RSSI values
    parser.parse(payload, 0);       // Maximum (closest)
    parser.parse(payload, -127);    // Minimum (farthest)
    parser.parse(payload, 127);     // Invalid positive value
    parser.parse(payload, -128);    // Minimum int8_t

    SUCCEED();
}

// ============================================
// Message Pack Attack
// ============================================

TEST_F(MaliciousInputTest, MessagePack_TooManyMessages) {
    // Message Pack claiming to have 255 messages
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0xF0,        // Message Pack (0xF << 4)
        0xFF,        // Claiming 255 messages
    };
    payload.resize(50, 0x00);

    auto result = parser.parse(payload, -60);
    // Should reject or handle gracefully
    SUCCEED();
}

TEST_F(MaliciousInputTest, MessagePack_RecursiveNesting) {
    // Message Pack containing another Message Pack
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0xF0,        // Message Pack
        0x02,        // 2 messages
        // First "message" is another Message Pack
        0xF0, 0x01,
        // ... with yet another
        0xF0, 0x01,
    };
    payload.resize(100, 0x00);

    auto result = parser.parse(payload, -60);
    // Should not recurse infinitely
    SUCCEED();
}

// ============================================
// Authentication Message Attacks
// ============================================

TEST_F(MaliciousInputTest, Authentication_OversizedPage) {
    // Authentication message with invalid page index
    std::vector<uint8_t> payload = {
        0x16, 0xFF, 0x0D, 0xFA,
        0x20,        // Authentication (0x2 << 4)
        0x00,        // Auth type
        0xFF,        // Invalid page index (should be 0-15)
    };
    payload.resize(30, 0x00);

    auto result = parser.parse(payload, -60);
    // Should reject invalid page
    SUCCEED();
}

TEST_F(MaliciousInputTest, Authentication_InvalidAuthType) {
    for (uint8_t auth_type = 0; auth_type < 16; auth_type++) {
        std::vector<uint8_t> payload = {
            0x16, 0xFF, 0x0D, 0xFA,
            0x20,
            auth_type,
            0x00,  // Page 0
        };
        payload.resize(30, 0x00);

        auto result = parser.parse(payload, -60);
        // Should handle all auth types
    }
    SUCCEED();
}
