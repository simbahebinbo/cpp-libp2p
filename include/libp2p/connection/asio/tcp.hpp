/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_CONNECTION_ASIO_TCP_HPP
#define LIBP2P_CONNECTION_ASIO_TCP_HPP

#include <boost/asio/ip/tcp.hpp>
#include <libp2p/connection/asio/socket.hpp>

namespace libp2p {
  struct AsioSocketTcp : IAsioSocket {
    AsioSocketTcp(boost::asio::ip::tcp::socket &&socket)
        : socket{std::move(socket)} {}

    boost::asio::executor get_executor() override {
      return socket.get_executor();
    }

    void async_read_some(const boost::asio::mutable_buffer &buffer,
                         Cb cb) override {
      socket.async_read_some(buffer, std::move(cb));
    }

    void async_write_some(const boost::asio::const_buffer &buffer,
                          Cb cb) override {
      socket.async_write_some(buffer, std::move(cb));
    }

    boost::asio::ip::tcp::socket socket;
  };
}  // namespace libp2p

#endif  // LIBP2P_CONNECTION_ASIO_TCP_HPP
