/* Copyright (c) 2021 Parola Marco
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "clparser.hpp"

#include <string_view>
#include <vector>
#include <optional>
#include <cstring>
#include <cassert>
#include <cmath>



namespace {

	template<typename uint>
	std::optional<uint> parse_uint(const std::string& str) {
		using ssize_t = std::make_signed_t<size_t>;
		uint r = 0;
		auto last = str.size() - 1;
		for(ssize_t i = last; i >= 0; --i) {
			static_assert('9' - '0' == 9);
			int decDigitValue = str[i] - '0';
			if((decDigitValue < 0) || (decDigitValue > 9)) {
				return std::nullopt;
			} else {
				r += decDigitValue * pow(10, last - i);
			}
		}
		return r;
	}


	/** If the long option is in a single argument, split it and return the
	 * value portion of it; otherwise, increment the cursor and return the next
	 * argument. If the key does not match the one provided, `std::nullopt`
	 * is returned. */
	std::optional<std::string> get_long_option_value(
			const std::string& key,
			const xorinator::StaticVector<std::string_view>& argvxx,
			size_t& cursor
	) {
		if(! (argvxx[cursor].starts_with(key.c_str()) && (
			(argvxx[cursor].size() == key.size()) ||
			((argvxx[cursor].size() > key.size()) && (argvxx[cursor][key.size()] == '='))
		))) {
			return std::nullopt;
		}
		{
			// Check for a '=', eventually return the result
			for(size_t i=0; char c : argvxx[cursor]) {
				if(c == '=') {
					return {
						std::string(argvxx[cursor].begin() + (i+1), argvxx[cursor].end()) };
				}
				++i;
			}
		} { // Increment the cursor, then return the two arguments
			++cursor;
			if(cursor < argvxx.size()) {
				return std::string(argvxx[cursor]);
			} else {
				return "";
			}
		}
	}

	/** If the short option is in a single argument, split it and return the
	 * value portion of it; otherwise, increment the cursor and return the next
	 * argument. If the key does not match the one provided, `std::nullopt`
	 * is returned. */
	std::optional<std::string> get_short_option_value(
			char key,
			const xorinator::StaticVector<std::string_view>& argvxx,
			size_t& cursor
	) {
		using namespace std::string_literals;
		if((argvxx[cursor].size() < 2) || (argvxx[cursor][1] != key)) {
			return std::nullopt;
		}
		if(argvxx[cursor].size() == 2) {
			++cursor;
			if(cursor < argvxx.size()) {
				return std::string(argvxx[cursor]);
			} else {
				return "";
			}
		} else {
			return std::string(argvxx[cursor].begin() + 2);
		}
	}



	/** Checks the presence of a long option argument.
	 * If the argument is a long option, it is interpreted and the state of
	 * the "construction" variables is altered accordingly, then returns `true`;
	 * returns `false` otherwise. */
	bool check_option_long(
			const xorinator::StaticVector<std::string_view>& argvxx,
			size_t& cursor,
			std::vector<std::string>& rngKeysDynV,
			xorinator::cli::OptionBits::IntType& options,
			size_t& litterSize
	) {
		using xorinator::cli::OptionBits;
		if((argvxx[cursor].size() < 3) || (! argvxx[cursor].starts_with("--"))) return false;
		std::optional<std::string> optValue;
		if(optValue = get_long_option_value("--key", argvxx, cursor)) {
			rngKeysDynV.push_back(optValue.value());
		} else
		if(optValue = get_long_option_value("--litter", argvxx, cursor)) {
			auto uintValue = parse_uint<size_t>(optValue.value());
			if(uintValue) {
				litterSize = uintValue.value();
			} else {
				throw xorinator::cli::InvalidCommandLineException(
					"invalid positive number \"" + optValue.value() + '"');
			}
		} else
		if(argvxx[cursor] == "--quiet") {
			options = options | OptionBits::eQuiet;
		} else
		if(argvxx[cursor] == "--force") {
			options = options | OptionBits::eForce;
		} else {
			throw xorinator::cli::InvalidCommandLineException(
				"unrecognized option \"" + std::string(argvxx[cursor]) + '"');
		}
		return true;
	}


	/** Checks the presence of a short option argument.
	 * If the argument is a short option, it is interpreted and the state of
	 * the "construction" variables is altered accordingly, then returns `true`;
	 * returns `false` otherwise. */
	bool check_option_short(
			const xorinator::StaticVector<std::string_view>& argvxx,
			size_t& cursor,
			std::vector<std::string>& rngKeysDynV,
			xorinator::cli::OptionBits::IntType& options,
			size_t& litterSize
	) {
		using xorinator::cli::OptionBits;
		if(
				(argvxx[cursor].size() < 2) ||
				(argvxx[cursor][0] != '-') || (argvxx[cursor][1] == '-')
		) {
			return false;
		}
		std::optional<std::string> optValue;
		if(optValue = get_short_option_value('k', argvxx, cursor)) {
			rngKeysDynV.push_back(optValue.value());
		} else
		if(optValue = get_short_option_value('g', argvxx, cursor)) {
			auto uintValue = parse_uint<size_t>(optValue.value());
			if(uintValue) {
				litterSize = uintValue.value();
			} else {
				throw xorinator::cli::InvalidCommandLineException(
					"invalid positive number \"" + optValue.value() + '"');
			}
		} else
		for(char option : std::string_view(argvxx[cursor].begin() + 1, argvxx[cursor].end())) {
			if(option == 'q') {
				options = options | OptionBits::eQuiet;
			} else
			if(option == 'f') {
				options = options | OptionBits::eForce;
			} else {
				throw xorinator::cli::InvalidCommandLineException(
					"unrecognized option \"" + std::string(argvxx[cursor]) + '"');
			}
		}
		return true;
	}


	/** Checks the presence of an option argument.
	 * If the argument is an option, it is interpreted and the state of
	 * the "construction" variables is altered accordingly, then returns `true`;
	 * returns `false` otherwise. */
	bool check_option(
			const xorinator::StaticVector<std::string_view>& argvxx,
			size_t& cursor,
			std::vector<std::string>& rngKeysDynV,
			xorinator::cli::OptionBits::IntType& options,
			size_t& litterSize
	) {
		return
			check_option_short(argvxx, cursor, rngKeysDynV, options, litterSize) ||
			check_option_long(argvxx, cursor, rngKeysDynV, options, litterSize);
	}


	xorinator::cli::CmdType type_from_strvw(const std::string_view& sv) {
		if(sv.empty() || sv == "?" || sv == "help") {
			return xorinator::cli::CmdType::eNone; }
		if(sv == "multiplex" || sv == "mux" || sv == "m") {
			return xorinator::cli::CmdType::eMultiplex; }
		if(sv == "demultiplex" || sv == "demux" || sv == "dmx" || sv == "d") {
			return xorinator::cli::CmdType::eDemultiplex; }
		return xorinator::cli::CmdType::eError;
	}

}



namespace xorinator::cli {

	CommandLine::CommandLine():
			cmdType(CmdType::eNone),
			litterSize(0),
			firstLiteralArg(1),
			options(OptionBits::eNone)
	{ }


	CommandLine::CommandLine(int argc, char const * const * argv):
			cmdType(CmdType::eNone),
			litterSize(0),
			firstLiteralArg(argc + 1),
			options(0)
	{
		using namespace std::string_view_literals;
		auto argvxx = StaticVector<std::string_view>(argc);
		std::vector<std::string> rngKeysDynV;  rngKeysDynV.reserve(2);
		std::vector<std::string> argsDynV;  argsDynV.reserve(argc);

		{ // Copy argv into its C++ style friend
			for(int i=0; i < argc; ++i) {
				argvxx[i] = std::string_view(argv[i]); }
		} { // Parse the arguments
			size_t cursor = 1;  // Skip the first argv element
			bool literal = false;  // Arguments to be inserted without parsing after "--"
			assert(argc >= 0); // You never know
			while(cursor < size_t(argc)) {
				if(literal) {
					argsDynV.push_back(std::string(argvxx[cursor]));
				} else {
					if(argvxx[cursor] == "--"sv) {
						literal = true;
						firstLiteralArg = argsDynV.size();
					}
					else if(! check_option(argvxx, cursor, rngKeysDynV, options, litterSize)) {
						/* If ::check_option returns `true`, then `cursor`, `rngKeysDynV` and
						 * `cursor` are modified by said function. */
						argsDynV.push_back(std::string(argvxx[cursor]));
					}
				}
				++cursor;
			}
		} { // Move the dynamic vectors' content to the StaticVector members
			zeroArg = argvxx[0];
			rngKeys = StaticVector<std::string>(rngKeysDynV.size());
			std::move(rngKeysDynV.begin(), rngKeysDynV.end(), rngKeys.begin());
			// argsDynV:  [0] subcommand,  [1] first arg,  [2] var arg 0,  [3] var arg 1...
			if(! argsDynV.empty()) {
				cmdType = type_from_strvw(argsDynV.front());
				if(argsDynV.size() > 1) {
					firstArg = argsDynV[1];
					if(argsDynV.size() >= 2) {
						variadicArgs = StaticVector<std::string>(argsDynV.size() - 2);
						std::move(argsDynV.begin() + 2, argsDynV.end(), variadicArgs.begin());
					}
				}
			}
		}
	}

}
