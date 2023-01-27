/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_TCP_CONNECTION_UTIL_HPP
#define LIBP2P_TCP_CONNECTION_UTIL_HPP

#include <boost/asio/buffer.hpp>
#include <gsl/span>

namespace libp2p::transport::detail {
  inline auto makeBuffer(gsl::span<uint8_t> s) {
    return boost::asio::buffer(s.data(), s.size());
  }

  inline auto makeBuffer(gsl::span<uint8_t> s, size_t size) {
    return boost::asio::buffer(s.data(), size);
  }

  inline auto makeBuffer(gsl::span<const uint8_t> s) {
    return boost::asio::buffer(s.data(), s.size());
  }

  inline auto makeBuffer(gsl::span<const uint8_t> s, size_t size) {
    return boost::asio::buffer(s.data(), size);
  }
}  // namespace libp2p::transport::detail

#endif  // LIBP2P_TCP_CONNECTION_UTIL_HPP
