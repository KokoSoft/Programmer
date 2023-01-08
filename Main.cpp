// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#include <iostream>
#include "Elf.hpp"
#include "Hex.hpp"
#include "Image.hpp"
#include "Network.hpp"
#include "Programmer.hpp"
#include "DeviceDescriptor.hpp"
#include "Target.hpp"

// TODO: Move this heaer to Network
#include <ws2tcpip.h>


int main(int argc, char** argv) {
	try {
		Network::startup();

		std::cout << "Hello World! " << argc << "\n";

		if (!!(argc > 1)) {
			Target t(DeviceDescriptor::PIC18F97J60 << 5, 128);
			t.start();
		} else {
			NetworkProgrammer prog;
			//prog.discover_device();
			IN_ADDR ip;
			inet_pton(AF_INET, "192.168.56.101", &ip);
			prog.connect_device(ip.s_addr);
			prog.read(1024, 1024);
			prog.erase(1024);
			prog.read(1024, 1024);
			std::array<std::byte, 64> tmp;
			tmp.fill((std::byte)0xAA);
			prog.write(1024+128, tmp);
			prog.read(1024, 256);
		}

		if (0)
		{
			Image img;
			Elf elf("mc.production.elf");
			elf.read_image2(img);
			//elf.print();
		}

		if (0)
		{
			ImageProgrammer img;
			Elf elf("rolety.X.production.elf");
			elf.read_image(img);
			img.program();
			/*
			 *
			 * 0x0 - 0x3; 0x4 bytes (0x4 in from file)
			 * 0x8 - 0x57; 0x50 bytes (0x50 in from file)
			 * 0xFC00 - 0xFFFE; 0x3FF bytes (0x3FF in from file)
			 * 0x10000 - 0x13C16; 0x3C17 bytes (0x3C17 in from file)
			 * 0x13C18 - 0x13C25; 0xE bytes (0xE in from file)
			 * 0x1FFF8 - 0x1FFFD; 0x6 bytes (0x6 in from file)
			 *
			 *
			 * 0x0 - 0x3; 0x4 bytes (reset_vec)
			 * 0x8 - 0x57; 0x50 bytes (intcode)
			 * 0x0FC00 - 0x0FFFE; 0x3FF bytes (mediumconst$0)
			 * 0x10000 - 0x13C16; 0x3C17 bytes (text42)
			 * 0x13C18 - 0x13C25; 0xE bytes (text71)
			 * 0x1FFF8 - 0x1FFFD; 0x6 bytes (_stray_data_0_)
			 */

			 /* Image from elf:
			  * 0x0 - 0x3; 0x4 bytes (0x4 in from file)
			  * 0x1FF3E - 0x1FF57; 0x1A bytes (0x1A in from file)
			  * 0x1FF58 - 0x1FF7B; 0x24 bytes (0x24 in from file)
			  * 0x1FF7C - 0x1FFB9; 0x3E bytes (0x3E in from file)
			  * 0x1FFBA - 0x1FFF7; 0x3E bytes (0x3E in from file)
			  * 0x1FFF8 - 0x1FFFD; 0x6 bytes (0x6 in from file)
			  */

			  /*
			0x1 - 0x39; 0x39 bytes (0x0 in from file)
			0xD7E - 0xE7F; 0x102 bytes (0x0 in from file)
			*/
			/*
			0x1 - 0x39; 0x39 bytes (0x0 in from file)
			0xD7E - 0xE7F; 0x102 bytes (0x0 in from file)
			*/

		}

		if (0)
		{
			Image img;
			Hex::read("rolety.X.production.hex", img);
			//Hex::read("mc.production.hex", img);
			/*
			 * 0x0 0x3
			 * 0x8 0x57
			 * 0xfc00 0xfffe
			 * 0x10000 0x13c16
			 * 0x13c18 0x13c25
			 * 0x1fff8 0x1fffd
			 */
		}

	}
	catch (std::exception& err) {
		//::SetConsoleOutputCP(CP_UTF8);
		printf("Error: %s\n", err.what());
	}

	Network::cleanup();
	return 0;
}
