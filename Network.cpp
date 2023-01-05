// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <stdio.h>
#include <exception>
#include <memory>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <Iphlpapi.h>

#include "types.hpp"
#include "Network.hpp"

void network_startup() {
	WSADATA wsa_data;
	std::memset(&wsa_data, 0, sizeof(wsa_data));

	int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (res != NO_ERROR) {
		WSACleanup();
		throw SocketException();// Exception("WSAStartup failed with error %d.");
	}
}

void network_cleanup() {
	WSACleanup();
}

#include <ws2tcpip.h>

void network_test() {
	InterfaceList list;
	InterfaceList2 list2;

	SocketUDP sock;
	//sock.set_broadcast(false);
	sock.set_broadcast(true);
	//std::array<std::byte, 64> ;
	std::byte t[24];
	std::byte tt[240];
	sockaddr addr;
	struct sockaddr_in RecvAddr;
	std::memset(&RecvAddr, 0, sizeof(RecvAddr));
	RecvAddr.sin_family = AF_INET;
	RecvAddr.sin_port = htons(666);

	RecvAddr.sin_addr.S_un.S_addr = inet_addr("192.168.128.255");
	addr.sa_family = AF_INET;
	sock.sendto(t, 0, (const sockaddr *)&RecvAddr, sizeof(RecvAddr));

	//RecvAddr.sin_addr.S_un.S_addr = inet_addr("192.168.56.255");
	//sock.sendto(t, 0, (const sockaddr*)&RecvAddr, sizeof(RecvAddr));

	RecvAddr.sin_addr.S_un.S_addr = inet_addr("255.255.255.255");
	sock.sendto(t, 0, (const sockaddr*)&RecvAddr, sizeof(RecvAddr));

	char ip[INET_ADDRSTRLEN] = {};
	int Sender_addr_len = sizeof(RecvAddr);
	std::span<std::byte> span(tt);
	int i;

	for (;;) {
		i = sock.recvfrom(span, 0, (sockaddr*)&RecvAddr, &Sender_addr_len);
		inet_ntop(AF_INET, &RecvAddr.sin_addr, ip, sizeof(ip));
		printf("Received %d from %s:%u\n", i, ip, ntohs(RecvAddr.sin_port));
	}
}

#include <array>

void network_test2() {
	SocketUDP sock;

	//     This option is needed on the socket in order to be able to receive broadcast messages
	//   If not set the receiver will not receive broadcast messages in the local network.
	//sock.set_broadcast(true);

	struct sockaddr_in Recv_addr;
	struct sockaddr_in Sender_addr;
	int Sender_addr_len = sizeof(Sender_addr);

	int len = sizeof(struct sockaddr_in);

	std::array<std::byte, 512> recvbuff;
	int recvbufflen = 50;

	char sendMSG[] = "Broadcast message from READER";

	Recv_addr.sin_family = AF_INET;
	Recv_addr.sin_port = htons(666);
	Recv_addr.sin_addr.s_addr = INADDR_ANY;
	sock.bind(&Recv_addr);

	char addr[INET_ADDRSTRLEN] = {};

	for (;;) {
		std::span<std::byte> span(recvbuff);
		int i = sock.recvfrom(span, 0, (sockaddr*)&Sender_addr, &Sender_addr_len);
		inet_ntop(AF_INET, &Sender_addr.sin_addr, addr, sizeof(addr));
		printf("Received %d from %s:%u\n", i, addr, ntohs(Sender_addr.sin_port));
		Sleep(250);
		std::span<std::byte> span2(&recvbuff[0], i);
		sock.sendto(span2, 0, (const sockaddr*)&Sender_addr, sizeof(Sender_addr));
		//printf("Received Message is : %s\n", recvbuff);
	}
}

InterfaceList::InterfaceList() {
	ULONG size = sizeof(MIB_IPADDRTABLE) * 3;
	std::unique_ptr<std::byte[]> buf;
	unsigned int retries = 0;
	DWORD ret;

	do {
		if (retries++ > 5)
			throw Exception("Unable to GetIpAddrTable");

		buf = std::make_unique<std::byte[]>(size);
		ret = GetIpAddrTable(reinterpret_cast<PMIB_IPADDRTABLE>(buf.get()), &size, false);
	} while (ret == ERROR_INSUFFICIENT_BUFFER);

	if (ret != NO_ERROR)
		throw std::system_error(ret, std::system_category());

	const MIB_IPADDRTABLE* const addr_table = reinterpret_cast<MIB_IPADDRTABLE*>(buf.get());
	_interfaces.reserve(addr_table->dwNumEntries);

	for (DWORD i = 0; i < addr_table->dwNumEntries; i++) {
		Interface& entry = _interfaces.emplace_back();
		entry.address = addr_table->table[i].dwAddr;
		entry.mask = addr_table->table[i].dwMask;
	}

	print(addr_table);
}

void InterfaceList::print(const MIB_IPADDRTABLE* const pIPAddrTable) {
	IN_ADDR IPAddr;
	printf("\tNum Entries: %ld\n", pIPAddrTable->dwNumEntries);
	for (int i = 0; i < (int)pIPAddrTable->dwNumEntries; i++) {
		printf("\n\tInterface Index[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwIndex);
		IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwAddr;
		printf("\tIP Address[%d]:     \t%s\n", i, inet_ntoa(IPAddr));
		IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwMask;
		printf("\tSubnet Mask[%d]:    \t%s\n", i, inet_ntoa(IPAddr));
		IPAddr.S_un.S_addr = (u_long)pIPAddrTable->table[i].dwBCastAddr;
		printf("\tBroadCast[%d]:      \t%s (%lX)\n", i, inet_ntoa(IPAddr), pIPAddrTable->table[i].dwBCastAddr);
		printf("\tReassembly size[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwReasmSize);
		printf("\tType and State[%d]:", i);
		if (pIPAddrTable->table[i].wType & MIB_IPADDR_PRIMARY)
			printf("\tPrimary IP Address");
		if (pIPAddrTable->table[i].wType & MIB_IPADDR_DYNAMIC)
			printf("\tDynamic IP Address");
		if (pIPAddrTable->table[i].wType & MIB_IPADDR_DISCONNECTED)
			printf("\tAddress is on disconnected interface");
		if (pIPAddrTable->table[i].wType & MIB_IPADDR_DELETED)
			printf("\tAddress is being deleted");
		if (pIPAddrTable->table[i].wType & MIB_IPADDR_TRANSIENT)
			printf("\tTransient address");
		printf("\n");
	}
}

void InterfaceList::print() {

}

#include <ws2tcpip.h>

void InterfaceList2::printAddr(const SOCKET_ADDRESS* const Address, UINT8 prefix)
{
	//printf("  Length of sockaddr: %d\n", Address->iSockaddrLength);
	if (Address->lpSockaddr->sa_family == AF_INET)
	{
		const sockaddr_in* si = (sockaddr_in*)(Address->lpSockaddr);
		char a[INET_ADDRSTRLEN] = {};
		if (inet_ntop(AF_INET, &(si->sin_addr), a, sizeof(a)))
			printf("\tIPv4 address: %s", a);
	}
	else if (Address->lpSockaddr->sa_family == AF_INET6)
	{
		sockaddr_in6* si = (sockaddr_in6*)(Address->lpSockaddr);
		char a[INET6_ADDRSTRLEN] = {};
		if (inet_ntop(AF_INET6, &(si->sin6_addr), a, sizeof(a)))
			printf("\tIPv6 address: %s", a);
	}

	printf(prefix != 0xff ? " / %u\n" : "\n", prefix);
}

void InterfaceList2::flags(DWORD f) {
#define X(x) if (f & x) printf(#x " ")
X(IP_ADAPTER_DDNS_ENABLED);
X(IP_ADAPTER_REGISTER_ADAPTER_SUFFIX);
X(IP_ADAPTER_DHCP_ENABLED);
X(IP_ADAPTER_RECEIVE_ONLY);
X(IP_ADAPTER_NO_MULTICAST);
X(IP_ADAPTER_IPV6_OTHER_STATEFUL_CONFIG);
X(IP_ADAPTER_NETBIOS_OVER_TCPIP_ENABLED);
X(IP_ADAPTER_IPV4_ENABLED);
X(IP_ADAPTER_IPV6_ENABLED);
X(IP_ADAPTER_IPV6_MANAGE_ADDRESS_CONFIG);
#undef X
printf("\n");
}

void InterfaceList2::type(DWORD f) {
#define X(x) if (f == x) printf(" " #x "\n");
	X(IF_TYPE_OTHER);
	X(IF_TYPE_ETHERNET_CSMACD);
	X(IF_TYPE_ISO88025_TOKENRING);
	X(IF_TYPE_PPP);
	X(IF_TYPE_SOFTWARE_LOOPBACK);
	X(IF_TYPE_ATM);
	X(IF_TYPE_IEEE80211);
	X(IF_TYPE_TUNNEL);
	X(IF_TYPE_IEEE1394);
#undef X
}

InterfaceList2::InterfaceList2() {
	ULONG size = 15 * 1024;
	std::unique_ptr<std::byte[]> buf;
	unsigned int retries = 0;
	DWORD ret;

	do {
		if (retries++ > 5)
			throw Exception("Unable to GetAdaptersAddresses");

		buf = std::make_unique<std::byte[]>(size);
		ret = GetAdaptersAddresses(AF_INET,
								   GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_GATEWAYS,
								   nullptr, reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.get()), &size);
	} while (ret == ERROR_BUFFER_OVERFLOW);

	if (ret != NO_ERROR)
		throw std::system_error(ret, std::system_category());

	const IP_ADAPTER_ADDRESSES* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());
	for (; adapter; adapter = adapter->Next) {
		if (adapter->Length != sizeof(IP_ADAPTER_ADDRESSES))
			throw Exception("Unsupported IP_ADAPTER_ADDRESSES version.");

		// Use only active interfaces with enabled IPv4, skip local loopback
		if ((adapter->OperStatus != IfOperStatusUp) || (!adapter->Ipv4Enabled) ||
			(adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK))
			continue;

		Interface& entry = _interfaces.emplace_back(adapter);
		printf("\tDescription: %wS\n", adapter->Description);
		printf("\tFriendly name: %wS\n\n", adapter->FriendlyName);
	}

	//print(reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get()));
}

InterfaceList2::Interface::Interface(const IP_ADAPTER_ADDRESSES* const adapter) {
		char a[INET_ADDRSTRLEN] = {};
		ULONG tmp;

		const IP_ADAPTER_UNICAST_ADDRESS* unicast_addr = adapter->FirstUnicastAddress;
		while (unicast_addr) {
			unicast.emplace_back(unicast_addr);
			unicast_addr = unicast_addr->Next;
		}

		const IP_ADAPTER_DNS_SERVER_ADDRESS* dns_server = adapter->FirstDnsServerAddress;
		while (dns_server) {
			tmp = dns.emplace_back(to_IPv4(&dns_server->Address));

			if (inet_ntop(AF_INET, &tmp, a, sizeof(a)))
				printf("\tDNS address: %s\n", a);

			dns_server = dns_server->Next;
		}

		const IP_ADAPTER_GATEWAY_ADDRESS* gateway_addr = adapter->FirstGatewayAddress;
		while (gateway_addr) {
			tmp = gateway.emplace_back(to_IPv4(&gateway_addr->Address));

			if (inet_ntop(AF_INET, &tmp, a, sizeof(a)))
				printf("\tGateway address: %s\n", a);

			gateway_addr = gateway_addr->Next;
		}
}

InterfaceList2::UnicastAddress::UnicastAddress(const IP_ADAPTER_UNICAST_ADDRESS* const address) {
	this->address = InterfaceList2::to_IPv4(&address->Address);
	ConvertLengthToIpv4Mask(address->OnLinkPrefixLength, &mask);

	char a[INET_ADDRSTRLEN] = {};
	if (inet_ntop(AF_INET, &(this->address), a, sizeof(a)))
		printf("\tIPv4 address: %s / %u\n", a, address->OnLinkPrefixLength);
}

ULONG InterfaceList2::to_IPv4(const SOCKET_ADDRESS* const address) {
	const sockaddr_in* const addr = reinterpret_cast<const sockaddr_in*>(address->lpSockaddr);

	if (addr->sin_family != AF_INET)
		throw Exception("Unsupported address family.");

	return addr->sin_addr.S_un.S_addr;
}

void InterfaceList2::print(const IP_ADAPTER_ADDRESSES* const pAddresses) {
	LPVOID lpMsgBuf = NULL;

	const IP_ADAPTER_ADDRESSES* pCurrAddresses = NULL;
	ULONG outBufLen = 0;
	ULONG Iterations = 0;

	PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
	PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
	PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
	IP_ADAPTER_DNS_SERVER_ADDRESS* pDnServer = NULL;
	IP_ADAPTER_PREFIX* pPrefix = NULL;
	IP_ADAPTER_GATEWAY_ADDRESS* pGateway = NULL;
	unsigned int i = 0;

	// If successful, output some information from the data we received
	pCurrAddresses = pAddresses;
	while (pCurrAddresses) {
		printf("\tLength of the IP_ADAPTER_ADDRESS struct: %ld\n",
			pCurrAddresses->Length);
		printf("\tIfIndex (IPv4 interface): %u\n", pCurrAddresses->IfIndex);
		printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);

		pUnicast = pCurrAddresses->FirstUnicastAddress;
		if (pUnicast != NULL) {
			for (i = 0; pUnicast != NULL; i++) {
				printAddr(&pUnicast->Address, pUnicast->OnLinkPrefixLength);
				pUnicast = pUnicast->Next;
			}
			printf("\tNumber of Unicast Addresses: %d\n", i);
		}
		else
			printf("\tNo Unicast Addresses\n");

		pAnycast = pCurrAddresses->FirstAnycastAddress;
		if (pAnycast) {
			for (i = 0; pAnycast != NULL; i++) {
				printAddr(&pAnycast->Address);
				pAnycast = pAnycast->Next;
			}
			printf("\tNumber of Anycast Addresses: %d\n", i);
		}
		else
			printf("\tNo Anycast Addresses\n");

		pMulticast = pCurrAddresses->FirstMulticastAddress;
		if (pMulticast) {
			for (i = 0; pMulticast != NULL; i++) {
				printAddr(&pMulticast->Address);
				pMulticast = pMulticast->Next;
			}
			printf("\tNumber of Multicast Addresses: %d\n", i);
		}
		else
			printf("\tNo Multicast Addresses\n");

		pDnServer = pCurrAddresses->FirstDnsServerAddress;
		if (pDnServer) {
			for (i = 0; pDnServer != NULL; i++) {
				printAddr(&pDnServer->Address);
				pDnServer = pDnServer->Next;
			}
			printf("\tNumber of DNS Server Addresses: %d\n", i);
		}
		else
			printf("\tNo DNS Server Addresses\n");

		pGateway = pCurrAddresses->FirstGatewayAddress;
		if (pGateway) {
			for (i = 0; pGateway != NULL; i++) {
				printAddr(&pGateway->Address);
				pGateway = pGateway->Next;
			}
			printf("\tNumber of Gateway Addresses: %d\n", i);
		}
		else
			printf("\tNo Gateway Addresses\n");

		printf("\tDNS Suffix: %wS\n", pCurrAddresses->DnsSuffix);
		printf("\tDescription: %wS\n", pCurrAddresses->Description);
		printf("\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);

		if (pCurrAddresses->PhysicalAddressLength != 0) {
			printf("\tPhysical address: ");
			for (i = 0; i < (int)pCurrAddresses->PhysicalAddressLength;
				i++) {
				if (i == (pCurrAddresses->PhysicalAddressLength - 1))
					printf("%.2X\n",
						(int)pCurrAddresses->PhysicalAddress[i]);
				else
					printf("%.2X-",
						(int)pCurrAddresses->PhysicalAddress[i]);
			}
		}
		printf("\tFlags: %ld", pCurrAddresses->Flags);
		flags(pCurrAddresses->Flags);
		printf("\tMtu: %lu\n", pCurrAddresses->Mtu);
		printf("\tIfType: %ld", pCurrAddresses->IfType);
		type(pCurrAddresses->IfType);
		printf("\tOperStatus: %ld\n", pCurrAddresses->OperStatus);

		printf("\tIpv4Metric: %u\n", pCurrAddresses->Ipv4Metric);
		/*printf("\tIpv6IfIndex (IPv6 interface): %u\n", pCurrAddresses->Ipv6IfIndex);
		printf("\tZoneIndices (hex): ");
		for (i = 0; i < 16; i++)
			printf("%lx ", pCurrAddresses->ZoneIndices[i]);
		printf("\n");
		*/
		printf("\tTransmit link speed: %I64u\n", pCurrAddresses->TransmitLinkSpeed);
		printf("\tReceive link speed: %I64u\n", pCurrAddresses->ReceiveLinkSpeed);

		pPrefix = pCurrAddresses->FirstPrefix;
		if (pPrefix) {
			for (i = 0; pPrefix != NULL; i++) {
				printAddr(&pPrefix->Address);
				pPrefix = pPrefix->Next;
			}
			printf("\tNumber of IP Adapter Prefix entries: %d\n", i);
		}
		else
			printf("\tNo IP Adapter Prefix\n");

		printf("-----------------------------------------------------------\n");

		pCurrAddresses = pCurrAddresses->Next;
	}
}