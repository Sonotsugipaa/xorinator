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
				std::vector<std::string> cmdLnRngKeysDynV;
				std::vector<std::string> cmdLnVarargsDynV;
				cmdLnRngKeysDynV.insert(cmdLnRngKeysDynV.begin(), cmdln.rngKeys.begin(), cmdln.rngKeys.end());
				cmdLnVarargsDynV.insert(cmdLnVarargsDynV.begin(), cmdln.variadicArgs.begin(), cmdln.variadicArgs.end());
				bool success = true;
				#define CHECK_(EXPECT_, GOT_, MSG_) if(! (EXPECT_ == GOT_)) { os << MSG_ << " mismatch\n"; success = false; }
				#define CHECK_PR_(EXPECT_, GOT_, MSG_) if(! (EXPECT_ == GOT_)) { os << MSG_ << " mismatch (expected '" << (EXPECT_) << "', got '" << (GOT_) << "')\n"; success = false; }
					CHECK_   (cmdType, cmdln.cmdType,        "command type");
					CHECK_   (opts, cmdln.options,           "options");
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
				return eSuccess;
			} catch(std::exception& ex) {
				os << "Exception: " << ex.what() << std::endl;
			} catch(...) {
				os << "Exception (unhandled type)";
			}
			return eFailure;
		};
	}


	template<bool useLiteral>
	auto test_literal_pos(std::ostream& os) {
		using namespace xorinator;
		try {
			if constexpr(useLiteral) {
				constexpr unsigned expect = 1;
				auto argv = std::array<const char*, 6> { "xor", "mux", "arg0", "-f", "--", "-f" };
				auto cmdln = cli::CommandLine(argv.size(), argv.data());
				if(cmdln.firstLiteralArg != 1) {
					os
						<< "first literal argument is " << cmdln.firstLiteralArg
						<< ", but it should be " << expect << '\n';
					return eFailure;
				}
			} else {
				auto argv = std::array<const char*, 5> { "xor", "mux", "arg0", "-f", "arg" };
				auto cmdln = cli::CommandLine(argv.size(), argv.data());
				unsigned expectHigherThan = argv.size();
				if(cmdln.firstLiteralArg <= expectHigherThan) {
					os
						<< "first literal argument is " << cmdln.firstLiteralArg
						<< ", but it should be > " << expectHigherThan << '\n';
					return eFailure;
				}
			}
			os << std::flush;
			return eSuccess;
		} catch(std::exception& ex) {
			os << "Exception: " << ex.what() << std::endl;
			return eFailure;
		}
	}


	auto test_literal_cmd(std::ostream& os) {
		using namespace xorinator;
		try {
			auto argv = std::array<const char*, 6> { "xor", "mux", "arg0", "-f", "--", "-f" };
			auto cmdln = cli::CommandLine(argv.size(), argv.data());
			std::vector<std::string> cmdLnRngKeysDynV;
			std::vector<std::string> cmdLnVarargsDynV;
			cmdLnRngKeysDynV.insert(cmdLnRngKeysDynV.begin(), cmdln.rngKeys.begin(), cmdln.rngKeys.end());
			cmdLnVarargsDynV.insert(cmdLnVarargsDynV.begin(), cmdln.variadicArgs.begin(), cmdln.variadicArgs.end());
			bool success = true;
			#define CHECK_(EXPECT_, GOT_, MSG_) if(! (EXPECT_ == GOT_)) { os << MSG_ << " mismatch\n"; success = false; }
			#define CHECK_PR_(EXPECT_, GOT_, MSG_) if(! (EXPECT_ == GOT_)) { os << MSG_ << " mismatch (expected '" << (EXPECT_) << "', got '" << (GOT_) << "')\n"; success = false; }
				CHECK_   (cli::CmdType::eMultiplex,           cmdln.cmdType,     "command type");
				CHECK_   (cli::OptionBits::eForce,            cmdln.options,     "options");
				CHECK_   (std::vector<std::string>(),         cmdLnRngKeysDynV,  "rng keys");
				CHECK_PR_("xor",                              cmdln.zeroArg,     "zero argument");
				CHECK_PR_("arg0",                             cmdln.firstArg,    "first argument");
				CHECK_   (std::vector<std::string> { "-f" },  cmdLnVarargsDynV,  "variadic arguments");
			#undef CHECK_
			os << std::flush;
			return success? eSuccess : eFailure;
		} catch(std::exception& ex) {
			os << "Exception: " << ex.what() << std::endl;
			return eFailure;
		}
	}


	const auto cmdLines = std::array<DynArgv, 11> {
		DynArgv { "xor", "mux", "--key", "1234", "in.txt", "-k", "5678", "out.1.txt", "out.2.txt", "--key", "9abc" },
		DynArgv { "xor", "dmx", "in.txt", "out.1.txt", "out.2.txt", "-q" },
		DynArgv { "xor", "dmx", "-fq"},
		DynArgv { "xor", "dmx", "--key=abc"},
		DynArgv { "xor", "dmx", "-kabc"},
		DynArgv { "xor", "mux", "--invalid-option" },
		DynArgv { "xor", "mux", "-fqk" },
		DynArgv { "xor", "mux", "-fqinvald" },
		DynArgv { "xor", "mux" },
		DynArgv { "xor", "invalid subcommand" },
		DynArgv { "xor" } };

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	constexpr static auto optNone = xorinator::cli::OptionBits::eNone;
	constexpr static auto optQuiet = xorinator::cli::OptionBits::eQuiet;
	constexpr static auto optForce = xorinator::cli::OptionBits::eForce;
	batch
		.run("Command with literal argument marker (syntax)", test_literal_cmd)
		.run("Command with literal argument marker (first argument)", test_literal_pos<true>)
		.run("Command with literal argument marker (absent)", test_literal_pos<false>)
		.run("Multiple arguments, --key options", mk_test_cmdln(cmdLines[0],
			"xor", CmdType::eMultiplex, { "1234", "5678", "9abc" }, "in.txt", { "out.1.txt", "out.2.txt" }, optNone))
		.run("Multiple arguments, -q option", mk_test_cmdln(cmdLines[1],
			"xor", CmdType::eDemultiplex, { }, "in.txt", { "out.1.txt", "out.2.txt" }, optQuiet))
		.run("No argument, conflated -q and -f options", mk_test_cmdln(cmdLines[2],
			"xor", CmdType::eDemultiplex, { }, { }, { }, optQuiet | optForce))
		.run("No argument, --key=abc option", mk_test_cmdln(cmdLines[3],
			"xor", CmdType::eDemultiplex, { "abc" }, { }, { }, optNone))
		.run("No argument, -kabc option", mk_test_cmdln(cmdLines[4],
			"xor", CmdType::eDemultiplex, { "abc" }, { }, { }, optNone))
		.run("No argument, long invalid option (fail)", mk_test_cmdln_except<xorinator::cli::InvalidCommandLineException>(cmdLines[5]))
		.run("No argument, conflated -f, -q and -k option (fail)", mk_test_cmdln_except<xorinator::cli::InvalidCommandLineException>(cmdLines[6]))
		.run("No argument, multiple invalid conflated options (fail)", mk_test_cmdln_except<xorinator::cli::InvalidCommandLineException>(cmdLines[7]))
		.run("No argument nor option", mk_test_cmdln(cmdLines[8],
			"xor", CmdType::eMultiplex, { }, "", { }, optNone))
		.run("Unrecognized subcommand", mk_test_cmdln(cmdLines[9],
			"xor", CmdType::eError, { }, "", { }, optNone))
		.run("Nothing", mk_test_cmdln(cmdLines[10],
			"xor", CmdType::eNone, { }, "", { }, optNone));
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
