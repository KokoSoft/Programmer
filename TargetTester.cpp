// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <chrono>
#include <cassert>
#include <cinttypes>
#include <exception>

#include "TargetTester.hpp"

#include <stdio.h>

Timer::Timer(int64_t time) {
	_handle = CreateWaitableTimer(nullptr, false, nullptr);
	if (!_handle)
		throw std::system_error(GetLastError(), std::system_category());

	// in 100 nanoseconds intervals
	LARGE_INTEGER due_time = { .QuadPart = time * -100 };
	if (!SetWaitableTimer(_handle, &due_time, time, nullptr, nullptr, false))
		throw std::system_error(GetLastError(), std::system_category());
}

Timer::~Timer() {
	CloseHandle(_handle);
}

bool Timer::check() {
	DWORD result = WaitForSingleObject(_handle, 0);
	switch (result) {
		case WAIT_OBJECT_0:
			return true;
		case WAIT_TIMEOUT:
			return false;
		case WAIT_ABANDONED:
		case WAIT_FAILED:
		default:
			throw std::system_error(GetLastError(), std::system_category());
	}
}


TargetTester::TargetTester(uint32_t address)
	: _poll{ { _socket, POLLIN} }, _timeout(false), _stats{0}, _stats_timer(1000*10)
{
	std::random_device dev;

	_rand_eng.seed(dev());

	_tx_address.sin_family = AF_INET;
	_tx_address.sin_addr.s_addr = address;
	_tx_address.sin_port = Network::htons()(666);

	if (address == INADDR_BROADCAST)
		_socket.set_broadcast(true);
	_socket.set_nonblocking(true);
	_seq = 0;
}

void TargetTester::check_ESTAT(uint8_t ESTAT) {
	if (ESTAT & ESTAT_TXABRT_MASK) printf("ESTAT: Transmit Abort Error\n");
	if (ESTAT & ESTAT_BUFER_MASK) printf("ESTAT: Ethernet Buffer Error\n");
	//if (ESTAT & ESTAT_RXBUSY_MASK) printf("ESTAT: Receive Busy\n");
	if (ESTAT & ESTAT_RESERVED_MASK) printf("ESTAT: Reserved\n");
	if (!(ESTAT & ESTAT_PHYRDY_MASK)) printf("ESTAT: Ethernet PHY Clock NOT Ready\n");
}

void TargetTester::check_RCON(uint8_t RCON) {
	if (!(RCON & RCON_nBOR_MASK)) printf("RCON: Brown-out Reset\n");
	if (!(RCON & RCON_nPOR_MASK)) printf("RCON: Power-on Reset\n");
	if (!(RCON & RCON_nPD_MASK)) printf("RCON: Power-Down Detection\n");
	if (!(RCON & RCON_nTO_MASK)) printf("RCON: Watchdog Timer Time-out\n");
	if (!(RCON & RCON_nRI_MASK)) printf("RCON: RESET Instruction\n");
	if (!(RCON & RCON_nCM_MASK)) printf("RCON: Configuration Mismatch\n");
}

void TargetTester::check_STKPTR(uint8_t STKPTR) {
	if (STKPTR & STKPTR_STKUNF_MASK) printf("STKPTR: Stack Underflow\n");
	if (STKPTR & STKPTR_STKFUL_MASK) printf("STKPTR: Stack Full\n");
}

bool TargetTester::received(const void* response, int len) {
	const struct {
		Response resp;
		uint8_t payload[MAX_PAYLOAD];
	} *rx_buf;
	rx_buf = reinterpret_cast<decltype(rx_buf)>(response);

	bool print = _timeout;

	_stats.rx++;
	_stats.rx_total_bytes += len + NET_HEADERS_SIZE;

	if (len < sizeof(Response)) {
		printf("Response too short.\n");
		return false;
	}

	Frame& const request = _queue.front();
	if (rx_buf->resp.cur_seq != request.seq) {
		printf("Seq mismatch.\n");
		_stats.seq_mismatch++;
		print = true;
#if 1
		// TODO: Generalnie to to wybucha gdy przypadkiem zostawisz wgrany firmware, który w pêtli wysy³a jedn¹ odpowiedŸ
		// Robimy w pêtli pop, a¿ wyczyœcimy queue i jest problem. Tylko teraz jest 01:40 i nie mam si³y
		// Wymyœlaæ rozwi¹zania na to. Popraw to proszê!
		while (rx_buf->resp.cur_seq != request.seq) {
			if ((rx_buf->resp.last_seq + 1) == rx_buf->resp.cur_seq)
				_stats.lost_tx++;
			else
				_stats.lost_rx++;

			_queue.pop();
			request = _queue.front();
		}
#endif
	}
	int payload_size = len - sizeof(Response);
	if (payload_size == request.payload_size) {
		if (std::memcmp(request.payload, rx_buf->payload, payload_size)) {
			_stats.payload_invalid++;
			printf("Invalid payload\n");
		}
	} else {
		_stats.payload_size++;
		printf("Payload size mismatch. Received: %d, expected: %d\n", payload_size, request.payload_size);
	}

	if (rx_buf->resp.boot_counter < _last_response.boot_counter) {
		print = true;
		printf("Boot counter decremented.\n");
	}

	if (rx_buf->resp.received_arp < _last_response.received_arp) {
		print = true;
		printf("Received ARP decremented.\n");
	}

	if (rx_buf->resp.received_udp < _last_response.received_udp) {
		print = true;
		printf("Received UDP decremented.\n");
	}

	if (rx_buf->resp.boot_counter > _last_response.boot_counter) {
		print = true;
		printf("Reboot detected (%u).\n", rx_buf->resp.boot_counter);
	}

	/*
	if (rx_buf->resp.ERXRDPT < _last_response.ERXRDPT) {
		print = true;
		printf("Receive buffer wrap around %u -> %u\n", _last_response.ERXRDPT, rx_buf->resp.ERXRDPT);
	}
	*/

	std::memcpy(&_last_response, &rx_buf->resp, sizeof(_last_response));
	if (request.command & COMMAND_CLEAR) {
		print = true;
		printf("Statistics cleared.\n");
		_last_response.boot_counter = 0;
		_last_response.received_arp = 0;
		_last_response.received_udp = 0;
	}

	check_ESTAT(rx_buf->resp.ESTAT);
	if (!ENDLESS_TX)
		check_RCON(rx_buf->resp.RCON);
	check_STKPTR(rx_buf->resp.STKPTR);

	if (print || _stats_timer.check()) {
	//if (print) {
		printf("Tx: len = %u, seq = %u. ", request.payload_size, request.seq);
		printf("Rx: last seq: %u, seq: %u, boot: %u, arp: %u, udp: %u, Read: 0x%04X, Write: 0x%04X, EPKTCNT: %u\n",
			rx_buf->resp.last_seq, rx_buf->resp.cur_seq, rx_buf->resp.boot_counter, rx_buf->resp.received_arp,
			rx_buf->resp.received_udp, rx_buf->resp.ERXRDPT, rx_buf->resp.ERXWRPT, rx_buf->resp.EPKTCNT);
	//}
		printf("Stats: Tx: %u, Rx: %u, lost rx: %u, lost tx: %u, timeout: %u, pld size: %u, pld inv: %u, seq inv: %u\n", _stats.tx, _stats.rx, _stats.lost_rx,
			   _stats.lost_tx, _stats.timeout, _stats.payload_size, _stats.payload_invalid, _stats.seq_mismatch);

#if 1
		auto now = std::chrono::steady_clock::now();
		//int elapsed = 1;
		//int elapsed = std::chrono::duration_cast<std::chrono::duration<int, std::chrono::seconds>>(now - _stats.start_time).count();
		auto delay = std::chrono::duration_cast<std::chrono::seconds>(now - _stats.start_time);
		auto elapsed = delay.count();
		if (elapsed) {
			uint64_t upload = _stats.tx_total_bytes * 8 / elapsed;
			uint64_t download = _stats.rx_total_bytes * 8 / elapsed;
			printf("Transmitted: ");
			print_size(_stats.tx_total_bytes);
			printf("B (");
			print_size(upload);
			printf("b/s) Received: ");
			print_size(_stats.rx_total_bytes);
			printf("B (");
			print_size(download);
			printf("b/s)\n");
		}
#endif
	}
	_timeout = false;
	if (!ENDLESS_TX)
		_queue.pop();
	return true;
}

void TargetTester::timeout() {
	_timeout = true;
	_stats.timeout++;
	const Frame& request = _queue.front();
	printf("Receive timeout! Payload size: %u.\n", request.payload_size);
	send();
}

void TargetTester::send(bool clear) {
	struct {
		Request reg;
		uint8_t payload[MAX_PAYLOAD];
	} tx_buf;


	int len = _rand_distr(_rand_eng) % MAX_PAYLOAD;
	if (ENDLESS_TX || TX_THROUGHPUT_TEST)
		len = MAX_PAYLOAD;

	if (!CHECKSUM_BYTE_SUPPORT)
		len &= ~1;

	_seq++;
	_seq &= 0x7FFFFFFF;

	Frame& frame = _queue.emplace();
	frame.command = 0;

	if (clear) {
		_seq |= 0x80000000;
		frame.command |= COMMAND_CLEAR;
	}

	frame.seq = _seq;
	frame.payload_size = len;

	tx_buf.reg.seq = _seq;
	prepare_payload(tx_buf.payload, frame);
	_socket.sendto(std::span<std::byte>(reinterpret_cast<std::byte*>(&tx_buf),
				   len + sizeof(Request)), 0, &_tx_address, sizeof(_tx_address));

	_stats.tx++;
	_stats.tx_total_bytes += len + sizeof(Request) + NET_HEADERS_SIZE;
}

void TargetTester::prepare_payload(uint8_t* tx_buf, Frame& frame) {
	uint8_t* check_buf = frame.payload;
	uint8_t mask = 0x5A;
	int size = frame.payload_size;

	assert(size <= sizeof(frame.payload));

	while (size) {
		auto rand = _rand_distr(_rand_eng);
		for (int i = 0; (i < sizeof(rand)) && size; i++) {
			uint8_t val = (uint8_t)rand;
			*tx_buf++ = val;
			mask ^= val;
			*check_buf++ = mask;
			rand >>= 8;
			size--;
		}
	}
}

void TargetTester::test() {
	using std::chrono::steady_clock;
	using std::chrono::milliseconds;
	using std::chrono::duration_cast;
	using std::chrono::duration;

	int rx_address_size;
	std::byte buf[BUFFER_SIZE];

	_stats.start_time = steady_clock::now();

	send(true);
	int imax = 0;
	while (1) {
		while (_queue.size() < QUEUE_FILL_LEVEL)
			send(false);

		auto now = steady_clock::now();
		auto deadline = now + milliseconds(TIMEOUT);
		for (; deadline > now; now = steady_clock::now()) {
			int timeout = duration_cast<duration<int, std::milli>>(deadline - now).count();
			int ret = Network::poll(_poll, timeout);

			if (ret && (_poll[0].revents & POLLIN)) {
				rx_address_size = sizeof(_rx_address);
				int i = 0;
				for (;;) {
					const int size = _socket.recvfrom(buf, 0, &_rx_address, &rx_address_size);
					if (size < 0)
						break;
					i++;

					if (received(buf, size)) {
						while (_queue.size() < QUEUE_FILL_LEVEL)
							send(false);
						deadline = steady_clock::now() + milliseconds(TIMEOUT);
					}
				}
				if (i > imax) imax = i;
			}
		}
		printf("imax = %d\n", imax);
		timeout();
	}
}

void TargetTester::print_size(uint64_t value) {
	double val = value;

	if (val >= 1000 * 1000 * 1000) {
		printf("%.2f G", val / (1000.0 * 1000 * 1000));
	} else if (val >= 1000 * 1000) {
		printf("%.2f M", val / (1000.0 * 1000));
	} else if (val >= 1000) {
		printf("%.2f k", val / 1000.0);
	} else
		printf("%" PRIu64 " ", value);
}
