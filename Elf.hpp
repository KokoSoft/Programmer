/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __ELF_HPP__
#define __ELF_HPP__

#include <fstream>
#include <filesystem>
#include <vector>

#include "Image.hpp"

#include "elf.h"

class Elf;

class ElfSection {
	public:
		ElfSection() : header{ 0 } {};

	protected:

		Elf32_Shdr header;
		std::shared_ptr<unsigned char[]> buffer;
		friend class Elf;
};

class StringSection : public ElfSection {
	public:
		std::string get(unsigned int index);
};

class Elf {
	public:
		Elf(std::filesystem::path path);
		void print();
		void read_section(ElfSection &section, unsigned int index);

		// Read firmware image from elf file
		void read_image(ImageInterface& image);
		void read_image2(ImageInterface& image);

	protected:
		std::ifstream file;
		std::streamsize file_size;
		std::vector<Elf32_Shdr> sections;
		std::vector<Elf32_Phdr> programs;

	private:
		Elf32_Ehdr file_header;
		StringSection strings;

		void read(void* buf, std::streamsize size);
		void read_header();
		void read_programs();
		void read_sections();
};





#endif /* __ELF_HPP__ */
