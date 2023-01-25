/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_CONNECTION_ASIO_WS_HPP
#define LIBP2P_CONNECTION_ASIO_WS_HPP

#include <boost/beast/core/role.hpp>
#include <boost/beast/websocket.hpp>
#include <libp2p/connection/asio/socket.hpp>

namespace libp2p {
  inline void beast_close_socket(AsioSocket &stream) {}

  template <typename Cb>
  void async_teardown(boost::beast::role_type role, AsioSocket &stream,
                      Cb &&cb) {}

  struct AsioSocketWs : IAsioSocket {
    AsioSocketWs(AsioSocket inner) : socket{std::move(inner)} {
      socket.binary(true);
    }

    boost::asio::executor get_executor() override {
      return socket.get_executor();
    }

    void async_read_some(const boost::asio::mutable_buffer &buffer,
                         Cb cb) override {
      socket.async_read_some(buffer, std::move(cb));
    }

    void async_write_some(const boost::asio::const_buffer &buffer,
                          Cb cb) override {
      socket.async_write_some(true, buffer, std::move(cb));
    }

    boost::beast::websocket::stream<AsioSocket> socket;
  };
}  // namespace libp2p

#endif  // LIBP2P_CONNECTION_ASIO_WS_HPP
