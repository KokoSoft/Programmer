// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <iostream>
#include <array>

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

std::span<std::byte> Image::process(size_t address, size_t size) {
	return std::span(_sections.emplace_back(address, size)._data);
}


Section::Section(const MemoryBlock& block) {
	std::cout << " new";
	_address = block.address;
	_data.assign(block.data.begin(), block.data.end());
}

Section::Section(size_t address, size_t size) {
	_address = address;
	_data.resize(size);
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




constexpr uint32_t ERASE_SIZE = 1024;
constexpr uint32_t WRITE_SIZE = 64;

constexpr size_t sector_align(size_t addr) {
	return addr & ~(WRITE_SIZE - 1);
}
constexpr size_t erase_align(size_t addr) {
	return addr & ~(ERASE_SIZE - 1);
}
constexpr size_t min(size_t a, size_t b) {
	return a < b ? a : b;
}

void ImageProgrammer::program() {
	size_t erase_end = 0;	// End address of erased page
	size_t sector_addr = 0;	// Start address of current sector
	size_t sector_end = 0;	// End address of current sector
	std::array<std::byte, WRITE_SIZE> buffer;
	
	_sections.sort();
	for (Section& sec : _sections) {
		printf("0x%06zX - 0x%06zX\n", sec.address(), sec.address() + sec.size() - 1);
		std::span<const std::byte>::iterator data = sec.data().begin();
		auto address = sec.address();
		const auto end_address = sec.end_address();

		// Process section
		while (address < end_address) {
			// Switch to a new sector
			if (address >= sector_end) {
				// Write prepared data
				if (sector_end)
					write(sector_addr, buffer);

				// Prepare sector buffer
				sector_addr = sector_align(address);
				sector_end = sector_addr + WRITE_SIZE;
				buffer.fill(std::byte(0xFF));

				// Erase page
				if (sector_addr >= erase_end) {
					auto erase_addr = erase_align(sector_addr);
					erase(erase_addr);
					erase_end = erase_addr + ERASE_SIZE;
				}
			}

			// Fill sector data
			const auto offset = address - sector_addr;
			const auto size = min(WRITE_SIZE - offset, end_address - address);
			const auto data_end = data + size;
			std::copy(data, data_end, buffer.begin() + offset);
			data = data_end;
			address += size;
		}
	}
	write(sector_addr, buffer);
}


void ImageProgrammer::erase(size_t address) {
	printf("\terase 0x%06zX - 0x%06zX\n", address, address + ERASE_SIZE - 1);
}

void ImageProgrammer::write(size_t address, const std::span<std::byte>& data) {
	printf("\twrite 0x%06zX - 0x%06zX (%zu)\n", address, address + WRITE_SIZE - 1, data.size());
	/*
	for (std::byte b : data)
		printf("%02X", (unsigned char)b);
	printf("\n");
	*/
}

void ImageProgrammer::progress(size_t pos, size_t max, Operation op) {

}
