/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __NETWORK_HPP__
#define __NETWORK_HPP__

#include <system_error>
#include <span>

#include <Winsock2.h> 
#include <Ws2tcpip.h>

#include <Programmer/types.hpp>

namespace programmer {

class SocketException : public std::system_error {
	public:
		SocketException(const char*) : std::system_error(WSAGetLastError(), std::system_category()) {}
		SocketException() : std::system_error(WSAGetLastError(), std::system_category()) {}
};

class Socket {
	public:
		Socket() : _handle(INVALID_SOCKET) {}

		Socket(int af, int type, int protocol) : _handle(::socket(af, type, protocol))
		{
			if (_handle == INVALID_SOCKET)
				throw SocketException("socket");
		}

		void setsockopt(int level, int optname, const void* optval, int optlen) {
			int ret = ::setsockopt(_handle, level, optname, reinterpret_cast<const char*>(optval), optlen);
			if (ret == SOCKET_ERROR)
				throw SocketException("setsockopt");
		}

		void set_nonblocking(bool nonblock) {
			u_long mode = nonblock;
			int result = ioctlsocket(_handle, FIONBIO, &mode);
			if (result != NO_ERROR)
				throw SocketException("ioctlsocket");
		}

		void bind(const void* addr, int addr_len) {
			int ret = ::bind(_handle, reinterpret_cast<const sockaddr*>(addr), addr_len);
			if (ret == SOCKET_ERROR)
				throw SocketException("bind");
		}
		
		~Socket() {
			if (_handle != INVALID_SOCKET)
				::closesocket(_handle);
		}

		operator SOCKET() const { return _handle; }

	protected:
		const SOCKET _handle;
};

class SocketUDP : public Socket {
	public:
		SocketUDP() : Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) {};

		int sendto(const std::span<const std::byte>& buf, int flags, const void* addr, int addr_len) {
			int ret = ::sendto(_handle, reinterpret_cast<const char*>(buf.data()), buf.size_bytes(),
							   flags, reinterpret_cast<const sockaddr*>(addr), addr_len);
			if (ret == SOCKET_ERROR)
				throw SocketException("sendto");

			if (ret != buf.size_bytes())
				throw Exception("Truncated sendto");

			return ret;
		}
		/*
		int recvfrom(std::span<std::byte>& buf, int flags, void* addr, int* addr_len) {
			int ret = ::recvfrom(_handle, reinterpret_cast<char*>(buf.data()), buf.size_bytes(),
								 flags, reinterpret_cast<sockaddr*>(addr), addr_len);
			if (ret == SOCKET_ERROR)
				throw SocketException("recvfrom");
			return ret;
		}
		*/
		int recvfrom(std::span<std::byte> buf, int flags, void* addr, int* addr_len) {
			int ret = ::recvfrom(_handle, reinterpret_cast<char*>(buf.data()), buf.size_bytes(),
								 flags, reinterpret_cast<sockaddr*>(addr), addr_len);
			if (ret == SOCKET_ERROR) {
				ret = WSAGetLastError();
				if (ret != WSAEWOULDBLOCK)
					throw SocketException("recvfrom");
				return -1;
			}
			return ret;
		}

		// Enables incoming connections are to be accepted or rejected by the application, not by the protocol stack.
		void set_broadcast(bool broadcast) {
			BOOL opt = broadcast;
			setsockopt(SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
		}

		// Indicates that data should not be fragmented regardless of the local MTU.
		void set_dont_fragment(bool dont_fragment) {
			DWORD opt = dont_fragment;
			setsockopt(IPPROTO_IP, IP_DONTFRAGMENT, &opt, sizeof(opt));
		}

		// Allows or blocks broadcast reception.
		void receive_broadcast(bool allow_broadcast) {
			DWORD opt = allow_broadcast;
			setsockopt(IPPROTO_IP, IP_RECEIVE_BROADCAST, &opt, sizeof(opt));
		}

		void bind(const sockaddr_in* addr) {
			Socket::bind(addr, sizeof(*addr));
		}
};

class Network {
	public:
		/* On Windows poll can only operate on sockets.
		 * On linux it can operate on any file descriptor.
		 */
		static int poll(std::span<struct pollfd> fds, int timeout) {
			int ret = ::WSAPoll(fds.data(), fds.size(), timeout);
			if (ret == SOCKET_ERROR)
				throw SocketException("WSAPoll");

#ifdef _DEBUG
			for (const struct pollfd& fd : fds)
				if (fd.revents & POLLNVAL)
					throw Exception("Invalid descriptor passed to poll.");
#endif // _DEBUG

			return ret;
		}

		static void startup();
		static void cleanup();

		template <typename T>
		static constexpr T big_endian(T val) noexcept {
			if constexpr (std::endian::native == std::endian::little)
				return std::byteswap(val);
			else
				return val;
		}

		template <typename T>
		struct convert_endian {
			constexpr T operator()(T val) const noexcept {
				if constexpr (std::endian::native == std::endian::little)
					return std::byteswap(val);
				else
					return val;
			}
		};

		using htons = convert_endian<uint16_t>;
		using ntohs = convert_endian<uint16_t>;
		using htonl = convert_endian<uint32_t>;
		using ntohl = convert_endian<uint32_t>;
};

} // namespace programmer

#endif /* __NETWORK_HPP__ */
