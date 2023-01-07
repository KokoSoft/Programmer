/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __NETWORK_HPP__
#define __NETWORK_HPP__

void network_startup();
void network_cleanup();
void network_test();
void network_test2();

#include <system_error>
#include <span>
#include <vector>

//#include <winsock.h>
#include <Winsock2.h> // WSAPool and struct poolfd / WSAPOLLFD

#include "types.hpp"

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
			if (ret == SOCKET_ERROR)
				throw SocketException("recvfrom");
			return ret;
		}

		// Enables incoming connections are to be accepted or rejected by the application, not by the protocol stack.
		void set_broadcast(bool broadcast) {
			BOOL opt = broadcast;
			setsockopt(SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
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
};



typedef struct _MIB_IPADDRTABLE MIB_IPADDRTABLE;

class InterfaceList {
	public:
		InterfaceList();
		void print();

	protected:
		typedef struct {
			DWORD address;
			DWORD mask;
		} Interface;
		std::vector<Interface> _interfaces;

	private:
		void print(const MIB_IPADDRTABLE* const pIPAddrTable);
};

//struct SOCKET_ADDRESS;
typedef struct _SOCKET_ADDRESS SOCKET_ADDRESS;
typedef struct _IP_ADAPTER_ADDRESSES_LH IP_ADAPTER_ADDRESSES;
typedef struct _IP_ADAPTER_UNICAST_ADDRESS_LH IP_ADAPTER_UNICAST_ADDRESS;

class InterfaceList2 {
public:
	InterfaceList2();
	void print();

protected:
	class UnicastAddress {
		public:
			UnicastAddress(const IP_ADAPTER_UNICAST_ADDRESS* const address);

			ULONG address;
			ULONG mask;
	};

	class Interface {
		public:
			Interface(const IP_ADAPTER_ADDRESSES* const adapter);

			std::vector<UnicastAddress> unicast;
			std::vector<ULONG> dns;
			std::vector<ULONG> gateway;
	};

	std::vector<Interface> _interfaces;

	private:
		static ULONG to_IPv4(const SOCKET_ADDRESS* const address);
		void printAddr(const SOCKET_ADDRESS* const Address, UINT8 prefix = 0xff);
		void flags(DWORD f);
		void type(DWORD f);
		void print(const IP_ADAPTER_ADDRESSES* const pAddresses);
};

#endif /* __NETWORK_HPP__ */
