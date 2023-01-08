// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <chrono>
#include <array>
#include <cassert>

#include "Programmer.hpp"

NetworkProgrammer::NetworkProgrammer()
	: _poll{ { _socket, POLLIN} },
	_tx_address{ AF_INET, Network::htons()(Protocol::PORT) }
{ }

void NetworkProgrammer::set_address(uint32_t address, uint16_t port) {
	_tx_address.sin_addr.s_addr = address;
	_tx_address.sin_port = Network::htons()(port);

	if (address == INADDR_BROADCAST)
		_socket.set_broadcast(true);
}

// Send frame and wait for reply
void NetworkProgrammer::communicate() {
	using std::chrono::steady_clock;
	using std::chrono::milliseconds;
	using std::chrono::duration_cast;
	using std::chrono::duration;

	for (int i = 0; i < 3; i++) {
		_socket.sendto(_tx_buf.data(), 0, &_tx_address, sizeof(_tx_address));

		auto now = steady_clock::now();
		auto deadline = now + milliseconds(TIMEOUT);
		for (; deadline > now; now = steady_clock::now()) {
			int timeout = duration_cast<duration<int, std::milli>>(deadline - now).count();
			int ret = Network::poll(_poll, timeout);

			if (ret && (_poll[0].revents & POLLIN)) {
				auto status = process();
				switch (status) {
					case Result::Ignore:
					case Result::ExtendTime:
						// Write operation needs 2.8ms to complete. Extending a timeout seems not necessery.
						break;
					case Result::Done:
						return;
				}
			}
		}
	}

	throw Exception("The target did not respond within the specified time");
}

// Process received frame
NetworkProgrammer::Result NetworkProgrammer::process() {
	int rx_address_size = sizeof(_rx_address);

	const int size = _socket.recvfrom(_rx_buf, 0, &_rx_address, &rx_address_size);
	if (size < sizeof(Protocol::Header))
		throw Exception("A truncated frame was received.");

	_rx_buf.set_content_length(size);

	if (_rx_buf.get_version() != Protocol::VERSION)
		throw Exception("Unsupported protocol version.");

	if (_rx_buf.get_sequence() != _tx_buf.get_sequence())
		return Result::Ignore;

	const auto operation = _rx_buf.get_operation();
	if (operation != _tx_buf.get_operation())
		throw Exception("Invalid operation code in response.");

	switch (_rx_buf.get_status()) {
		case Protocol::STATUS_OK:
			if ((operation != Protocol::OP_DISCOVER) &&
				(operation != Protocol::OP_NET_CONFIG) &&
				(operation != Protocol::OP_RESET))
				throw Exception("Received unexcepted status from target.");
			
			return Result::Done;

		case Protocol::STATUS_DONE:
			if ((operation != Protocol::OP_READ) &&
				(operation != Protocol::OP_WRITE) &&
				(operation != Protocol::OP_ERASE) &&
				(operation != Protocol::OP_CHECKSUM))
				throw Exception("Received unexcepted status from target.");

			return Result::Done;

		case Protocol::STATUS_INPROGRESS:
			if ((operation != Protocol::OP_READ) &&
				(operation != Protocol::OP_WRITE) &&
				(operation != Protocol::OP_ERASE) &&
				(operation != Protocol::OP_CHECKSUM))
				throw Exception("Received unexcepted status from target.");

			return Result::ExtendTime;

		case Protocol::STATUS_INV_OP:
			throw Exception("Operation not supported by the target.");

		case Protocol::STATUS_INV_PARAM:
			throw Exception("The target detected an invalid parameter.");

		case Protocol::STATUS_INV_SRC:
			throw Exception("The target did not allow this operation.");

		case Protocol::STATUS_REQUEST:
		default:
			throw Exception("Target reported an invalid status.");
	}
}

#include <ws2tcpip.h>

// Process DiscoverReply from target
void NetworkProgrammer::discover(Protocol::Operation op) {
	try {
		communicate();
	}
	catch (...) {
		_socket.set_broadcast(false);
		throw;
	}
	_tx_address = _rx_address;
	_socket.set_broadcast(false);
	//_socket.bind(&_tx_address); Winsock error

	char addr[INET_ADDRSTRLEN] = {};
	inet_ntop(_rx_address.sin_family, &_rx_address.sin_addr, addr, sizeof(addr));
	printf("Detected target @ %s:%u\n", addr, Network::ntohs()(_rx_address.sin_port));

	auto info = _rx_buf.get_payload<Protocol::DiscoverReply>(op);
	printf("Device ID.........: %04X\n", info->device_id.native());
	printf("Bootloader version: %u.%02u\n", info->version >> 8, info->version & 0xff);
	printf("Bootloader address: 0x%06X\n", info->bootloader_address.native());
}

// Discover device on network
void NetworkProgrammer::discover_device(uint16_t port) {
	set_address(INADDR_BROADCAST, port);

	_tx_buf.select_operation(Protocol::OP_DISCOVER);

	discover();
}

// Configure a device's network
void NetworkProgrammer::configure_device(uint32_t ip_address, uint16_t port) {
	const uint8_t mac[6] = {0xCF, 0x8B, 0xC1, 0xB5, 0xB8, 0x0D};

	set_address(INADDR_BROADCAST, port);

	auto conf = _tx_buf.prepare_payload<Protocol::NetworkConfig>();
	conf->ip_address = ip_address;
	std::memcpy(&conf->mac_address, mac, sizeof(mac));

	discover(Protocol::OP_NET_CONFIG);
}

// Select device
void NetworkProgrammer::connect_device(uint32_t ip_address, uint16_t port) {
	set_address(ip_address, port);

	_tx_buf.select_operation(Protocol::OP_DISCOVER);

	discover();
}

// Read a device's memory
std::span<const std::byte> NetworkProgrammer::read(uint32_t address, size_t size) {
	auto read = _tx_buf.prepare_payload<Protocol::Read>();
	read->address = address;
	// TODO: Limit read length
	read->length = size;

	communicate();
	return _rx_buf.get_payload(Protocol::OP_READ);
}

// Write a device's memory
void NetworkProgrammer::write(uint32_t address, const std::span<const std::byte>& buffer) {
	if ((address % WRITE_SIZE) || (buffer.size_bytes() != WRITE_SIZE))
		throw Exception("Write not alligned to sector size!");

	auto write = _tx_buf.prepare_payload<Protocol::Write>();
	write->address = address;
	std::memcpy(write->data, buffer.data(), sizeof(write->data));

	communicate();
}

// Erase a device's memory
void NetworkProgrammer::erase(uint32_t address) {
	auto erase = _tx_buf.prepare_payload<Protocol::Erase>();
	erase->address = address;

	communicate();
}

// Reset a device
void NetworkProgrammer::reset() {
	_tx_buf.select_operation(Protocol::OP_RESET);
	communicate();
}

// Calculate a checksum of a device's memory
uint32_t NetworkProgrammer::checksum(uint32_t address, size_t size) {
	auto checksum = _tx_buf.prepare_payload<Protocol::Checksum>();
	checksum->address = address;

	communicate();

	auto result = _rx_buf.get_payload<Protocol::ChecksumReply>(Protocol::OP_CHECKSUM);
	return result->checksum;
}

/* TransmitBuffer */

NetworkProgrammer::TransmitBuffer::TransmitBuffer() 
	: _size(0)
{
	auto hdr = get_header();
	hdr->version = Protocol::VERSION;
	hdr->status = Protocol::STATUS_REQUEST;
}

// Select operation without payload
void NetworkProgrammer::TransmitBuffer::select_operation(Protocol::Operation op) {
	get_header()->operation = op;
	_size = sizeof(Protocol::Header);
}

Protocol::Header* NetworkProgrammer::TransmitBuffer::get_header() {
	static_assert(std::tuple_size<decltype(_buffer)>::value >= sizeof(Protocol::Header), "Tx buffer too small");
	return reinterpret_cast<Protocol::Header*>(_buffer.data());
}
const Protocol::Header* NetworkProgrammer::TransmitBuffer::get_header() const {
	static_assert(std::tuple_size<decltype(_buffer)>::value >= sizeof(Protocol::Header), "Tx buffer too small");
	return reinterpret_cast<const Protocol::Header*>(_buffer.data());
}

// Get span of a prepared data. Increment sequence number.
const std::span<const std::byte> NetworkProgrammer::TransmitBuffer::data() {
	get_header()->seq++;
	return std::span<const std::byte>(_buffer.data(), _size);
}

/* ReceiveBuffer */

const Protocol::Header* NetworkProgrammer::ReceiveBuffer::get_header() const {
	// TODO: assert(_size >= sizeof(Protocol::Header), "Rx buffer too small");
	return reinterpret_cast<const Protocol::Header*>(_buffer.data());
}

// Set length of a data in the buffer
void NetworkProgrammer::ReceiveBuffer::set_content_length(size_t size) {
	assert(size <= _buffer.size());
	_size = size;
}
