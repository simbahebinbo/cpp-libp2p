/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/transport/tcp/tcp_listener.hpp>

#include <libp2p/connection/asio/ssl.hpp>
#include <libp2p/connection/asio/tcp.hpp>
#include <libp2p/connection/asio/ws.hpp>
#include <libp2p/log/logger.hpp>
#include <libp2p/transport/impl/upgrader_session.hpp>

namespace libp2p::transport {
  template <typename F>
  void acceptWs(AsioSocket socket, const multi::Multiaddress &address,
                const std::shared_ptr<boost::asio::ssl::context> &context,
                F f) {
    using P = multi::Protocol::Code;
    auto ws = [=](AsioSocket socket, F f) {
      auto ws = std::make_shared<AsioSocketWs>(std::move(socket));
      ws->socket.async_accept(
          [=, f{std::move(f)}](boost::system::error_code ec) mutable {
            if (ec) {
              return f(ec);
            }
            f(AsioSocket{std::move(ws)});
          });
    };
    if (address.hasProtocol(P::WS)) {
      return ws(std::move(socket), std::move(f));
    }
    if (not address.hasProtocol(P::WSS)) {
      auto executor = socket.executor;
      return boost::asio::post(
          executor, [socket{std::move(socket)}, f{std::move(f)}]() mutable {
            f(std::move(socket));
          });
    }
    auto ssl = std::make_shared<AsioSocketSsl>(context, std::move(socket));
    ssl->socket.async_handshake(
        boost::asio::ssl::stream_base::handshake_type::server,
        [=, f{std::move(f)}](boost::system::error_code ec) mutable {
          if (ec) {
            return f(ec);
          }
          ws(AsioSocket{std::move(ssl)}, std::move(f));
        });
  }
  outcome::result<SslServerConfig> SslServerConfig::make(std::string_view pem) {
    auto context = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::tls);
    boost::system::error_code ec;
    context->use_private_key(boost::asio::buffer(pem),
                             boost::asio::ssl::context::file_format::pem, ec);
    if (ec) {
      return ec;
    }
    context->use_certificate_chain(boost::asio::buffer(pem), ec);
    if (ec) {
      return ec;
    }
    return SslServerConfig{context};
  }

  TcpListener::TcpListener(boost::asio::io_context &context,
                           SslServerConfig ssl_server_config,
                           std::shared_ptr<Upgrader> upgrader,
                           TransportListener::HandlerFunc handler)
      : context_(context),
        acceptor_(context_),
        ssl_server_config_{std::move(ssl_server_config)},
        upgrader_(std::move(upgrader)),
        handle_(std::move(handler)) {}

  outcome::result<void> TcpListener::listen(
      const multi::Multiaddress &address) {
    if (!canListen(address)) {
      return std::errc::address_family_not_supported;
    }

    if (acceptor_.is_open()) {
      return std::errc::already_connected;
    }

    address_ = address;
    tcp_address_ = *TcpMultiaddress::from(address);

    // TODO(@warchant): replace with parser PRE-129
    using namespace boost::asio;  // NOLINT
    try {
      OUTCOME_TRY(endpoint, tcp_address_->endpoint());

      // setup acceptor, throws
      acceptor_.open(endpoint.protocol());
      acceptor_.set_option(ip::tcp::acceptor::reuse_address(true));
      acceptor_.bind(endpoint);
      acceptor_.listen();

      // start listening
      doAccept();

      return outcome::success();
    } catch (const boost::system::system_error &e) {
      log::createLogger("Listener")
          ->error("Cannot listen to {}: {}", address.getStringAddress(),
                  e.code().message());
      return e.code();
    }
  }

  bool TcpListener::canListen(const multi::Multiaddress &ma) const {
    auto tcp = TcpMultiaddress::from(ma);
    if (tcp and tcp->wss and not ssl_server_config_.context) {
      return false;
    }
    return tcp.has_value();
  }

  outcome::result<multi::Multiaddress> TcpListener::getListenMultiaddr() const {
    boost::system::error_code ec;
    auto endpoint = acceptor_.local_endpoint(ec);
    if (ec) {
      return ec;
    }
    return tcp_address_->multiaddress(endpoint);
  }

  bool TcpListener::isClosed() const {
    return !acceptor_.is_open();
  }

  outcome::result<void> TcpListener::close() {
    boost::system::error_code ec;
    acceptor_.close(ec);
    if (ec) {
      return outcome::failure(ec);
    }
    return outcome::success();
  }

  void TcpListener::doAccept() {
    using namespace boost::asio;    // NOLINT
    using namespace boost::system;  // NOLINT

    if (!acceptor_.is_open()) {
      return;
    }

    acceptor_.async_accept([self{this->shared_from_this()}](
                               boost::system::error_code ec,
                               ip::tcp::socket sock) {
      if (ec) {
        return self->handle_(ec);
      }
      auto endpoint = sock.remote_endpoint(ec);
      if (not ec) {
        auto remote = self->tcp_address_->multiaddress(endpoint);
        acceptWs(AsioSocket{std::make_shared<AsioSocketTcp>(std::move(sock))},
                 *self->address_, self->ssl_server_config_.context,
                 [=](outcome::result<AsioSocket> _socket) mutable {
                   if (not _socket) {
                     return;
                   }
                   auto conn = std::make_shared<TcpConnection>(
                       std::move(_socket.value()), false, *self->address_,
                       remote);

                   auto session = std::make_shared<UpgraderSession>(
                       self->upgrader_, std::move(conn), self->handle_);

                   session->secureInbound();
                 });
      }

      self->doAccept();
    });
  };

}  // namespace libp2p::transport
