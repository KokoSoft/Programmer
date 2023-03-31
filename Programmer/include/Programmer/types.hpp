/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __TYPES_HPP__
#define __TYPES_HPP__

#include <stdio.h>

#include <string>
#include <stdexcept>
#include <cassert>

#include <Programmer/Exceptions.hpp>

namespace programmer {

#define _T(x) L ## x
typedef std::wstring String;

#ifdef _MSC_VER
#define PACKED_STRUCT_BEGIN	__pragma(pack(push, 1))
#define PACKED_STRUCT_END	__pragma(pack(pop))
#elif defined(__GNUC__)
#define PACKED_STRUCT_BEGIN	__attribute__ ((__packed__))
#define PACKED_STRUCT_END
#else
#error 'Declare PACKED_STRUCT_* for this compiler.'
#endif

} // namespace programmer

#endif /* __TYPES_HPP__ */
