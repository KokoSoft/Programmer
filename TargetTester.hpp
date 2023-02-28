/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __TARGETTESTER_HPP__
#define __TARGETTESTER_HPP__

/* Tester configuration */
constexpr bool TX_THROUGHPUT_TEST = false; /* Target sends a response frame indefinitely. Transmitter throughput test */
constexpr bool CHECKSUM_BYTE_SUPPORT = false; /* Target sends a response frame indefinitely. Transmitter throughput test */

#include <cstdint>
#include <random>
#include <array>
#include <chrono>
#include <queue>

#include "Network.hpp"

class Timer {
	public:
		Timer(int64_t time);
		~Timer();
		bool check();
	private:
		HANDLE _handle;
};

class TargetTester {
	public:
		TargetTester(uint32_t address);

		void test();
		static void print_size(uint64_t value);
	private:
		typedef struct {
			uint32_t seq;
		} Request;

		typedef struct {// 28 bajtów
			uint8_t RCON;
			uint8_t STKPTR;
			uint8_t ESTAT;
			uint8_t EPKTCNT;
			uint16_t ERXRDPT;
			uint16_t ERXWRPT;
			uint32_t last_seq;
			uint32_t boot_counter;
			uint32_t received_udp;
			uint32_t received_arp;
			uint32_t cur_seq;
		} Response;

		static constexpr long long TIMEOUT = 1000;
		static constexpr long long TARGET_HEADERS = 50; // 14 + 20 + 8 + 7
		static constexpr long long BUFFER_SIZE = TX_THROUGHPUT_TEST ? 1024 : 512;
		static constexpr long long MAX_PAYLOAD = BUFFER_SIZE - TARGET_HEADERS - sizeof(Response);
		// Preamble + start delimiter + inter packet gap
		static constexpr long long ETH_LAYER1_SIZE = 7 + 1 + 12;
		// phy + eth + ip + udp + frame check sequence
		static constexpr long long NET_HEADERS_SIZE = ETH_LAYER1_SIZE + 0x00E + 0x014 + 0x008 + 4;

		typedef struct {
			uint32_t seq;
			int payload_size;
			uint8_t command;
			uint8_t payload[BUFFER_SIZE];
		} Frame;

		bool received(const void* response, int len);
		void timeout();
		void send(bool clear = false);

		void prepare_payload(uint8_t* tx_buf, Frame& frame);

		void check_ESTAT(uint8_t ESTAT);
		void check_RCON(uint8_t RCON);
		void check_STKPTR(uint8_t STKPTR);

		Response _last_response;
		std::ranlux24_base _rand_eng;
		std::uniform_int_distribution<unsigned long> _rand_distr;

		SocketUDP _socket;
		std::array<struct pollfd, 1> _poll;
		struct sockaddr_in _tx_address;
		struct sockaddr_in _rx_address;

		Timer _stats_timer;
		bool _timeout;

		std::queue<Frame> _queue;
		uint32_t _seq;

		struct {
			uint32_t rx;
			uint32_t tx;
			uint32_t timeout;
			uint32_t lost_rx;
			uint32_t lost_tx;
			uint32_t payload_size;
			uint32_t payload_invalid;
			uint32_t seq_mismatch;

			uint64_t tx_total_bytes;
			uint64_t rx_total_bytes;
			std::chrono::steady_clock::time_point start_time;
		} _stats;

		enum {
			// RCON bitfield definitions
			// BOR : Brown - out Reset Status bit
			RCON_nBOR_MASK		= 0x0001,
			// POR : Power - on Reset Status bit
			RCON_nPOR_MASK		= 0x0002,
			// PD : Power - Down Detection Flag bit
			RCON_nPD_MASK		= 0x0004,
			// TO : Watchdog Timer Time - out Flag bit
			RCON_nTO_MASK		= 0x0008,
			// RI : RESET Instruction Flag bit
			RCON_nRI_MASK		= 0x0010,
			// CM : Configuration Mismatch Flag bit
			RCON_nCM_MASK		= 0x0020,
			// IPEN: Interrupt Priority Enable bit
			RCON_IPEN_MASK		= 0x0080,

			// STKPTR bitfield definitions
			// STKUNF : Stack Underflow Flag bit(1)
			STKPTR_STKUNF_MASK	= 0x0040,
			// STKFUL: Stack Full Flag bit(1)
			STKPTR_STKFUL_MASK	= 0x0080,

			// PHYRDY : Ethernet PHY Clock Ready bit
			ESTAT_PHYRDY_MASK	= 0x0001,
			// TXABRT : Transmit Abort Error bit
			ESTAT_TXABRT_MASK	= 0x0002,
			// RXBUSY : Receive Busy bit
			ESTAT_RXBUSY_MASK	= 0x0004,
			// BUFER: Ethernet Buffer Error Status bit
			ESTAT_BUFER_MASK	= 0x0040,

			// Unimplemented : Read as ‘0’
			// Reserved : Write as ‘0’
			ESTAT_RESERVED_MASK	= 0x00B8,
		};
};

#endif /* __TARGETTESTER_HPP__ */
