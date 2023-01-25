/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/transport/tcp/tcp_transport.hpp>

#include <libp2p/connection/asio/ssl.hpp>
#include <libp2p/connection/asio/tcp.hpp>
#include <libp2p/connection/asio/ws.hpp>
#include <libp2p/transport/impl/upgrader_session.hpp>

namespace libp2p::transport {
  template <typename Executor, typename F>
  void connectTcp(Executor executor, const multi::Multiaddress &address, F f) {
    auto [host, port] = detail::getHostAndTcpPort(address);
    using boost::asio::ip::tcp;
    auto resolver = std::make_shared<tcp::resolver>(executor);
    auto next = [=, f{std::move(f)}](
                    boost::system::error_code ec,
                    tcp::resolver::results_type resolved) mutable {
      std::ignore = resolver;
      if (ec) {
        return f(ec);
      }
      auto socket = std::make_shared<AsioSocketTcp>(tcp::socket{executor});
      boost::asio::async_connect(
          socket->socket, resolved,
          [=, f{std::move(f)}](boost::system::error_code ec,
                               tcp::endpoint) mutable {
            if (ec) {
              return f(ec);
            }
            f(std::move(socket));
          });
    };
    using P = multi::Protocol::Code;
    switch (detail::getFirstProtocol(address)) {
      case P::DNS4:
        return resolver->async_resolve(tcp::v4(), host, port, std::move(next));
      case P::DNS6:
        return resolver->async_resolve(tcp::v6(), host, port, std::move(next));
      default:  // Could be only DNS, IP6 or IP4 as tcpResolve already
                // checked for that in the beginning of the method
        return resolver->async_resolve(host, port, std::move(next));
    }
  }

  template <typename F>
  void connectWs(AsioSocket socket, const multi::Multiaddress &address,
                 const std::shared_ptr<boost::asio::ssl::context> &context,
                 F f) {
    auto [host, port] = detail::getHostAndTcpPort(address);
    using P = multi::Protocol::Code;
    auto ws = [=, host{host}](AsioSocket socket, F f) {
      auto ws = std::make_shared<AsioSocketWs>(std::move(socket));
      ws->socket.async_handshake(
          host, "/",
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
        boost::asio::ssl::stream_base::handshake_type::client,
        [=, f{std::move(f)}](boost::system::error_code ec) mutable {
          if (ec) {
            return f(ec);
          }
          ws(AsioSocket{std::move(ssl)}, std::move(f));
        });
  }

  void TcpTransport::dial(const peer::PeerId &remoteId,
                          multi::Multiaddress address,
                          TransportAdaptor::HandlerFunc handler) {
    dial(remoteId, std::move(address), std::move(handler),
         std::chrono::milliseconds::zero());
  }

  void TcpTransport::dial(const peer::PeerId &remoteId,
                          multi::Multiaddress address,
                          TransportAdaptor::HandlerFunc handler,
                          std::chrono::milliseconds timeout) {
    if (!canDial(address)) {
      context_->post([handler{std::move(handler)}] {
        handler(std::errc::address_family_not_supported);
      });
      return;
    }

    connectTcp(
        context_->get_executor(), address,
        [=, self{shared_from_this()}, cb{std::move(handler)}](
            outcome::result<std::shared_ptr<AsioSocketTcp>> _tcp) mutable {
          if (not _tcp) {
            return cb(_tcp.error());
          }
          auto &tcp = _tcp.value();
          boost::system::error_code ec;
          auto endpoint = tcp->socket.local_endpoint(ec);
          if (ec) {
            return cb(ec);
          }
          auto _local = detail::makeAddress(endpoint, address);
          if (not _local) {
            return cb(_local.error());
          }
          connectWs(AsioSocket{std::move(_tcp.value())}, address,
                    client_ssl_context_,
                    [=, local{std::move(_local.value())}](
                        outcome::result<AsioSocket> _socket) mutable {
                      if (not _socket) {
                        return cb(_socket.error());
                      }
                      auto conn = std::make_shared<TcpConnection>(
                          std::move(_socket.value()), true, local, address);
                      auto session = std::make_shared<UpgraderSession>(
                          self->upgrader_, std::move(conn), cb);
                      session->secureOutbound(remoteId);
                    });
        });
  }

  std::shared_ptr<TransportListener> TcpTransport::createListener(
      TransportListener::HandlerFunc handler) {
    return std::make_shared<TcpListener>(*context_, ssl_server_config_,
                                         upgrader_, std::move(handler));
  }

  bool TcpTransport::canDial(const multi::Multiaddress &ma) const {
    return detail::supportsIpTcp(ma);
  }

  TcpTransport::TcpTransport(std::shared_ptr<boost::asio::io_context> context,
                             SslServerConfig ssl_server_config,
                             std::shared_ptr<Upgrader> upgrader)
      : context_{std::move(context)},
        ssl_server_config_{std::move(ssl_server_config)},
        upgrader_{std::move(upgrader)} {
    client_ssl_context_ = std::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::tls);
  }

  peer::Protocol TcpTransport::getProtocolId() const {
    return "/tcp/1.0.0";
  }

}  // namespace libp2p::transport
