/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBP2P_TRANSPORT_TCP_ADDRESS_HPP
#define LIBP2P_TRANSPORT_TCP_ADDRESS_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/lexical_cast.hpp>
#include <libp2p/multi/multiaddress.hpp>

namespace libp2p::transport {
  // TODO(xDimon): Support 'DNSADDR' addresses.
  // Issue: https://github.com/libp2p/cpp-libp2p/issues/97
  struct TcpMultiaddress {
    using Endpoint = boost::asio::ip::tcp::endpoint;

    static boost::optional<TcpMultiaddress> from(
        const multi::Multiaddress &address) {
      using P = multi::Protocol::Code;
      auto v = address.getProtocolsWithValues();
      auto it = v.begin();
      if (it == v.end()) {
        return boost::none;
      }
      switch (it->first.code) {
        case P::IP4:
        case P::IP6:
        case P::DNS4:
        case P::DNS6:
        case P::DNS:
          break;
        default:
          return boost::none;
      }
      TcpMultiaddress tcp;
      tcp.host_type = it->first.code;
      tcp.host = it->second;
      ++it;
      if (it == v.end()) {
        return boost::none;
      }
      if (it->first.code != P::TCP) {
        return boost::none;
      }
      try {
        tcp.port = boost::lexical_cast<uint16_t>(it->second);
      } catch (const boost::bad_lexical_cast &) {
        return boost::none;
      }
      ++it;
      if (it == v.end()) {
        return tcp;
      }
      if (it->first.code == P::WS) {
        tcp.ws = true;
        ++it;
      } else if (it->first.code == P::WSS) {
        tcp.wss = true;
        ++it;
      }
      if (it == v.end()) {
        return tcp;
      }
      if (it->first.code != P::P2P) {
        return boost::none;
      }
      return tcp;
    }

    outcome::result<Endpoint> endpoint() const {
      using P = multi::Protocol::Code;
      if (host_type != P::IP4 and host_type != P::IP6) {
        return std::errc::address_family_not_supported;
      }
      boost::system::error_code ec;
      auto ip = boost::asio::ip::make_address(host, ec);
      if (ec) {
        return ec;
      }
      return Endpoint{ip, port};
    }

    multi::Multiaddress multiaddress(const Endpoint &endpoint) const {
      auto ip = endpoint.address();
      // TODO(warchant): PRE-181 refactor to use builder instead
      std::string s;
      if (ip.is_v4()) {
        s += "/ip4/";
      } else {
        s += "/ip6/";
      }
      s += ip.to_string();
      s += "/tcp/";
      s += std::to_string(endpoint.port());
      if (ws) {
        s += "/ws";
      } else if (wss) {
        s += "/wss";
      }
      return multi::Multiaddress::create(s).value();
    }

    multi::Protocol::Code host_type;
    std::string host;
    uint16_t port;
    bool ws = false;
    bool wss = false;
  };
}  // namespace libp2p::transport

#endif  // LIBP2P_TRANSPORT_TCP_ADDRESS_HPP
