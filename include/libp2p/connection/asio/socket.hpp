/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_CONNECTION_ASIO_STREAM_HPP
#define LIBP2P_CONNECTION_ASIO_STREAM_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/detail/buffer_sequence_adapter.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <functional>
#include <libp2p/connection/asio/shared_fn.hpp>

namespace libp2p {
  struct IAsioSocket {
    virtual ~IAsioSocket() = default;

    virtual boost::asio::executor get_executor() = 0;

    using Cb = std::function<void(boost::system::error_code, size_t)>;
    virtual void async_read_some(const boost::asio::mutable_buffer &buffer,
                                 Cb cb) = 0;
    virtual void async_write_some(const boost::asio::const_buffer &buffer,
                                  Cb cb) = 0;
  };

  struct AsioSocket {
    AsioSocket(std::shared_ptr<IAsioSocket> impl)
        : executor{impl->get_executor()}, impl{std::move(impl)} {}

    using lowest_layer_type = AsioSocket;
    lowest_layer_type &lowest_layer() {
      return *this;
    }
    const lowest_layer_type &lowest_layer() const {
      return *this;
    }

    using executor_type = boost::asio::executor;
    boost::asio::executor get_executor() {
      return executor;
    }

    template <typename MutableBufferSequence, typename Cb>
    void async_read_some(const MutableBufferSequence &buffers, Cb &&cb) {
      if (not impl) {
        boost::asio::post(executor, [cb{std::move(cb)}]() mutable {
          cb(boost::system::error_code{boost::asio::error::eof}, 0);
        });
        return;
      }
      boost::asio::mutable_buffer buffer{
          boost::asio::detail::buffer_sequence_adapter<
              boost::asio::mutable_buffer,
              MutableBufferSequence>::first(buffers)};
      impl->async_read_some(buffer, SharedFn{std::forward<Cb>(cb)});
    }

    template <typename ConstBufferSequence, typename Cb>
    void async_write_some(const ConstBufferSequence &buffers, Cb &&cb) {
      if (not impl) {
        boost::asio::post(executor, [cb{std::move(cb)}]() mutable {
          cb(boost::system::error_code{boost::asio::error::shut_down}, 0);
        });
        return;
      }
      boost::asio::const_buffer buffer{
          boost::asio::detail::buffer_sequence_adapter<
              boost::asio::const_buffer, ConstBufferSequence>::first(buffers)};
      impl->async_write_some(buffer, SharedFn{std::forward<Cb>(cb)});
    }

    void close() {
      impl.reset();
    }

    boost::asio::executor executor;
    std::shared_ptr<IAsioSocket> impl;
  };
}  // namespace libp2p

#endif  // LIBP2P_CONNECTION_ASIO_STREAM_HPP
