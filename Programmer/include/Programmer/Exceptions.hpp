/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Koko Software. All rights reserved.
 *
 * Author: Adrian Warecki <embedded@kokosoftware.pl>
 */

#ifndef __EXCEPTIONS_HPP__
#define __EXCEPTIONS_HPP__

#include <string>
#include <exception>
//#include <stdexcept>
#include <format>

namespace programmer {

class Exception : public std::runtime_error {
	public:
		Exception() : std::runtime_error(nullptr) {};
		Exception(const std::string_view format, std::format_args&& args);

		template <typename... Args>
		Exception(const std::string_view format, Args&&... args) : std::runtime_error(nullptr) {
			_message = std::vformat(format, std::make_format_args(args...));
		}

		template <typename... Args>
		void append(const std::string_view format, Args&&... args) {
			_message.append(" ");
			_message.append(std::vformat(format, std::make_format_args(args...)));
		}

		template <typename... Args>
		void prepend(const std::string_view format, Args&&... args) {
			_message.insert(0, " ");
			_message.insert(0, std::vformat(format, std::make_format_args(args...))); 
		}

#if 0
		constexpr void log() {
		Exception(const char* message, ...);
		void prepend(const char* message, ...);
		void append(const char* message, ...);
#endif

		virtual const char* what() const override {
			return _message.c_str();
		}

	protected:
		std::string _message;
};

} // namespace programmer

#endif /* __EXCEPTIONS_HPP__ */
