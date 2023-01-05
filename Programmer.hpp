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

class NetworkProgrammer {
	public:
		// Read a device's memory
		virtual void read(uint32_t address, std::span<std::byte> buffer) = 0;

		// Write a device's memory
		virtual void write(uint32_t address, const std::span<const std::byte> &buffer) = 0;

		// Erase a device's memory
		virtual void erase(uint32_t address) = 0;

		// Reset a device
		virtual void reset() = 0;

		// Calculate a checksum of a device's memory
		virtual uint32_t checksum(uint32_t address, size_t size) = 0;
};

class NetworkProgrammer {
	public:
		// Discover device on network
		bool discover_device();

		// Configure a device's network
		bool configure_device(uint32_t ip_address);

		// Select device
		bool connect_device(uint32_t ip_address);

		// Set UDP port number used for communication
		void set_port(uint16_t port);

		// Read a device's memory
		virtual void read(uint32_t address, std::span<std::byte> buffer);

		// Write a device's memory
		virtual void write(uint32_t address, const std::span<const std::byte>& buffer);

		// Erase a device's memory
		virtual void erase(uint32_t address);

		// Reset a device
		virtual void reset();

		// Calculate a checksum of a device's memory
		virtual void checksum(uint32_t address, size_t size);
	private:
		// Process received frame
		void process();

		// Send frame and wait for reply
		void communicate();
};

#endif /* __PROGRAMMER_HPP__ */
