/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __IMAGE_HPP__
#define __IMAGE_HPP__

#include <span>
#include <list>
#include <vector>

class MemoryBlock {
	public:
		MemoryBlock(size_t address, const std::span<const std::byte>& data) : address(address), data(data) {};
		size_t end_address() const { return address + data.size(); };
		const size_t address;
		std::span<const std::byte> data;
};

class Section {
	public:
		Section(const MemoryBlock& block);
		void add(const MemoryBlock& block);
		bool joinable(const MemoryBlock& block);
		size_t end_address() const { return _address + _data.size(); };

	protected:
		size_t _address;
		std::vector<std::byte> _data;
};

class ImageInterface {
	public:
		virtual void process(size_t address, const std::span<const std::byte>& data) = 0;
};

class Image : public ImageInterface {
	public:
		virtual void process(size_t address, const std::span<const std::byte>& data);

	protected:
		std::list<Section> _sections;
};

#endif /* __IMAGE_HPP__ */