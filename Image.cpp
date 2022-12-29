// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <iostream>

#include "types.hpp"
#include "Image.hpp"

void Image::process(size_t address, const std::span<const std::byte>& data) {
	std::cout << " 0x" << std::hex << address << " 0x" << data.size_bytes();

	if (!data.size())
		return;

	MemoryBlock block(address, data);

	for (Section& sect : _sections) {
		if (sect.joinable(block)) {
			sect.add(block);
			return;
		}
	}

	_sections.emplace_back(block);
}

Section::Section(const MemoryBlock& block) {
	std::cout << " new";
	_address = block.address;
	_data.assign(block.data.begin(), block.data.end());
}

void Section::add(const MemoryBlock& block) {
	std::cout << " add";

	if (block.end_address() == _address) {
		std::cout << " before";
		_data.insert(_data.begin(), block.data.begin(), block.data.end());
		_address = block.address;
	} else

	if (block.address == end_address()) {
		_data.insert(_data.end(), block.data.begin(), block.data.end());
	} else
		throw Exception("Overlapping memory blocks.");
}

bool Section::joinable(const MemoryBlock& block) {
	return (block.end_address() >= _address) && (block.address <= end_address());
}
