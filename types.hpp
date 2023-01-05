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
#include <exception>
#include <stdexcept>
#include <cassert>

#define _T(x) L ## x
typedef std::wstring String;
#define Exception(x) std::runtime_error(x)

#endif /* __TYPES_HPP__ */
