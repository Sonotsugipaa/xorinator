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

#include <test_tools.hpp>

#include <cli-tool/clparser.hpp>

#include <array>
#include <iostream>

using namespace std::string_literals;



namespace {

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	using xorinator::cli::CmdType;


	struct DynArgv {
		std::string storage;
		int argc;
		xorinator::StaticVector<char*> argv;

		DynArgv(std::initializer_list<std::string> ln):
				argc(ln.size()),
				argv(argc+1)
		{
			{ // Find the length of the storage string first
				std::string::size_type len = 0;
				for(const auto& arg : ln) {
					len += arg.size() + 1; }
				storage = std::string(len, '\0');
			} { // Move and link the strings to the storage
				std::string::size_type cursor = 0;
				for(size_t i=0; const auto& arg : ln) {
					std::copy(arg.begin(), arg.end(), storage.begin() + cursor);
					argv[i++] = storage.data() + cursor;
					cursor += arg.size() + 1;
				}
			} { // The last argv element must always be nullptr
				argv.back() = nullptr;
			}
		}
	};


	void print_cl(std::ostream& os, const xorinator::cli::CommandLine& cl) {
		os << "xor ";
		switch(cl.cmdType) {
			default: case CmdType::eError:  os << "?";  break;
			case CmdType::eNone:  os << "''";  break;
			case CmdType::eMultiplex:  os << "mux";  break;
			case CmdType::eDemultiplex:  os << "demux";  break;
		}
		if(0 != (cl.options & xorinator::cli::OptionBits::eQuiet)) {
			os << " --quiet"; }
		for(const auto& rngKey : cl.rngKeys) {
			os << " --key " << rngKey; }
		os << "  " << cl.firstArg;
		if(! cl.variadicArgs.empty()) {
			os << ' '; }
		for(const auto& arg : cl.variadicArgs) {
			os << ' ' << arg; }
	}


	auto mk_test_cmdln(
			const DynArgv& argv, std::string zeroArg, CmdType cmdType,
			std::vector<std::string> rngKeys,
			std::string firstKeyFile, std::vector<std::string> keyFiles,
			xorinator::cli::OptionBits::IntType opts
	) {
		return [
			&argv, zeroArg, cmdType, rngKeys, firstKeyFile, keyFiles, opts
		] (std::ostream& os) {
			try {
				auto cmdln = xorinator::cli::CommandLine(argv.argc, argv.argv.data());
				print_cl(os, cmdln);  os << '\n';
				std::vector<std::string> cmdLnRngKeysDynV;
				std::vector<std::string> cmdLnVarargsDynV;
				cmdLnRngKeysDynV.insert(cmdLnRngKeysDynV.begin(), cmdln.rngKeys.begin(), cmdln.rngKeys.end());
				cmdLnVarargsDynV.insert(cmdLnVarargsDynV.begin(), cmdln.variadicArgs.begin(), cmdln.variadicArgs.end());
				bool success = true;
				#define CHECK_(EXPECT_, GOT_, MSG_) if(! (EXPECT_ == GOT_)) { os << MSG_ << " mismatch\n"; success = false; }
				#define CHECK_PR_(EXPECT_, GOT_, MSG_) if(! (EXPECT_ == GOT_)) { os << MSG_ << " mismatch (expected '" << (EXPECT_) << "', got '" << (GOT_) << "')\n"; success = false; }
					CHECK_   (cmdType, cmdln.cmdType,        "command type");
					CHECK_PR_(opts, cmdln.options,           "options");
					CHECK_   (rngKeys, cmdLnRngKeysDynV,     "rng keys");
					CHECK_PR_(zeroArg, cmdln.zeroArg,        "zero argument");
					CHECK_PR_(firstKeyFile, cmdln.firstArg,  "first argument");
					CHECK_   (keyFiles, cmdLnVarargsDynV,    "variadic arguments");
				#undef CHECK_
				os << std::flush;
				return success? eSuccess : eFailure;
			} catch(std::exception& ex) {
				os << "Exception: " << ex.what() << std::endl;
				return eFailure;
			}
		};
	}


	template<typename Exception>
	auto mk_test_cmdln_except(const DynArgv& argv) {
		return [&argv](std::ostream& os) {
			try {
				auto cmdln = xorinator::cli::CommandLine(argv.argc, argv.argv.data());
				print_cl(os, cmdln);  os << '\n';
				os << "No exception thrown" << std::endl;
				return eFailure;
			} catch(Exception& ex) {
				os << "Exception: " << ex.what() << std::endl;
				return eSuccess;
			} catch(std::exception& ex) {
				os << "Exception: " << ex.what() << std::endl;
			} catch(...) {
				os << "Exception (unhandled type)";
			}
			return eFailure;
		};
	}


	const auto cmdLines = std::array<DynArgv, 6> {
		DynArgv { "xor", "mux", "--key", "1234", "in.txt", "-k", "5678", "out.1.txt", "out.2.txt", "--key", "9abc" },
		DynArgv { "xor", "dmx", "in.txt", "out.1.txt", "out.2.txt", "-q" },
		DynArgv { "xor", "mux", "--key" },
		DynArgv { "xor", "mux" },
		DynArgv { "xor", "invalid subcommand" },
		DynArgv { "xor" } };

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	constexpr static auto optNone = xorinator::cli::OptionBits::eNone;
	constexpr static auto optQuiet = xorinator::cli::OptionBits::eQuiet;
	batch
	.run("Command line 0", mk_test_cmdln(cmdLines[0],
		"xor", CmdType::eMultiplex, { "1234", "5678", "9abc" }, "in.txt", { "out.1.txt", "out.2.txt" }, optNone))
	.run("Command line 1", mk_test_cmdln(cmdLines[1],
		"xor", CmdType::eDemultiplex, { }, "in.txt", { "out.1.txt", "out.2.txt" }, optQuiet))
	.run("Command line 2", mk_test_cmdln_except<xorinator::cli::InvalidCommandLineException>(cmdLines[2]))
	.run("Command line 3", mk_test_cmdln(cmdLines[3],
		"xor", CmdType::eMultiplex, { }, "", { }, optNone))
	.run("Command line 4", mk_test_cmdln(cmdLines[4],
		"xor", CmdType::eError, { }, "", { }, optNone))
	.run("Command line 5", mk_test_cmdln(cmdLines[5],
		"xor", CmdType::eNone, { }, "", { }, optNone));
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
