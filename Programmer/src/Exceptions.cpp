/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#include <string>
#include <exception>
//#include <stdexcept>
#include <format>

#include <Programmer/Exceptions.hpp>

Exception::Exception(const std::string_view format, std::format_args&& args)
	: std::runtime_error(nullptr)
{
	_message = std::vformat(format, args);
}


#if 0
Exception::Exception(const char* message, ...)
	: std::runtime_error(nullptr)
{
	std::va_list arg_list;

	va_start(arg_list, message);
	_message = std::format(message, arg_list);
	va_end(arg_list);
}

void Exception::prepend(const char* message, ...) {
	std::va_list arg_list;

	va_start(arg_list, message);
	_message.insert(0, std::format(message, arg_list));
	va_end(arg_list);
}

void Exception::append(const char* message, ...) {
	std::va_list arg_list;

	va_start(arg_list, message);
	_message.append(std::format(message, arg_list));
	va_end(arg_list);
}
#endif

