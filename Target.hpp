/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __TARGET_HPP__
#define __TARGET_HPP__

#include <cinttypes>
#include <vector>

#include "Network.hpp"

class Target {
	public:
		Target(uint16_t dev_id, size_t flash_size);
		void start();
	private:
		static constexpr uint32_t ERASE_SIZE = 1024;
		static constexpr uint32_t WRITE_SIZE = 64;

		static const char* get_operation_name(uint8_t op);
		static const char* get_status_name(uint8_t stat);

		void hexdump(const void* buf, size_t len);
		void send(const void* buf, size_t size, const sockaddr_in* addr);

		const uint16_t _dev_id;
		std::vector<std::byte> _flash;
		SocketUDP _socket;

};

#endif /* __TARGET_HPP__ */
