// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include "Programmer.hpp"

// Discover device on network
bool NetworkProgrammer::discover_device() {

}

// Configure a device's network
bool NetworkProgrammer::configure_device(uint32_t ip_address) {

}

// Select device
bool NetworkProgrammer::connect_device(uint32_t ip_address) {

}

// Set UDP port number used for communication
void NetworkProgrammer::set_port(uint16_t port) {

}

// Process received frame
void NetworkProgrammer::process() {

}

// Send frame and wait for reply
void NetworkProgrammer::communicate() {

}

// Read a device's memory
void NetworkProgrammer::read(uint32_t address, std::span<std::byte> buffer) {

}

// Write a device's memory
void NetworkProgrammer::write(uint32_t address, const std::span<const std::byte>& buffer) {

}

// Erase a device's memory
void NetworkProgrammer::erase(uint32_t address) {

}

// Reset a device
void NetworkProgrammer::reset() {

}

// Calculate a checksum of a device's memory
void NetworkProgrammer::checksum(uint32_t address, size_t size) {

}
