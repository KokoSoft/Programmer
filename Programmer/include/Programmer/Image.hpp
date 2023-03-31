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
		Section(size_t address, size_t size);
		void add(const MemoryBlock& block);
		bool joinable(const MemoryBlock& block);
		size_t end_address() const { return _address + _data.size(); };
		size_t address() const { return _address; };
		size_t size() const { return _data.size(); };
		std::span<const std::byte> data() const { return std::span(_data); };
		bool operator <(const Section& s) { return _address < s._address; }

	protected:
		size_t _address;
		std::vector<std::byte> _data;
		friend class Image;
};

class ImageInterface {
	public:
		virtual void process(size_t address, const std::span<const std::byte>& data) = 0;
		virtual std::span<std::byte> process(size_t address, size_t size) = 0;
};

class Image : public ImageInterface {
	protected:
		virtual void process(size_t address, const std::span<const std::byte>& data);
		virtual std::span<std::byte> process(size_t address, size_t size);

	protected:
		std::list<Section> _sections;
};

class ImageProgrammer : public Image {
	public:
		void program();
		void erase(size_t address);
		void write(size_t address, const std::span<std::byte>& data);

		enum Operation {
			READ, WRITE, ERASE, VERIFY
		};
		void progress(size_t pos, size_t max, Operation op);
};
#endif /* __IMAGE_HPP__ */