#include "orip/bit_reader.h"
#include <algorithm>
#include <stdexcept>

namespace orip {
namespace utils {

BitReader::BitReader(const uint8_t* data, size_t length)
    : data_(data), length_(length), pos_(0), bit_pos_(0) {}

uint8_t BitReader::readU8() {
    if (pos_ >= length_) {
        throw std::out_of_range("BitReader: buffer underflow");
    }
    return data_[pos_++];
}

uint16_t BitReader::readU16() {
    if (pos_ + 2 > length_) {
        throw std::out_of_range("BitReader: buffer underflow");
    }
    uint16_t result = readLE16(data_ + pos_);
    pos_ += 2;
    return result;
}

uint32_t BitReader::readU32() {
    if (pos_ + 4 > length_) {
        throw std::out_of_range("BitReader: buffer underflow");
    }
    uint32_t result = readLE32(data_ + pos_);
    pos_ += 4;
    return result;
}

int8_t BitReader::readI8() {
    return static_cast<int8_t>(readU8());
}

int16_t BitReader::readI16() {
    return static_cast<int16_t>(readU16());
}

int32_t BitReader::readI32() {
    return static_cast<int32_t>(readU32());
}

uint32_t BitReader::readBits(size_t count) {
    if (count > 32) {
        throw std::invalid_argument("BitReader: cannot read more than 32 bits");
    }

    uint32_t result = 0;
    size_t bits_read = 0;

    while (bits_read < count) {
        if (pos_ >= length_) {
            throw std::out_of_range("BitReader: buffer underflow");
        }

        size_t bits_available = 8 - bit_pos_;
        size_t bits_to_read = std::min(bits_available, count - bits_read);

        uint8_t mask = ((1 << bits_to_read) - 1) << bit_pos_;
        uint8_t bits = (data_[pos_] & mask) >> bit_pos_;

        result |= static_cast<uint32_t>(bits) << bits_read;

        bits_read += bits_to_read;
        bit_pos_ += bits_to_read;

        if (bit_pos_ >= 8) {
            bit_pos_ = 0;
            pos_++;
        }
    }

    return result;
}

void BitReader::readBytes(uint8_t* dest, size_t count) {
    if (pos_ + count > length_) {
        throw std::out_of_range("BitReader: buffer underflow");
    }
    std::copy(data_ + pos_, data_ + pos_ + count, dest);
    pos_ += count;
}

void BitReader::skip(size_t count) {
    if (pos_ + count > length_) {
        throw std::out_of_range("BitReader: buffer underflow");
    }
    pos_ += count;
}

bool BitReader::hasMore() const {
    return pos_ < length_;
}

size_t BitReader::remaining() const {
    return length_ - pos_;
}

size_t BitReader::position() const {
    return pos_;
}

void BitReader::reset() {
    pos_ = 0;
    bit_pos_ = 0;
}

} // namespace utils
} // namespace orip
