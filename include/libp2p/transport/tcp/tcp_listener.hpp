/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_TCP_LISTENER_HPP
#define LIBP2P_TCP_LISTENER_HPP

#include <boost/asio.hpp>
#include <libp2p/transport/tcp/address.hpp>
#include <libp2p/transport/tcp/tcp_connection.hpp>
#include <libp2p/transport/transport_listener.hpp>
#include <libp2p/transport/upgrader.hpp>

namespace boost::asio::ssl {
  class context;
}  // namespace boost::asio::ssl

namespace libp2p::transport {
  struct SslServerConfig {
    static outcome::result<SslServerConfig> make(std::string_view pem);

    std::shared_ptr<boost::asio::ssl::context> context = nullptr;
  };

  /**
   * @brief TCP Server (Listener) implementation.
   */
  class TcpListener : public TransportListener,
                      public std::enable_shared_from_this<TcpListener> {
   public:
    ~TcpListener() override = default;

    TcpListener(boost::asio::io_context &context,
                SslServerConfig ssl_server_config,
                std::shared_ptr<Upgrader> upgrader,
                TransportListener::HandlerFunc handler);

    outcome::result<void> listen(const multi::Multiaddress &address) override;

    bool canListen(const multi::Multiaddress &ma) const override;

    outcome::result<multi::Multiaddress> getListenMultiaddr() const override;

    bool isClosed() const override;

    outcome::result<void> close() override;

   private:
    boost::asio::io_context &context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    SslServerConfig ssl_server_config_;
    std::shared_ptr<Upgrader> upgrader_;
    TransportListener::HandlerFunc handle_;
    boost::optional<multi::Multiaddress> address_;
    boost::optional<TcpMultiaddress> tcp_address_;

    void doAccept();
  };

}  // namespace libp2p::transport

#endif  // LIBP2P_TCP_LISTENER_HPP
