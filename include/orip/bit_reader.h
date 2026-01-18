#ifndef ORIP_BIT_READER_H
#define ORIP_BIT_READER_H

#include <cstdint>
#include <cstddef>

namespace orip {
namespace utils {

// Utility class for reading bits and bytes from a buffer
// Handles little-endian byte order (as used in BLE)
class BitReader {
public:
    BitReader(const uint8_t* data, size_t length);

    // Read unsigned integers (little-endian)
    uint8_t readU8();
    uint16_t readU16();
    uint32_t readU32();

    // Read signed integers (little-endian)
    int8_t readI8();
    int16_t readI16();
    int32_t readI32();

    // Read specific number of bits (up to 32)
    uint32_t readBits(size_t count);

    // Read bytes into buffer
    void readBytes(uint8_t* dest, size_t count);

    // Skip bytes
    void skip(size_t count);

    // Check if more data available
    bool hasMore() const;
    size_t remaining() const;

    // Get current position
    size_t position() const;

    // Reset to beginning
    void reset();

private:
    const uint8_t* data_;
    size_t length_;
    size_t pos_;
    size_t bit_pos_;  // For bit-level reading
};

// Inline helper functions for endian conversion
inline uint16_t readLE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) |
           (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t readLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline int32_t readLE32Signed(const uint8_t* p) {
    return static_cast<int32_t>(readLE32(p));
}

} // namespace utils
} // namespace orip

#endif // ORIP_BIT_READER_H
