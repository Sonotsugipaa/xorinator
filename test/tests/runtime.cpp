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
#include <cli-tool/runtime.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <array>



namespace {

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;


	const std::string srcPath = "deterministic-msg.txt";
	const std::string srcCpPath = "deterministic-msg.demux.txt";
	const std::string otpNoGenPath = "run-tests.sh";
	const std::string otpDstPath0 = "deterministic-msg.1.xor";
	const std::string otpDstPath1 = "deterministic-msg.2.xor";
	const std::string message = "rcompat\n";
	const std::string expectedExceptionMsg = "Expected an exception, none thrown";


	bool mkFile(std::ostream& os, const std::string& path, const std::string& content) {
		try {
			auto file = std::ofstream(path);
			file.exceptions(std::ios_base::badbit);
			file << content;
			return true;
		} catch(std::exception& ex) {
			os << "Could not write to \"" << path << "\": " << ex.what() << std::endl;
		}
		return false;
	};


	bool cmpFile(std::ostream& os, const std::string& path, const std::string& expContent) {
		try {
			auto file = std::ifstream(path);
			file.exceptions(std::ios_base::badbit);
			std::string content;
			char c;
			while(file.get(c)) content.push_back(c);
			if(content.size() != expContent.size()) {
				os
					<< "File \"" << path << "\" does not have the expected content (size mismatch, "
					<< content.size() << " vs " << expContent.size() << ')' << std::endl;
				return false;
			}
			std::make_signed_t<size_t> mismatchAt = -1;
			for(size_t i=0; i < content.size(); ++i) {
				if(content[i] != expContent[i]) {
					mismatchAt = i;
					break;
				}
			}
			if(mismatchAt >= 0) {
				os << "File \"" << path << "\" does not have the expected content (mismatch at " << mismatchAt << ")" << std::endl;
				return false;
			}
		} catch(std::exception& ex) {
			os << "Could not read from \"" << path << "\": " << ex.what() << std::endl;
		}
		return true;
	};


	bool cmpFiles(std::ostream& os, const std::string& pathExpect, const std::string& pathResult) {
		try {
			auto file0 = std::ifstream(pathExpect);
			auto file1 = std::ifstream(pathResult);
			file0.exceptions(std::ios_base::badbit);
			file1.exceptions(std::ios_base::badbit);
			char c0, c1;

			// Check size
			file0.seekg(0, std::ios_base::end);
			file1.seekg(0, std::ios_base::end);
			if(file0.tellg() != file1.tellg()) {
				os << "File \"" << pathResult << "\": size mismatch with \"" << pathExpect << '"' << std::endl;
				return false;
			}
			file0.seekg(0, std::ios_base::beg);
			file1.seekg(0, std::ios_base::beg);

			while(file0.get(c0) && file1.get(c1)) {
				if(c0 != c1) {
					os << "File \"" << pathResult << "\" does not have the expected content" << std::endl;
					return false;
				}
			}
			return true;
		} catch(std::exception& ex) {
			os << "Could not read from \"" << pathExpect << "\" and \"" << pathResult << "\": " << ex.what() << std::endl;
		}
		return false;
	};


	/** Expect a command with only one pad, either for multiplexing or demultiplexing, to fail. */
	template<bool muxNotDemux>
	utest::ResultType test_not_enough_pads(std::ostream& os) {
		using xorinator::cli::CommandLine;
		try {
			if constexpr(muxNotDemux) {
				std::array<const char*, 4> argv = { "xor", "mux", srcPath.c_str(), otpDstPath0.c_str() };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			} else {
				std::array<const char*, 4> argv = { "xor", "dmx", srcPath.c_str(), otpDstPath0.c_str() };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			}
		} catch(xorinator::cli::InvalidCommandLineException& ex) {
			return eSuccess;
		} catch(std::exception& ex) {
			os << "std::exception: " << ex.what() << std::endl;
		} catch(...) {
			os << "An unknown exception was thrown" << std::endl;
		}
		os << expectedExceptionMsg << std::endl;
		return eFailure;
	}


	/** Expect two files with different sizes to demux into a file with
	 * the smallest input file's size. */
	utest::ResultType test_demux_diff_sizes(std::ostream& os) {
		using xorinator::cli::CommandLine;
		try {
			{ // Create the files
				if(! mkFile(os, otpDstPath0, "abcdefgh"))  return utest::ResultType::eNeutral;
				if(! mkFile(os, otpDstPath1, "zyxwvuts_123"))  return utest::ResultType::eNeutral;
			} { // Run the demultiplex subcommand
				std::array<const char*, 5> argv = { "xor", "dmx", srcCpPath.c_str(), otpDstPath0.c_str(), otpDstPath1.c_str() };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			} { // Compare the result
				static const std::string hardcodedExpect = {
					char(0x1b),  char(0x1b),  char(0x1b),  char(0x13),
					char(0x13),  char(0x13),  char(0x13),  char(0x1b) };
				if(! cmpFile(os, srcCpPath, hardcodedExpect))  return eFailure;
			}
		} catch(std::exception& ex) {
			os << "Exception: " << ex.what() << std::endl;
			return eFailure;
		}
		return eSuccess;
	}


	/** Expect a command with only one non-option argument to fail. */
	template<bool muxNotDemux>
	utest::ResultType test_no_pad(std::ostream& os) {
		using xorinator::cli::CommandLine;
		try {
			if constexpr(muxNotDemux) {
				std::array<const char*, 5> argv = { "xor", "mux", srcPath.c_str(), "-k1234", "-k5678" };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			} else {
				std::array<const char*, 5> argv = { "xor", "dmx", srcPath.c_str(), "-k1234", "-k5678" };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			}
		} catch(xorinator::cli::InvalidCommandLineException& ex) {
			return eSuccess;
		} catch(std::exception& ex) {
			os << "std::exception: " << ex.what() << std::endl;
		} catch(...) {
			os << "An unknown exception was thrown" << std::endl;
		}
		os << expectedExceptionMsg << std::endl;
		return eFailure;
	}


	/** Expect a demultiplexing operation to run successfully and produce the
	 * correct output when using --key arguments. This test should fail if
	 * the deterministic algorithm for generating one-time pads from strings
	 * changes, therefore breaking retrocompatibility, therefore causing
	 * previously multiplexed files to be lost. */
	utest::ResultType test_keys_demux(std::ostream& os) {
		using xorinator::cli::CommandLine;
		try {
			{ // Create the files
				if(! mkFile(os, srcCpPath, message))  return utest::ResultType::eNeutral;
				if(! mkFile(os, otpDstPath0, "abcdefgh"))  return utest::ResultType::eNeutral;
			} { // Run the multiplex subcommand
				std::array<const char*, 6> argv = { "xor", "dmx", "-k1234", "-klaks", srcCpPath.c_str(), otpDstPath0.c_str()};
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			} { // Compare the result    a208 7f37 2d29 71de
				static const std::string hardcodedExpect = {
					char(0xa2),  char(0x08),  char(0x7f),  char(0x37),
					char(0x2d),  char(0x29),  char(0x71),  char(0xde) };
				if(! cmpFile(os, srcCpPath, hardcodedExpect))  return eFailure;
			}
		} catch(std::exception& ex) {
			os << "Exception: " << ex.what() << std::endl;
			return eFailure;
		}
		return eSuccess;
	}


	/** Expect a file to be multiplexed, then demultiplexed,
	 * finally ending up with an exact copy of itself. */
	template<size_t litter, bool nogen>
	utest::ResultType test_mux_demux(std::ostream& os) {
		using xorinator::cli::CommandLine;
		try {
			{ // Create the file
				if(! mkFile(os, srcPath, message))  return utest::ResultType::eNeutral;
			} { // Run the multiplex subcommand
				const std::string& firstOtp = (nogen? ("-G"+otpNoGenPath) : otpDstPath0);
				if constexpr(litter == 0) {
					std::array<const char*, 5> argv = { "xor", "mux", srcPath.c_str(), firstOtp.c_str(), otpDstPath1.c_str() };
					if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
						return eFailure;
					}
				} else {
					std::string litterArg = "--litter=" + std::to_string(litter);
					std::array<const char*, 6> argv = { "xor", "mux", litterArg.c_str(), srcPath.c_str(), otpDstPath0.c_str(), otpDstPath1.c_str() };
					if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
						return eFailure;
					}
				}
			} { // Run the demultiplex subcommand
				const std::string& firstOtp = (nogen? otpNoGenPath : otpDstPath0);
				std::array<const char*, 5> argv = { "xor", "dmx", srcCpPath.c_str(), firstOtp.c_str(), otpDstPath1.c_str() };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			} { // Compare the files
				if(! cmpFiles(os, srcPath, srcCpPath))  return eFailure;
			}
		} catch(std::exception& ex) {
			os << "Exception: " << ex.what() << std::endl;
			return eFailure;
		}
		return eSuccess;
	}


	/** Expect a demultiplexing operation to match a hardcoded result: this test
	 * should fail if retrocompatibility is broken (even if multiplexing and
	 * demultiplexing operations are literally just bitwise XOR operations). */
	utest::ResultType test_pads_demux(std::ostream& os) {
		using xorinator::cli::CommandLine;
		try {
			{ // Create the files
				if(! mkFile(os, otpDstPath0, "abcdefgh"))  return utest::ResultType::eNeutral;
				if(! mkFile(os, otpDstPath1, "zyxwvuts"))  return utest::ResultType::eNeutral;
			} { // Run the demultiplex subcommand
				std::array<const char*, 5> argv = { "xor", "dmx", srcCpPath.c_str(), otpDstPath0.c_str(), otpDstPath1.c_str() };
				if(! xorinator::runtime::run(CommandLine(argv.size(), argv.data()))) {
					return eFailure;
				}
			} { // Compare the result
				static const std::string hardcodedExpect = {
					char(0x1b),  char(0x1b),  char(0x1b),  char(0x13),
					char(0x13),  char(0x13),  char(0x13),  char(0x1b) };
				if(! cmpFile(os, srcCpPath, hardcodedExpect))  return eFailure;
			}
		} catch(std::exception& ex) {
			os << "Exception: " << ex.what() << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	batch
		.run("Demux consistency (for pads)", test_pads_demux)
		.run("Demux consistency (for keys)", test_keys_demux)
		.run("Demux with differently sized inputs", test_demux_diff_sizes)
		.run("Not enough outputs (mux)", test_not_enough_pads<true>)
		.run("Not enough outputs (demux)", test_not_enough_pads<false>)
		.run("No output (mux)", test_no_pad<true>)
		.run("No output (demux)", test_no_pad<false>)
		.run("Mux & demux", test_mux_demux<0, false>)
		.run("Mux & demux (--litter=64)", test_mux_demux<64, false>)
		.run("Mux & demux (nogen)", test_mux_demux<0, true>);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
