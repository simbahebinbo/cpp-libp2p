/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_TCP_CONNECTION_HPP
#define LIBP2P_TCP_CONNECTION_HPP

#include <boost/noncopyable.hpp>
#include <libp2p/common/metrics/instance_count.hpp>
#include <libp2p/connection/asio/socket.hpp>
#include <libp2p/connection/raw_connection.hpp>
#include <libp2p/multi/multiaddress.hpp>

namespace libp2p::security {
  class TlsAdaptor;
}  // namespace libp2p::security

namespace libp2p::connection {
  class TlsConnection;
}  // namespace libp2p::connection

namespace libp2p::transport {

  /**
   * @brief boost::asio implementation of TCP connection (socket).
   */
  class TcpConnection : public connection::RawConnection,
                        public std::enable_shared_from_this<TcpConnection>,
                        private boost::noncopyable {
   public:
    TcpConnection(AsioSocket &&socket, bool initiator,
                  multi::Multiaddress local_multiaddress,
                  multi::Multiaddress remote_multiaddress);

    void read(gsl::span<uint8_t> out, size_t bytes,
              ReadCallbackFunc cb) override;

    void readSome(gsl::span<uint8_t> out, size_t bytes,
                  ReadCallbackFunc cb) override;

    void deferReadCallback(outcome::result<size_t> res,
                           ReadCallbackFunc cb) override;

    void write(gsl::span<const uint8_t> in, size_t bytes,
               WriteCallbackFunc cb) override;

    void writeSome(gsl::span<const uint8_t> in, size_t bytes,
                   WriteCallbackFunc cb) override;

    void deferWriteCallback(std::error_code ec, WriteCallbackFunc cb) override;

    outcome::result<multi::Multiaddress> remoteMultiaddr() override;

    outcome::result<multi::Multiaddress> localMultiaddr() override;

    bool isInitiator() const noexcept override;

    outcome::result<void> close() override;

    bool isClosed() const override;

    /// Called from network part with close errors
    /// or from close() if is closing by the host
    void close(std::error_code reason);

    // TODO (artem) make RawConnection::id()->string or str() or whatever
    const std::string &str() const {
      return debug_str_;
    }

   private:
    AsioSocket socket_;
    bool initiator_ = false;

    /// If true then no more callbacks will be issued
    bool closed_by_host_ = false;

    /// Close reason, is set on close to respond to further calls
    std::error_code close_reason_;

    multi::Multiaddress local_multiaddress_;
    multi::Multiaddress remote_multiaddress_;

    friend class security::TlsAdaptor;
    friend class connection::TlsConnection;

    std::string debug_str_;

   public:
    LIBP2P_METRICS_INSTANCE_COUNT_IF_ENABLED(libp2p::transport::TcpConnection);
  };
}  // namespace libp2p::transport

#endif  // LIBP2P_TCP_CONNECTION_HPP
