/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __PROGRAMMER_HPP__
#define __PROGRAMMER_HPP__

#include <cstdint>
#include <span>
#include <array>
#include <concepts>
#include <utility>

#include "Network.hpp"
#include "Protocol.hpp"
#include "DeviceDescriptor.hpp"

class ETarget : public Exception {
	public:
		ETarget(const Protocol::Status status);
		const Protocol::Status _status;
};

class IProgrammer {
	public:
		IProgrammer() : _desciptor(nullptr) {}

		// Read a device's memory
		virtual std::span<const std::byte> read(uint32_t address, size_t size) = 0;

		// Write a device's memory
		virtual void write(uint32_t address, const std::span<const std::byte> &buffer) = 0;

		// Erase a device's memory
		virtual void erase(uint32_t address) = 0;

		// Reset a device
		virtual void reset() = 0;

		// Calculate a checksum of a device's memory
		virtual uint32_t checksum(uint32_t address, size_t size) = 0;

		const DeviceDescriptor* get_descriptor() { return _desciptor; }

		// TODO: Move to NetworkProgrammer?
		enum class Status { Requested, Acked, Done };
		virtual void on_status(Status status) {}

	protected:
		const DeviceDescriptor* _desciptor;
};

class NetworkProgrammer : public IProgrammer {
	public:
		NetworkProgrammer();

		// Discover device on network
		void discover_device(uint16_t port = Protocol::PORT);

		// Configure a device's network
		void configure_device(uint32_t ip_address, uint16_t port = Protocol::PORT);

		// Select device
		void connect_device(uint32_t ip_address, uint16_t port = Protocol::PORT);

		// Read a device's memory
		virtual std::span<const std::byte> read(uint32_t address, size_t size);

		// Write a device's memory
		virtual void write(uint32_t address, const std::span<const std::byte>& buffer);

		// Erase a device's memory
		virtual void erase(uint32_t address);

		// Reset a device
		virtual void reset();

		// Calculate a checksum of a device's memory
		virtual uint32_t checksum(uint32_t address, size_t size);

		struct BootloaderInfo {
			uint16_t device_id;
			uint16_t version;
			uint32_t address;
		};

		const BootloaderInfo& get_bootloader_info() {
			return _bootloader;
		}

	private:
		static constexpr long long TIMEOUT = 1000;

		enum class Result {
			Ignore, ExtendTime, Done
		};

		// Process received frame
		Result process();

		void check_connection();

		// Send frame and wait for reply
		void communicate();

		void set_address(uint32_t address, uint16_t port);

		// Process DiscoverReply from target
		void process_discover(Protocol::Operation op = Protocol::OP_DISCOVER);

		SocketUDP _socket;
		std::array<struct pollfd, 1> _poll;
		struct sockaddr_in _tx_address;
		struct sockaddr_in _rx_address;
		BootloaderInfo _bootloader;

		class TransmitBuffer {
			public:
				TransmitBuffer();

				template <typename T>
					requires requires(T v) {
					T::Operation;
				}
				T* prepare_payload() {
					constexpr auto size = sizeof(Protocol::RequestHeader) + sizeof(T);
					static_assert(std::tuple_size<decltype(_buffer)>::value >= size, "Tx buffer too small");
			
					get_header()->operation = T::Operation;
					_size = size;
					return reinterpret_cast<T*>(&_buffer[sizeof(Protocol::RequestHeader)]);
				}

				// Select operation without payload
				void select_operation(Protocol::Operation op, uint32_t address = 0, uint16_t length = 0);

				uint8_t get_operation() const { return get_header()->operation; }
				uint8_t get_sequence() const { return get_header()->seq; }

				// Get span of a prepared data. Increment sequence number.
				const std::span<const std::byte> data();

			private:
				Protocol::RequestHeader* get_header();
				const Protocol::RequestHeader* get_header() const;

				size_t _size;
				std::array<std::byte, 128> _buffer;
		} _tx_buf;

		class ReceiveBuffer {
		public:
			template <typename T>
			const T* get_payload(Protocol::Operation op) {
				if (sizeof(Protocol::ReplyHeader) + sizeof(T) > _size)
					throw Exception("Not enough data in the buffer.");

				if (get_operation() != op)
					throw Exception("The buffer contains another data type.");

				return reinterpret_cast<T*>(&_buffer[sizeof(Protocol::ReplyHeader)]);
			}

			const std::span<const std::byte> get_payload(Protocol::Operation op) {
				constexpr auto header_size = sizeof(Protocol::ReplyHeader);

				if (header_size >= _size)
					throw Exception("No payload available.");

				if (get_operation() != op)
					throw Exception("The buffer contains another data type.");
				
				return std::span<const std::byte>(_buffer.data() + header_size, _size - header_size);
			}

			uint8_t get_operation() const { return get_header()->operation; }
			uint8_t get_sequence() const { return get_header()->seq; }
			Protocol::Status get_status() const { 
				return static_cast<Protocol::Status>(get_header()->status);
			}
			uint8_t get_version() const { return get_header()->version; }

			// Get span of the buffer
			operator std::span<std::byte>() { return std::span(_buffer); };

			// Set length of a data in the buffer
			void set_content_length(size_t size);

			static constexpr size_t BUFFER_SIZE = 1500; // TODO: To nie jest prawda. Musisz uwzglêdniæ jeszcze nag³ówek IP i UDP
			static constexpr size_t MAX_PAYLOAD = BUFFER_SIZE - sizeof(Protocol::ReplyHeader);
		private:
			const Protocol::ReplyHeader* get_header() const;

			size_t _size;
			std::array<std::byte, BUFFER_SIZE> _buffer;
		} _rx_buf;
};

#endif /* __PROGRAMMER_HPP__ */
