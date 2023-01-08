// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <stdio.h>
#include <ws2tcpip.h>
//#include <arpa/inet.h>

#include "Target.hpp"
#include "protocol.hpp"

Target::Target(uint16_t dev_id, size_t flash_size)
	: _dev_id(dev_id)
{
	_flash.resize(flash_size * 1024);
}

const char* Target::get_operation_name(uint8_t op) {
	switch (op) {
		case Protocol::OP_CHECKSUM: return "OP_CHECKSUM";
		case Protocol::OP_DISCOVER: return "OP_DISCOVER";
		case Protocol::OP_ERASE: return "OP_ERASE";
		case Protocol::OP_NET_CONFIG: return "OP_NET_CONFIG";
		case Protocol::OP_READ: return "OP_READ";
		case Protocol::OP_RESET: return "OP_RESET";
		case Protocol::OP_WRITE: return "OP_WRITE";
		default: return "Invalid";
	}
}

const char* Target::get_status_name(uint8_t stat) {
	switch (stat) {
		case Protocol::STATUS_REQUEST: return "STATUS_REQUEST";
		case Protocol::STATUS_OK: return "STATUS_OK";
		case Protocol::STATUS_INPROGRESS: return "STATUS_INPROGRESS";
		case Protocol::STATUS_DONE: return "STATUS_DONE";
		case Protocol::STATUS_INV_OP: return "STATUS_INV_OP";
		case Protocol::STATUS_INV_PARAM: return "STATUS_INV_PARAM";
		case Protocol::STATUS_INV_SRC: return "STATUS_INV_SRC";
		default: return "Invalid";
	}
}

void Target::hexdump(const void* buf, size_t len) {
	const unsigned char* b = reinterpret_cast<const unsigned char*>(buf);
	const unsigned char* end = b + len;
	while (b < end)
		printf("%02X ", *b++);

	printf("\n");
}

constexpr bool operator ==(const sockaddr_in& a, const sockaddr_in& b) noexcept {
	return (a.sin_family == b.sin_family) && (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

void Target::send(const void* buf, size_t size, const sockaddr_in* addr) {
	printf("\n");
	size += sizeof(Protocol::Header);
	hexdump(buf, size);

	const auto hdr = reinterpret_cast<const Protocol::Header*>(buf);
	printf("Tx %zu bytes: ver: %u, seq: %u, op: %u (%s), stat: %u (%s)", size,
		hdr->version, hdr->seq, hdr->operation, get_operation_name(hdr->operation), hdr->status, get_status_name(hdr->status));


	_socket.sendto(std::span<const std::byte>(reinterpret_cast<const std::byte*>(buf), size), 0, addr, sizeof(*addr));
}

void Target::start() {
	sockaddr_in rx_addr;
	sockaddr_in programmer_addr;
	int rx_addr_size;
	char addr[INET_ADDRSTRLEN] = {0xCD};
	uint8_t last_seq = 0;

	union {
		std::byte raw[1500];
		struct {
			Protocol::Header header;
			union {
				Protocol::DiscoverReply dr;
				Protocol::Erase erase;
				Protocol::Write write;
				Protocol::Read read;
				unsigned char payload[1500 - sizeof(Protocol::Header)];
			};
		};
	} buf;

	rx_addr.sin_family = AF_INET;
	rx_addr.sin_port = Network::htons()(Protocol::PORT);
	rx_addr.sin_addr.s_addr = INADDR_ANY;
	_socket.bind(&rx_addr);

	while (1) {
		rx_addr_size = sizeof(rx_addr);
		int len = _socket.recvfrom(buf.raw, 0, &rx_addr, &rx_addr_size);
		hexdump(buf.raw, len);
		inet_ntop(rx_addr.sin_family, &rx_addr.sin_addr, addr, sizeof(addr));
		printf("Rx: %s:%d %d bytes ", addr, Network::ntohs()(rx_addr.sin_port), len);
		if (len < sizeof(buf.header)) {
			printf("Packet too short!\n");
			continue;
		}

		printf("ver: %u, seq: %u, op: %u (%s), stat: %u (%s) ",
			   buf.header.version, buf.header.seq, buf.header.operation, get_operation_name(buf.header.operation),
			   buf.header.status, get_status_name(buf.header.status));
		if (buf.header.version != Protocol::VERSION) {
			printf("Invalid version!\n");
			continue;
		}

		if (buf.header.status != Protocol::STATUS_REQUEST) {
			printf("Invalid status!\n");
			continue;
		}

		if ((buf.header.operation != Protocol::OP_DISCOVER) &&
			(buf.header.operation != Protocol::OP_NET_CONFIG)) {
			if (buf.header.seq == last_seq) {
				printf("Duplicated seq!\n");
				continue;
			}

			if (rx_addr != programmer_addr) {
				printf("Invalid sender!\n");
				buf.header.status = Protocol::STATUS_INV_SRC;
				send(&buf, 0, &rx_addr);
				continue;
			}
		}
		last_seq = buf.header.seq;

		switch (buf.header.operation) {
			case Protocol::OP_DISCOVER:
			case Protocol::OP_NET_CONFIG:
				programmer_addr = rx_addr;
				buf.header.status = Protocol::STATUS_OK;
				buf.dr.bootloader_address = 0xDEADBEEF;
				buf.dr.version = 0x0100;
				buf.dr.device_id = _dev_id;
				send(&buf, sizeof(buf.dr), &rx_addr);
				break;

			case Protocol::OP_ERASE:
				printf("Erase 0x%06X ", buf.erase.address.native());

				if ((buf.erase.address % ERASE_SIZE) || (buf.erase.address >= _flash.size())) {
					buf.header.status = Protocol::STATUS_INV_PARAM;
					send(&buf, 0, &rx_addr);
					continue;
				}

				buf.header.status = Protocol::STATUS_INPROGRESS;
				send(&buf, 0, &rx_addr);
				memset(_flash.data() + buf.erase.address, 0xff, ERASE_SIZE);
				buf.header.status = Protocol::STATUS_DONE;
				send(&buf, 0, &rx_addr);
				break;

			case Protocol::OP_WRITE:
				printf("Write to 0x%06X ", buf.write.address.native());

				if ((buf.write.address % WRITE_SIZE) || (buf.write.address >= _flash.size())) {
					buf.header.status = Protocol::STATUS_INV_PARAM;
					send(&buf, 0, &rx_addr);
					continue;
				}

				buf.header.status = Protocol::STATUS_INPROGRESS;
				send(&buf, 0, &rx_addr);
				memcpy(_flash.data() + buf.write.address, buf.write.data, WRITE_SIZE);
				buf.header.status = Protocol::STATUS_DONE;
				send(&buf, 0, &rx_addr);
				break;

			case Protocol::OP_READ:
			{
				uint32_t addr = buf.read.address;
				uint32_t length = buf.read.length;
				printf("Read %u from 0x%06X ", length, addr);
				if (((addr + length) > _flash.size()) ||
					(length > sizeof(buf.payload))) {
					buf.header.status = Protocol::STATUS_INV_PARAM;
					send(&buf, 0, &rx_addr);
					continue;
				}

				buf.header.status = Protocol::STATUS_INPROGRESS;
				send(&buf, 0, &rx_addr);
				memcpy(buf.payload, _flash.data() + addr, length);
				buf.header.status = Protocol::STATUS_DONE;
				send(&buf, length, &rx_addr);
				break;
			}

			default:
				printf("Unsupported operation!");
				buf.header.status = Protocol::STATUS_INV_OP;
				send(&buf, 0, &rx_addr);
		}

		printf("\n");
	}
}
