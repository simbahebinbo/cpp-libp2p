/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_CONNECTION_ASIO_SSL_HPP
#define LIBP2P_CONNECTION_ASIO_SSL_HPP

#include <boost/asio/ssl/stream.hpp>
#include <libp2p/connection/asio/socket.hpp>

namespace libp2p {
  struct AsioSocketSsl : IAsioSocket {
    AsioSocketSsl(std::shared_ptr<boost::asio::ssl::context> context,
                  AsioSocket inner)
        : context{std::move(context)},
          socket{std::move(inner), *this->context} {}

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

    std::shared_ptr<boost::asio::ssl::context> context;
    boost::asio::ssl::stream<AsioSocket> socket;
  };
}  // namespace libp2p

#endif  // LIBP2P_CONNECTION_ASIO_SSL_HPP
