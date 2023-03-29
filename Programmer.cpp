// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <chrono>
#include <array>
#include <cassert>

#include "Programmer.hpp"
#include "DeviceDescriptor.hpp"

/* Programmer */

// Read a device's memory
std::span<const std::byte> Programmer::read(uint32_t address, size_t size) {
	try {
		// TODO: Check address space
		
		// TODO: Only even size support

		if (size > programmer_descriptor()->max_read)
			throw Exception("Read size exceeds limit.");

		return _programmer->read(address, size);
	}
	catch (Exception& err) {
		err.prepend("Unable to read {} bytes from address {:#06X}.", size, address);
		throw;
	}
}

// Write a device's memory
void Programmer::write(uint32_t address, const std::span<const std::byte>& buffer) {
	try {
		if (address % device_descriptor()->WRITE_SIZE)
			throw Exception("Address isn't aligned to the sector size.");

		if (buffer.size_bytes() % device_descriptor()->WRITE_SIZE)
			throw Exception("Size isn't aligned to the sector size.");

		// TODO: Check address correctness?

		// TODO: Support multiple write?
		if (buffer.size_bytes() > programmer_descriptor()->max_write)
			throw Exception("Size is beyond the capabilities of the programmer.");

		_programmer->write(address, buffer);
	}
	catch (Exception& err) {
		err.prepend("Write {} bytes at address {:#06X} failed.", buffer.size_bytes(), address);
		throw;
	}
}

// Erase a device's memory
void Programmer::erase(uint32_t address) {
	try {
		if (address % device_descriptor()->ERASE_SIZE)
			throw Exception("Address isn't aligned to erase block.");

		// TODO: Support for memory regions

		_programmer->erase(address);
	}
	catch (Exception& err) {
		err.prepend("Erase device memory at address {:#06X} failed.", address);
		throw;
	}
}

// Erase whole device memory
void Programmer::chip_erase() {
	try {
		_programmer->chip_erase();
	}
	catch (Exception& err) {
		err.prepend("Chip erase failed.");
		throw;
	}
}

// Erase sector and write it
void Programmer::erase_write(uint32_t address, const std::span<const std::byte>& buffer) {
	try {
		if (address % device_descriptor()->ERASE_SIZE)
			throw Exception("Address isn't aligned to erase size.");

		if (buffer.size_bytes() % device_descriptor()->WRITE_SIZE)
			throw Exception("Size isn't aligned to the sector size.");

		// TODO: Check address correctness?

		// TODO: Support multiple write?
		if ((buffer.size_bytes() > programmer_descriptor()->max_write)||
			(buffer.size_bytes() > device_descriptor()->ERASE_SIZE))
			throw Exception("Size is beyond the capabilities of the programmer.");

		_programmer->write(address, buffer);
	}
	catch (Exception& err) {
		err.prepend("Erase and write {} bytes at address {:#06X} failed.", buffer.size_bytes(), address);
		throw;
	}
}

// Reset a device
void Programmer::reset() {
	try {
		_programmer->reset();
	}
	catch (Exception& err) {
		err.prepend("Unable to reset device.");
		throw;
	}
}

// Calculate a checksum of a device's memory
uint32_t Programmer::checksum(uint32_t address, size_t size) {
	try {
		return _programmer->checksum(address, size);
	}
	catch (Exception& err) {
		err.prepend("Unable to checksum memory region {:#X} - {:#X}.", address, address + size - 1);
		throw;
	}
}


/* IProgrammerStrategy */

// Calculate a checksum of a device's memory
uint32_t IProgrammerStrategy::checksum(uint32_t address, size_t size) {
	throw Exception("Operation is not supported.");
}

// Erase whole device memory
void IProgrammerStrategy::chip_erase() {
	throw Exception("Operation is not supported.");
}

// Erase sector and write it
void IProgrammerStrategy::erase_write(uint32_t address, const std::span<const std::byte>& buffer) {
	throw Exception("Operation is not supported.");
}


/* ETarget */

ETarget::ETarget(const Protocol::Status status)
	: Exception(), _status(status)
{
	switch (status) {
		case Protocol::STATUS_INV_OP:
			_message = "Operation not supported by the target.";

		case Protocol::STATUS_INV_PARAM:
			_message = "The target detected an invalid parameter.";

		case Protocol::STATUS_INV_LENGTH:
			_message = "Invalid operation length.";

		case Protocol::STATUS_INV_ADDR:
			_message = "Invalid operation address.";

		case Protocol::STATUS_PROTECTED_ADDR:
			_message = "Forbidden operation address.";

		case Protocol::STATUS_INV_SRC:
			_message = "Sender aren't permitted to perform this operation.";

		case Protocol::STATUS_PKT_SIZE:
			_message = "The target reported invalid packet size.";

		case Protocol::STATUS_REQUEST:
		default:
			_message = "Target reported an invalid status.";
	}
}


/* NetworkProgrammer */

const ProgrammerDescriptor NetworkProgrammer::_prog_desc = {
	// max_write
	NetworkProgrammer::ReceiveBuffer::MAX_PAYLOAD,
	// TODO: NetworkProgrammer::TransmitBuffer::,
	// max_read
	NetworkProgrammer::ReceiveBuffer::MAX_PAYLOAD
};


NetworkProgrammer::NetworkProgrammer()
	: _poll{ { _socket, POLLIN} },
	_tx_address{ AF_INET, Network::htons()(Protocol::PORT) },
	IProgrammerStrategy(&_prog_desc)
{ 
	_socket.set_dont_fragment(true);
	_socket.receive_broadcast(false);
}

void NetworkProgrammer::set_address(uint32_t address, uint16_t port) {
	_tx_address.sin_addr.s_addr = address;
	_tx_address.sin_port = Network::htons()(port);

	if (address == INADDR_BROADCAST)
		_socket.set_broadcast(true);
}

void NetworkProgrammer::check_connection() {
	if (!_dev_desc)
		throw Exception("Not connected to a target.");
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

	throw Exception("The target did not respond within the specified time.");
}

// Process received frame
NetworkProgrammer::Result NetworkProgrammer::process() {
	int rx_address_size = sizeof(_rx_address);

	const int size = _socket.recvfrom(_rx_buf, 0, &_rx_address, &rx_address_size);
	if (size < sizeof(Protocol::ReplyHeader))
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
#if 0
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
#endif
			return Result::Done;

		case Protocol::STATUS_INPROGRESS:
			if ((operation != Protocol::OP_READ) &&
				(operation != Protocol::OP_WRITE) &&
				(operation != Protocol::OP_ERASE) &&
				(operation != Protocol::OP_ERASE_WRITE) &&
				(operation != Protocol::OP_CHIP_ERASE) &&
				(operation != Protocol::OP_CHECKSUM))
				throw Exception("Received unexcepted status from target.");

			return Result::ExtendTime;

		default:
			throw ETarget(_rx_buf.get_status());
	}
}

#include <ws2tcpip.h>

// Process DiscoverReply from target
void NetworkProgrammer::process_discover(Protocol::Operation op) {
	try {
		communicate();
	}
	catch (...) {
		_socket.set_broadcast(false);
		throw;
	}
	_tx_address = _rx_address;
	_socket.set_broadcast(false);

	char addr[INET_ADDRSTRLEN] = {};
	inet_ntop(_rx_address.sin_family, &_rx_address.sin_addr, addr, sizeof(addr));
	printf("Detected target @ %s:%u\n", addr, Network::ntohs()(_rx_address.sin_port));

	auto info = _rx_buf.get_payload<Protocol::DiscoverReply>(op);
	_bootloader.address = info->bootloader_address;
	_bootloader.version = info->version;
	_bootloader.device_id = info->device_id;

	printf("Device ID.........: %04X\n", _bootloader.device_id);
	printf("Bootloader version: %u.%02u\n", _bootloader.version >> 8, _bootloader.version & 0xff);
	printf("Bootloader address: 0x%06X\n", _bootloader.address);

	_dev_desc = DeviceDescriptor::find(_bootloader.device_id);
	printf("Device............: %s rev. %u\n", _dev_desc->name.c_str(), DeviceDescriptor::get_revision(_bootloader.device_id));
}

// Discover device on network
void NetworkProgrammer::discover_device(uint16_t port) {
	set_address(INADDR_BROADCAST, port);

	_tx_buf.select_operation(Protocol::OP_DISCOVER);

	process_discover();
}

// Configure a device's network
void NetworkProgrammer::configure_device(uint32_t ip_address, uint16_t port) {
	const uint8_t mac[6] = {0xCF, 0x8B, 0xC1, 0xB5, 0xB8, 0x0D};

	set_address(INADDR_BROADCAST, port);

	auto conf = _tx_buf.prepare_payload<Protocol::NetworkConfig>();
	conf->ip_address = ip_address;
	std::memcpy(&conf->mac_address, mac, sizeof(mac));
#if 0
	conf->mac_address_eth[0] = mac[4];
	conf->mac_address_eth[1] = mac[5];
	conf->mac_address_eth[2] = mac[2];
	conf->mac_address_eth[3] = mac[3];
	conf->mac_address_eth[4] = mac[0];
	conf->mac_address_eth[5] = mac[1];
#endif

	try {
		process_discover(Protocol::OP_NET_CONFIG);
	}
	catch (Exception& err) {
		err.prepend("Unable to configure network connection.");
		throw;
	}
}

// Select device
void NetworkProgrammer::connect_device(uint32_t ip_address, uint16_t port) {
	set_address(ip_address, port);

	_tx_buf.select_operation(Protocol::OP_DISCOVER);

	try {
		process_discover();
	}
	catch (Exception& err) {
		err.prepend("Unable to connect to a target.");
		throw;
	}
}

// Read a device's memory
std::span<const std::byte> NetworkProgrammer::read(uint32_t address, size_t size) {
	check_connection();

	_tx_buf.select_operation(Protocol::OP_READ, address, static_cast<uint16_t>(size));

	communicate();
	return _rx_buf.get_payload(Protocol::OP_READ);
}

// Write a device's memory
void NetworkProgrammer::write(uint32_t address, const std::span<const std::byte>& buffer) {
	check_connection();
	
		//_tx_buf.select_operation(Protocol::OP_WRITE, address, buffer.size_bytes());
	auto write = _tx_buf.prepare_payload<Protocol::Write>();
	write->address = address;
	std::memcpy(write->data, buffer.data(), sizeof(write->data));

	communicate();
}

// Erase a device's memory
void NetworkProgrammer::erase(uint32_t address) {
	check_connection();

	_tx_buf.select_operation(Protocol::OP_ERASE, address);
	communicate();
}

// Reset a device
void NetworkProgrammer::reset() {
	check_connection();
	_tx_buf.select_operation(Protocol::OP_RESET);
	communicate();
}

// Calculate a checksum of a device's memory
uint32_t NetworkProgrammer::checksum(uint32_t address, size_t size) {
	_tx_buf.select_operation(Protocol::OP_CHECKSUM, address, size);
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
void NetworkProgrammer::TransmitBuffer::select_operation(Protocol::Operation op, uint32_t address, uint16_t length) {
	auto header = get_header();
	header->operation = op;
	header->address = address;
	header->length = length;
	_size = sizeof(Protocol::RequestHeader);
}

Protocol::RequestHeader* NetworkProgrammer::TransmitBuffer::get_header() {
	static_assert(std::tuple_size<decltype(_buffer)>::value >= sizeof(Protocol::RequestHeader), "Tx buffer too small");
	return reinterpret_cast<Protocol::RequestHeader*>(_buffer.data());
}
const Protocol::RequestHeader* NetworkProgrammer::TransmitBuffer::get_header() const {
	static_assert(std::tuple_size<decltype(_buffer)>::value >= sizeof(Protocol::RequestHeader), "Tx buffer too small");
	return reinterpret_cast<const Protocol::RequestHeader*>(_buffer.data());
}

// Get span of a prepared data. Increment sequence number.
const std::span<const std::byte> NetworkProgrammer::TransmitBuffer::data() {
	get_header()->seq++;
	return std::span<const std::byte>(_buffer.data(), _size);
}

/* ReceiveBuffer */

const Protocol::ReplyHeader* NetworkProgrammer::ReceiveBuffer::get_header() const {
	// TODO: assert(_size >= sizeof(Protocol::ReplyHeader), "Rx buffer too small");
	return reinterpret_cast<const Protocol::ReplyHeader*>(_buffer.data());
}

// Set length of a data in the buffer
void NetworkProgrammer::ReceiveBuffer::set_content_length(size_t size) {
	assert(size <= _buffer.size());
	_size = size;
}
