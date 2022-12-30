/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __HEX_HPP__
#define __HEX_HPP__

#include <filesystem>
#include <fstream>
#include <string_view>

class ImageInterface;

class Hex {
	public:
		static void read(const std::filesystem::path& path, ImageInterface& image);

	protected:
		std::ifstream _file;

		Hex(const std::filesystem::path& path, ImageInterface& image);

	private:
		uint32_t _start_address;
		uint32_t _segment_address;
		std::vector<uint8_t> _payload;
		ImageInterface& _image;

		void read_file();
		void parse_line(const std::string_view &line);
		bool process_line();
};

#endif /* __HEX_HPP__ */