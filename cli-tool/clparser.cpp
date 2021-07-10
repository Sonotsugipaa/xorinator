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
#include <cstring>

using namespace std::string_literals;
using namespace std::string_view_literals;



namespace {

	bool check_option(
			const xorinator::StaticVector<std::string_view>& argvxx,
			int& cursor,
			std::vector<std::string>& rngKeysDynV,
			xorinator::cli::OptionBits::IntType& options
	) {
		static constexpr auto checkOptPrefix = [](const std::string_view& str) {
			return
				(str.front() == '-') && (
					(
						(str.size() > 2) && (str[1] == '-')
					) || (
						str.size() > 1
					)
				);
		};
		if((! argvxx[cursor].empty()) && checkOptPrefix(argvxx[cursor])) {
			if(argvxx[cursor] == "-k"sv || argvxx[cursor] == "--key"sv) {
				if(argvxx.size() > static_cast<unsigned>(cursor) + 1) {
					++cursor;
					rngKeysDynV.push_back(std::string(argvxx[cursor]));
				} else {
					throw xorinator::cli::InvalidCommandLine("no value to "s + std::string(argvxx[cursor]) + " option"s);
				}
			} else
			if(argvxx[cursor] == "-q"sv || argvxx[cursor] == "--quiet"sv) {
				options |= xorinator::cli::OptionBits::eQuiet;
			} else {
				throw std::runtime_error(
					"invalid option \""s + std::string(argvxx[cursor]) + "\""s);
			}
			return true;
		} else {
			return false;
		}
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

	CommandLine::CommandLine(int argc, char const * const * argv):
			cmdType(CmdType::eNone),
			options(0)
	{
		auto argvxx = StaticVector<std::string_view>(argc);
		std::vector<std::string> rngKeysDynV;  rngKeysDynV.reserve(2);
		std::vector<std::string> argsDynV;  argsDynV.reserve(argc);

		{ // Copy argv into its C++ style friend
			for(int i=0; i < argc; ++i) {
				argvxx[i] = std::string_view(argv[i]); }
		} { // Parse the arguments
			int cursor = 1;  // Skip the first argv element
			bool literal = false;  // Arguments to be inserted without parsing after "--"
			while(cursor < argc) {
				if(literal) {
					argsDynV.push_back(std::string(argvxx[cursor]));
				} else {
					if(argvxx[cursor] == "--"sv) {
						literal = true;
					}
					else if(! check_option(argvxx, cursor, rngKeysDynV, options)) {
						argsDynV.push_back(std::string(argvxx[cursor]));
					}
				}
				++cursor;
			}
		} { // Move the dynamic vectors' content to the StaticVector members
			zeroArg = argvxx[0];
			rngKeys = StaticVector<std::string>(rngKeysDynV.size());
			std::move(rngKeysDynV.begin(), rngKeysDynV.end(), rngKeys.begin());
			// argsDynV:  [0] command,  [1] first arg,  [2] var arg 0,  [3] var arg 1...
			if(! argsDynV.empty()) {
				cmdType = type_from_strvw(argsDynV.front());
				if(argsDynV.size() >= 1) {
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
