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
#include "TargetTester.hpp"

// TODO: Move this heaer to Network
#include <ws2tcpip.h>


//#define NET_TESTER
#define BOOT_TESTER
#define NET_CONFIG
//#define DISCOVER

int main(int argc, char** argv) {
	try {
		Network::startup();

		std::cout << "Hello World! " << argc << "\n";

#ifdef NET_TESTER
		IN_ADDR ip;
		inet_pton(AF_INET, "10.11.12.13", &ip);
		TargetNetworkTester test(ip.s_addr);
		test.test();
#elif defined(BOOT_TESTER)
		auto prog = std::make_unique<NetworkProgrammer>();
		IN_ADDR ip;
		//inet_pton(AF_INET, "192.168.56.101", &ip);
		inet_pton(AF_INET, "10.11.12.3", &ip);
		prog->configure_device(ip.s_addr);

		TargetProtoTester test(std::move(prog));
		test.run_tests();
#else
		if (!!(argc > 1)) {
			Target t(DeviceDescriptor::PIC18F97J60 << 5, 128);
			t.start();
		} else {
			NetworkProgrammer prog;
			do {
				repeat:
				try {
#ifdef NET_CONFIG
			IN_ADDR ip;
			//inet_pton(AF_INET, "192.168.56.101", &ip);
			inet_pton(AF_INET, "10.11.12.3", &ip);
			prog.configure_device(ip.s_addr);
#elif defined(DISCOVER)
			/* On Windows, broadcast packets aren't sent out on every interface. They are only sent on the primary interface...
			 * and I don't recall how Windows determines which that is. 
			 * https://github.com/ubihazard/broadcast/
			 */
			//prog.discover_device();
			IN_ADDR ip;
			inet_pton(AF_INET, "10.255.255.255", &ip);
			prog.connect_device(ip.s_addr);
#else
			IN_ADDR ip;
			//inet_pton(AF_INET, "192.168.128.101", &ip);
			inet_pton(AF_INET, "10.11.12.13", &ip);
			prog.connect_device(ip.s_addr);
#endif
				}
				catch (std::exception& err) {
					//::SetConsoleOutputCP(CP_UTF8);
					printf("Error: %s\n", err.what());
					goto repeat;
				}
			} while (0);
			int ooo = 0;
			do {
			repeat2:
				if (ooo++ > 10) return 0;
				try {
					prog.read(1024, 128);

				}
				catch (std::exception& err) {
					printf("Error2: %s\n", err.what());
					goto repeat2;
				}
			} while (0);
			prog.read(1024, 128);
			try {
				prog.read(1024, 1024);
			}
			catch (std::exception& err) {
				printf("Error3: %s\n", err.what());
			}
			prog.erase(1024);
			prog.read(1024, 102);
			std::array<std::byte, 64> tmp;
			tmp.fill((std::byte)0xAA);
			prog.write(1024+128, tmp);
			prog.read(1024, 25);
		}
#endif
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
