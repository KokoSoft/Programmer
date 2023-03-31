/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __DEVICE_HPP__
#define __DEVICE_HPP__

#include <cinttypes>
#include <array>
#include <string>

namespace programmer {

consteval auto KB(size_t x)
{
	return x * 1024;
}

class DeviceDescriptor final {
public:
	DeviceDescriptor(uint16_t p_dev_id, const char* p_name, uint32_t p_flash_size, uint32_t p_config_addr) noexcept
		: dev_id(p_dev_id), name(p_name), flash_size(p_flash_size), config_address(p_config_addr)
	{};

	constexpr bool operator==(uint16_t id) const noexcept {
		return dev_id == id;
	}

	const uint16_t dev_id;
	const std::string name;
	const size_t flash_size;
	const uint32_t config_address;

	static constexpr uint32_t MAX_ADDR = 0x1FFFFF;
	static constexpr uint32_t CONFIG1L = 0x300000;
	static constexpr uint32_t CONFIG1H = 0x300001;
	static constexpr uint32_t CONFIG2L = 0x300002;
	static constexpr uint32_t CONFIG2H = 0x300003;
	static constexpr uint32_t CONFIG3L = 0x300004;
	static constexpr uint32_t CONFIG3H = 0x300005;
	static constexpr uint32_t DEVID1 = 0x3FFFFE;
	static constexpr uint32_t DEVID2 = 0x3FFFFF;
	static constexpr uint32_t ERASE_SIZE = 1024;
	static constexpr uint32_t WRITE_SIZE = 64;
	static constexpr uint32_t RESET_VECTOR = 0x00;
	static constexpr uint32_t HI_PRIO_VECTOR = 0x08;
	static constexpr uint32_t LO_PRIO_VECTOR = 0x18;

	enum : uint16_t {
		PIC18F66J60 = 0b00011000000,
		PIC18F86J60 = 0b00011000001,
		PIC18F96J60 = 0b00011000010,
		PIC18F66J65 = 0b00011111000,
		PIC18F86J65 = 0b00011111010,
		PIC18F96J65 = 0b00011111100,
		PIC18F67J60 = 0b00011111001,
		PIC18F87J60 = 0b00011111011,
		PIC18F97J60 = 0b00011111101,
	};

	static const DeviceDescriptor* find(uint16_t dev_id);
	static constexpr uint8_t get_revision(uint16_t dev_id) noexcept { return dev_id & REV_MASK; }
	static constexpr uint16_t get_id(uint16_t dev_id) noexcept { return dev_id >> ID_SHIFT; }
private:
	static constexpr uint32_t ID_SHIFT = 5;
	static constexpr uint32_t REV_MASK = (1 << ID_SHIFT) - 1;
	static constexpr uint32_t ID_MASK = ~REV_MASK;
};

} // namespace programmer

#endif /* __DEVICE_HPP__ */
