// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <stdio.h>
#include <ws2tcpip.h>
//#include <arpa/inet.h>

#include <Programmer/Target.hpp>
#include <Programmer/protocol.hpp>

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
		case Protocol::OP_ERASE_WRITE: return "OP_ERASE_WRITE";
		case Protocol::OP_CHIP_ERASE: return "OP_CHIP_ERASE";
		default: return "Invalid";
	}
}

const char* Target::get_status_name(uint8_t stat) {
	switch (stat) {
		case Protocol::STATUS_REQUEST: return "STATUS_REQUEST";
		case Protocol::STATUS_OK: return "STATUS_OK";
		case Protocol::STATUS_INPROGRESS: return "STATUS_INPROGRESS";
		//case Protocol::STATUS_DONE: return "STATUS_DONE";
		case Protocol::STATUS_INV_OP: return "STATUS_INV_OP";
		case Protocol::STATUS_INV_PARAM: return "STATUS_INV_PARAM";
		case Protocol::STATUS_INV_SRC: return "STATUS_INV_SRC";
		case Protocol::STATUS_INV_ADDR: return "STATUS_INV_ADDR";
		case Protocol::STATUS_PKT_SIZE: return "STATUS_PKT_SIZE";
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
	size += sizeof(Protocol::ReplyHeader);
	hexdump(buf, size);

	const auto hdr = reinterpret_cast<const Protocol::ReplyHeader*>(buf);
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
		union {
			struct {
				Protocol::RequestHeader header;
				Protocol::Write write;
			} request;
			struct {
				Protocol::ReplyHeader header;
				union {
					Protocol::DiscoverReply dr;
					unsigned char payload[1500 - sizeof(Protocol::ReplyHeader)];
				};
			} reply;
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
		if (len < sizeof(buf.request.header)) {
			printf("Packet too short!\n");
			continue;
		}

		printf("ver: %u, seq: %u, op: %u (%s), stat: %u (%s) ",
			   buf.request.header.version, buf.request.header.seq, buf.request.header.operation,
			   get_operation_name(buf.request.header.operation), buf.request.header.status,
			get_status_name(buf.request.header.status));
		if (buf.request.header.version != Protocol::VERSION) {
			printf("Invalid version!\n");
			continue;
		}

		if (buf.request.header.status != Protocol::STATUS_REQUEST) {
			printf("Invalid status!\n");
			continue;
		}

		if ((buf.request.header.operation != Protocol::OP_DISCOVER) &&
			(buf.request.header.operation != Protocol::OP_NET_CONFIG)) {
			if (buf.request.header.seq == last_seq) {
				printf("Duplicated seq!\n");
				continue;
			}

			if (rx_addr != programmer_addr) {
				printf("Invalid sender!\n");
				buf.request.header.status = Protocol::STATUS_INV_SRC;
				send(&buf, 0, &rx_addr);
				continue;
			}
		}
		last_seq = buf.request.header.seq;

		switch (buf.request.header.operation) {
			case Protocol::OP_DISCOVER:
			case Protocol::OP_NET_CONFIG:
				programmer_addr = rx_addr;
				buf.reply.header.status = Protocol::STATUS_OK;
				buf.reply.dr.bootloader_address = 0xDEADBEEF;
				buf.reply.dr.version = 0x0100;
				buf.reply.dr.device_id = _dev_id;
				send(&buf, sizeof(buf.reply.dr), &rx_addr);
				break;

			case Protocol::OP_ERASE:
				printf("Erase 0x%06X ", buf.request.header.address.native());

				if ((buf.request.header.address % ERASE_SIZE) || (buf.request.header.address >= _flash.size())) {
					buf.request.header.status = Protocol::STATUS_INV_PARAM;
					send(&buf, 0, &rx_addr);
					continue;
				}

				buf.reply.header.status = Protocol::STATUS_INPROGRESS;
				send(&buf, 0, &rx_addr);
				memset(_flash.data() + buf.request.header.address, 0xff, ERASE_SIZE);
				buf.reply.header.status = Protocol::STATUS_OK;
				send(&buf, 0, &rx_addr);
				break;

			case Protocol::OP_WRITE:
				printf("Write to 0x%06X ", buf.request.header.address.native());

				if ((buf.request.header.address % WRITE_SIZE) || (buf.request.header.address >= _flash.size())) {
					buf.reply.header.status = Protocol::STATUS_INV_PARAM;
					send(&buf, 0, &rx_addr);
					continue;
				}

				buf.reply.header.status = Protocol::STATUS_INPROGRESS;
				send(&buf, 0, &rx_addr);
				memcpy(_flash.data() + buf.request.header.address, buf.request.write.data, WRITE_SIZE);
				buf.reply.header.status = Protocol::STATUS_OK;
				send(&buf, 0, &rx_addr);
				break;

			case Protocol::OP_READ:
			{
				uint32_t addr = buf.request.header.address;
				uint32_t length = buf.request.header.length;
				printf("Read %u from 0x%06X ", length, addr);
				if (((addr + length) > _flash.size()) ||
					(length > sizeof(buf.reply.payload))) {
					buf.reply.header.status = Protocol::STATUS_INV_PARAM;
					send(&buf, 0, &rx_addr);
					continue;
				}

				buf.reply.header.status = Protocol::STATUS_INPROGRESS;
				send(&buf, 0, &rx_addr);
				memcpy(buf.reply.payload, _flash.data() + addr, length);
				buf.reply.header.status = Protocol::STATUS_OK;
				send(&buf, length, &rx_addr);
				break;
			}

			default:
				printf("Unsupported operation!");
				buf.reply.header.status = Protocol::STATUS_INV_OP;
				send(&buf, 0, &rx_addr);
		}

		printf("\n");
	}
}
