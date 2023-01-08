// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <stdio.h>
#include <exception>
#include <memory>

#include <winsock2.h>

#include "Network.hpp"

void Network::startup() {
	WSADATA wsa_data;
	std::memset(&wsa_data, 0, sizeof(wsa_data));

	int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (res != NO_ERROR) {
		WSACleanup();
		throw SocketException();
	}
}

void Network::cleanup() {
	WSACleanup();
}
