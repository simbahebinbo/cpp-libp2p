/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/transport/tcp/tcp_connection.hpp>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <libp2p/transport/tcp/tcp_util.hpp>

#define TRACE_ENABLED 0
#include <libp2p/common/trace.hpp>

namespace libp2p::transport {

  namespace {
    auto &log() {
      static auto logger = log::createLogger("tcp-conn");
      return *logger;
    }

    inline std::error_code convert(boost::system::errc::errc_t ec) {
      return std::error_code(static_cast<int>(ec), std::system_category());
    }

    inline std::error_code convert(std::error_code ec) {
      return ec;
    }
  }  // namespace

  TcpConnection::TcpConnection(AsioSocket &&socket, bool initiator,
                               multi::Multiaddress local_multiaddress,
                               multi::Multiaddress remote_multiaddress)
      : socket_(std::move(socket)),
        initiator_{initiator},
        local_multiaddress_{std::move(local_multiaddress)},
        remote_multiaddress_{std::move(remote_multiaddress)} {
#ifndef NDEBUG
    debug_str_ = fmt::format("{} {} {}", local_multiaddress_.getStringAddress(),
                             initiator_ ? "->" : "<-",
                             remote_multiaddress_.getStringAddress());
#endif
  }

  outcome::result<void> TcpConnection::close() {
    closed_by_host_ = true;
    close(convert(boost::system::errc::connection_aborted));
    return outcome::success();
  }

  void TcpConnection::close(std::error_code reason) {
    assert(reason);

    if (!close_reason_) {
      close_reason_ = reason;
      log().debug("{} closing with reason: {}", debug_str_,
                  close_reason_.message());
    }
    socket_.close();
  }

  bool TcpConnection::isClosed() const {
    return closed_by_host_ || close_reason_;
  }

  outcome::result<multi::Multiaddress> TcpConnection::remoteMultiaddr() {
    return remote_multiaddress_;
  }

  outcome::result<multi::Multiaddress> TcpConnection::localMultiaddr() {
    return local_multiaddress_;
  }

  bool TcpConnection::isInitiator() const noexcept {
    return initiator_;
  }

  namespace {
    template <typename Callback>
    auto closeOnError(TcpConnection &conn, Callback cb) {
      return [cb{std::move(cb)}, wptr{conn.weak_from_this()}](auto ec,
                                                              auto result) {
        if (auto self = wptr.lock()) {
          if (ec) {
            self->close(convert(ec));
            return cb(std::forward<decltype(ec)>(ec));
          }
          TRACE("{} {}", self->str(), result);
          cb(result);
        } else {
          log().debug("connection wptr expired");
        }
      };
    }
  }  // namespace

  void TcpConnection::read(gsl::span<uint8_t> out, size_t bytes,
                           TcpConnection::ReadCallbackFunc cb) {
    TRACE("{} read {}", debug_str_, bytes);
    boost::asio::async_read(socket_, detail::makeBuffer(out, bytes),
                            closeOnError(*this, std::move(cb)));
  }

  void TcpConnection::readSome(gsl::span<uint8_t> out, size_t bytes,
                               TcpConnection::ReadCallbackFunc cb) {
    TRACE("{} read some up to {}", debug_str_, bytes);
    socket_.async_read_some(detail::makeBuffer(out, bytes),
                            closeOnError(*this, std::move(cb)));
  }

  void TcpConnection::write(gsl::span<const uint8_t> in, size_t bytes,
                            TcpConnection::WriteCallbackFunc cb) {
    TRACE("{} write {}", debug_str_, bytes);
    boost::asio::async_write(socket_, detail::makeBuffer(in, bytes),
                             closeOnError(*this, std::move(cb)));
  }

  void TcpConnection::writeSome(gsl::span<const uint8_t> in, size_t bytes,
                                TcpConnection::WriteCallbackFunc cb) {
    TRACE("{} write some up to {}", debug_str_, bytes);
    socket_.async_write_some(detail::makeBuffer(in, bytes),
                             closeOnError(*this, std::move(cb)));
  }

  void TcpConnection::deferReadCallback(outcome::result<size_t> res,
                                        ReadCallbackFunc cb) {
    // defers callback to the next event loop cycle,
    // cb will be called iff TcpConnection is still alive
    // and was not closed by host's side
    boost::asio::post(socket_.get_executor(),
                      [weak{weak_from_this()}, res, cb{std::move(cb)}]() {
                        if (auto self = weak.lock();
                            not self or self->closed_by_host_) {
                          return;
                        }
                        cb(res);
                      });
  }

  void TcpConnection::deferWriteCallback(std::error_code ec,
                                         WriteCallbackFunc cb) {
    deferReadCallback(ec, std::move(cb));
  }
}  // namespace libp2p::transport
