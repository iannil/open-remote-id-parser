#include <gtest/gtest.h>
#include "orip/bit_reader.h"

using namespace orip::utils;

class BitReaderTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(BitReaderTest, ReadU8) {
    uint8_t data[] = {0x12, 0x34, 0x56};
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.readU8(), 0x12);
    EXPECT_EQ(reader.readU8(), 0x34);
    EXPECT_EQ(reader.readU8(), 0x56);
    EXPECT_FALSE(reader.hasMore());
}

TEST_F(BitReaderTest, ReadU16LittleEndian) {
    uint8_t data[] = {0x34, 0x12};  // 0x1234 in little-endian
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.readU16(), 0x1234);
}

TEST_F(BitReaderTest, ReadU32LittleEndian) {
    uint8_t data[] = {0x78, 0x56, 0x34, 0x12};  // 0x12345678 in little-endian
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.readU32(), 0x12345678u);
}

TEST_F(BitReaderTest, ReadI8Signed) {
    uint8_t data[] = {0xFF, 0x80, 0x7F};  // -1, -128, 127
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.readI8(), -1);
    EXPECT_EQ(reader.readI8(), -128);
    EXPECT_EQ(reader.readI8(), 127);
}

TEST_F(BitReaderTest, ReadI16Signed) {
    uint8_t data[] = {0xFF, 0xFF};  // -1 in little-endian
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.readI16(), -1);
}

TEST_F(BitReaderTest, Skip) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    BitReader reader(data, sizeof(data));

    reader.skip(2);
    EXPECT_EQ(reader.readU8(), 0x03);
    EXPECT_EQ(reader.remaining(), 2u);
}

TEST_F(BitReaderTest, ReadBytes) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t dest[4] = {0};
    BitReader reader(data, sizeof(data));

    reader.readBytes(dest, 4);
    EXPECT_EQ(dest[0], 0x01);
    EXPECT_EQ(dest[1], 0x02);
    EXPECT_EQ(dest[2], 0x03);
    EXPECT_EQ(dest[3], 0x04);
}

TEST_F(BitReaderTest, Position) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    BitReader reader(data, sizeof(data));

    EXPECT_EQ(reader.position(), 0u);
    reader.readU8();
    EXPECT_EQ(reader.position(), 1u);
    reader.readU8();
    EXPECT_EQ(reader.position(), 2u);
}

TEST_F(BitReaderTest, Reset) {
    uint8_t data[] = {0x12, 0x34};
    BitReader reader(data, sizeof(data));

    reader.readU8();
    reader.reset();
    EXPECT_EQ(reader.position(), 0u);
    EXPECT_EQ(reader.readU8(), 0x12);
}

TEST_F(BitReaderTest, BufferUnderflow) {
    uint8_t data[] = {0x01};
    BitReader reader(data, sizeof(data));

    reader.readU8();
    EXPECT_THROW(reader.readU8(), std::out_of_range);
}

TEST_F(BitReaderTest, ReadLE16Helper) {
    uint8_t data[] = {0xFA, 0xFF};  // 0xFFFA (ODID Service UUID)
    EXPECT_EQ(readLE16(data), 0xFFFA);
}

TEST_F(BitReaderTest, ReadLE32Helper) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(readLE32(data), 0x04030201u);
}
