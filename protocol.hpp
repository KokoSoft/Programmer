// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Koko Software. All rights reserved.
//
// Author: Adrian Warecki <embedded@kokosoftware.pl>

#ifndef __PROTOCOL_HPP__
#define __PROTOCOL_HPP__

#include <cinttypes>
#include <bit>

namespace Protocol {
	constexpr uint16_t PORT = 666;
	constexpr uint16_t VERSION = 1;

	template <typename T>
		requires std::is_integral<T>::value
	struct bigendian {
	public:
		constexpr bigendian<T>& operator =(T v) noexcept {
			if constexpr (std::endian::native == std::endian::little)
				_value = std::byteswap(v);
			else
				_value = v;
			return *this;
		}
		constexpr operator T() const noexcept {
			if constexpr (std::endian::native == std::endian::little)
				return std::byteswap(_value);
			else
				return _value;
		}

		constexpr T native() const noexcept { return *this; }
	private:
		T _value;
	};

	typedef uint8_t be8_t;
	typedef bigendian<uint16_t> be16_t;
	typedef bigendian<uint32_t> be32_t;
	typedef uint16_t le16_t;

	enum Operation : uint8_t {
		OP_DISCOVER,	// Reply: DiscoverReply
		OP_NET_CONFIG,	// Reply: DiscoverReply
		OP_READ,		// Reply: ReadReply with STATUS_INPROGRESS, STATUS_DONE
		OP_WRITE,		// Reply: Header with STATUS_INPROGRESS, STATUS_DONE
		OP_ERASE,		// Reply: Header with STATUS_INPROGRESS, STATUS_DONE
		OP_RESET,		// Reply: Header with STATUS_OK
		OP_ERASE_WRITE,	// Reply: Header with STATUS_INPROGRESS, STATUS_OK
		OP_CHIP_ERASE,	// Reply: Header with STATUS_INPROGRESS, STATUS_OK
		OP_CHECKSUM,	// Reply: ChecksumReply
	};


	enum Status : uint8_t {
		STATUS_REQUEST,		// It is a request to a device
		STATUS_OK,
		STATUS_INPROGRESS,	// Read, Write, Erase, Checksum in progress
		//STATUS_DONE,		// Read, Write, Erase, Checksum done, attached reply structure / Reply with data
		STATUS_INV_OP,		// Unknown / unsupported operation
		STATUS_INV_PARAM,	// Invalid operation parameters
		STATUS_INV_SRC,		// Sender aren't permitted to perform this operation - discover first
		STATUS_INV_ADDR,	// Forbidden operation address
		STATUS_PKT_SIZE,	// Invalid packet size
	};

	PACKED_STRUCT_BEGIN
		struct RequestHeader {
		be8_t version;
		be8_t seq;
		be8_t operation;
		be8_t status;
		be32_t address;	// Starting address
		be16_t length;	// Length of data to read/write
	};
	PACKED_STRUCT_END

	PACKED_STRUCT_BEGIN
		struct ReplyHeader {
		be8_t version;
		be8_t seq;
		be8_t operation;
		be8_t status;
	};
	PACKED_STRUCT_END

	PACKED_STRUCT_BEGIN
	struct DiscoverReply {
		be16_t version;
		be32_t bootloader_address;
		le16_t device_id;
	};
	PACKED_STRUCT_END

	PACKED_STRUCT_BEGIN
	struct NetworkConfig {
		static constexpr uint8_t Operation = OP_NET_CONFIG;
		uint8_t mac_address[6];
		uint32_t ip_address;
		//uint8_t mac_address_eth[6];
	};
	PACKED_STRUCT_END

	struct ReadReply {
		be8_t data[1];
	};

	struct Write {
		static constexpr uint8_t Operation = OP_WRITE;
		be32_t address;
		be8_t data[64];
	};

	struct ChecksumReply {
		be32_t checksum;
	};
};

#endif // !__PROTOCOL_HPP__
