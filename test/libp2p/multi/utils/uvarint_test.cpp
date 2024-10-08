/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/multi/uvarint.hpp>

#include <gtest/gtest.h>
#include <libp2p/common/literals.hpp>

using libp2p::multi::UVarint;
using libp2p::common::operator""_unhex;

/**
 * @given an unsigned integer
 * @when creating a varint from the integer
 * @then a valid varint is created, which yields the original integer when
 * decoded
 */
TEST(UVarint, CreateFromInt) {
  UVarint v(2);
  ASSERT_EQ(v.toUInt64(), 2);
  v = 3245;
  ASSERT_EQ(v.toUInt64(), 3245);
  v = 0;
  ASSERT_EQ(v.toUInt64(), 0);
}

/**
 * @given an unsigned integer
 * @when creating a varint from the integer
 * @then a valid varint is created, which binary representation corresponds to
 * the varint standard
 */
TEST(UVarint, CorrectEncoding) {
  UVarint var(1);
  ASSERT_EQ(var.toVector(), "01"_unhex);
  var = 127;
  ASSERT_EQ(var.toVector(), "7F"_unhex);
  var = 128;
  ASSERT_EQ(var.toVector(), "8001"_unhex);
  var = 255;
  ASSERT_EQ(var.toVector(), "FF01"_unhex);
  var = 300;
  ASSERT_EQ(var.toVector(), "AC02"_unhex);
  var = 16384;
  ASSERT_EQ(var.toVector(), "808001"_unhex);
}

/**
 * @given a byte array
 * @when assessing the size of a varint stored in the array
 * @then result is the size corresponding to the varint standard
 */
TEST(UVarint, CalculateSize) {
  uint8_t bytes[] = {0x81, 0xA3, 0x75, 0x43, 0xAA};
  ASSERT_EQ(UVarint::calculateSize(std::span(bytes, 5)), 3);

  uint8_t another_bytes[] = {0x71, 0xA3, 0x75, 0x43, 0xAA};
  ASSERT_EQ(UVarint::calculateSize(std::span(another_bytes, 5)), 1);
}

/**
 * @given Sample integers from (N/2, N)
 * @when Encoding and decoding back sample integer
 * @then Encoding/decoding must be successful without losses of data
 */
TEST(UVarint, ReversibilitySuccess) {
  uint64_t max = std::numeric_limits<uint64_t>::max() / 2;
  for (uint64_t data = 2; data < max; data *= 2) {
    UVarint var{data};
    uint64_t decoded = var.toUInt64();
    ASSERT_EQ(data, decoded);
  }
}

/**
 * @given Minimum and maximum possible values
 * @when Encoding and decoding back sample integer
 * @then Encoding/decoding must be successful without losses of data
 */
TEST(UVarint, EncodeLimitsAreCorrect) {
  uint64_t min = std::numeric_limits<uint64_t>::min();
  uint64_t max = std::numeric_limits<uint64_t>::max();
  UVarint var_min{min};
  UVarint var_max{max};
  ASSERT_EQ(min, var_min.toUInt64());
  ASSERT_EQ(max, var_max.toUInt64());
}

/**
 * @given Encoded 2^64 value bytes (max size of uint64_t is 2^64-1)
 * @when Creating new UVarint from raw bytes
 * @then Decoding raw bytes must be failed
 */
TEST(UVaring, DecodeOverflowFailure) {
  std::vector<uint8_t> overflow_encoded_data{
      {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02}};
  auto var = UVarint::create(overflow_encoded_data);
  ASSERT_FALSE(var);
}
