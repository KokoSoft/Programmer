// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <algorithm>

#include "DeviceDescriptor.hpp"
#include "types.hpp"

const std::array supported_devices = {
	DeviceDescriptor(DeviceDescriptor::PIC18F66J60, "PIC18F66J60", KB(64), 0xFFF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F86J60, "PIC18F86J60", KB(64), 0xFFF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F96J60, "PIC18F96J60", KB(64), 0xFFF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F66J65, "PIC18F66J65", KB(96), 0x17FF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F86J65, "PIC18F86J65", KB(96), 0x17FF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F96J65, "PIC18F96J65", KB(96), 0x17FF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F67J60, "PIC18F67J60", KB(128), 0x1FFF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F87J60, "PIC18F87J60", KB(128), 0x1FFF8),
	DeviceDescriptor(DeviceDescriptor::PIC18F97J60, "PIC18F97J60", KB(128), 0x1FFF8),
};

const DeviceDescriptor* DeviceDescriptor::find(uint16_t dev_id) {
	auto dev = std::find(supported_devices.begin(), supported_devices.end(), get_id(dev_id));
	if (dev == supported_devices.end())
		throw Exception("Unknown device id.");

	return &*dev;
}
