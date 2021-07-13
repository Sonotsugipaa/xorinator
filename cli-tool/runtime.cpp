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

#include <iostream>
#include <random>
#include <cassert>
#include <unordered_set>

#ifdef XORINATOR_UNIX_PERM_CHECK
	#include <cerrno>
	extern "C" {
		#include <sys/stat.h>
		#include <unistd.h>
	}
#endif

#include <xorinator.hpp>

#include "runtime.hpp"

using xorinator::cli::CommandLine;
using xorinator::cli::CmdType;
using xorinator::cli::InvalidCommandLineException;
using xorinator::StaticVector;

using Rng = std::mt19937;
using RngKey = xorinator::RngKey<512>;



namespace {

	#ifdef XORINATOR_UNIX_PERM_CHECK

		#ifndef S_IRUSR // This is to make VS Code shut up about undefined S_IR*** macros
			#define S_IRUSR 0400
			#define S_IRGRP 0040
			#define S_IROTH 0004
		#endif

		template<mode_t rwxBit>
		std::string_view rwxString;

		template<> std::string_view rwxString<01> = "execute";
		template<> std::string_view rwxString<02> = "write";
		template<> std::string_view rwxString<04> = "read";

		template<mode_t rwxBit>
		void checkFilePermission(const std::string& path) {
			static_assert(S_IRUSR == 0400);
			static_assert(S_IRGRP == 0040);
			static_assert(S_IROTH == 0004);
			static_assert((rwxBit == 01) || (rwxBit == 02) | (rwxBit == 04));
			if(path == "-") return; // This may need to be removed in the future, but for now every file named "-" is stdin/stdout
			struct stat statResult;
			if(0 == stat(path.c_str(), &statResult)) {
				if(S_ISDIR(statResult.st_mode)) {
					throw xorinator::runtime::FilePermissionException(
						'"'+path+"\" is an existing directory");
				}
				uid_t prUid = geteuid();
				gid_t prGid = getegid();
				mode_t offset = 0;
				if(statResult.st_uid != prUid) {
					if(statResult.st_gid == prGid)
						offset = 3;
					else
						offset = 6;
				}
				if(! (statResult.st_mode & (04 << offset))) {
					throw xorinator::runtime::FilePermissionException(
						"user doesn't have " + std::string(rwxString<rwxBit>) +
						" permissions for \"" + path + '"');
				}
			} else {
				switch(errno) {
					case ENOENT:  return;
					default:
						throw xorinator::runtime::FilePermissionException(
							"could not determine permissions for file \""+path+'"');
				}
			}
		}

	#endif


	Rng mk_rng() {
		static auto rndDev = std::random_device();
		Rng rng = Rng(rndDev());
		return rng;
	}

	template<typename uint_t>
	uint_t random(Rng& rng) {
		static_assert(std::numeric_limits<uint_t>::is_integer);
		static_assert(! std::numeric_limits<uint_t>::is_signed);
		return rng();
	}


	/** Creates a deterministic number sequence from an arbitrary string,
	 * by hashing it with a simple algorithm involving xor operations
	 * and linear congruential RNGs. */
	RngKey keyFromGenerator(const std::string& gen) {
		RngKey::word_t hash = 0;
		std::array<RngKey::word_t, RngKey::word_count> key;
		for(std::string::size_type i=0; i < gen.size(); ++i) {
			hash = hash ^ std::minstd_rand(gen[i] ^ i)();
		}
		Rng rng = Rng(hash);
		for(RngKey::word_t& word : key) {
			word = rng();
		}
		return RngKey(key);
	}


	void checkPaths(const CommandLine& cmdln) {
		using xorinator::cli::CmdType;
		using CmdlnException = xorinator::cli::InvalidCommandLineException;
		if(cmdln.firstArg.empty())
			throw CmdlnException("invalid file \"\"");
		if(cmdln.variadicArgs.size() + cmdln.rngKeys.size() < 2) {
			if(cmdln.cmdType == CmdType::eMultiplex)
				throw CmdlnException("a multiplexing operation needs two or more keys");
			if(cmdln.cmdType == CmdType::eDemultiplex)
				throw CmdlnException("a demultiplexing operation needs two or more keys");
		}
		if((! (cmdln.options & xorinator::cli::OptionBits::eForce)) && (
				(cmdln.cmdType == CmdType::eMultiplex) ||
				(cmdln.cmdType == CmdType::eDemultiplex)
		)) {
			auto paths = std::unordered_set<std::string>(cmdln.variadicArgs.size());
			paths.insert(cmdln.firstArg);
			for(size_t i=0; i < cmdln.variadicArgs.size(); ++i) {
				if(! paths.insert(cmdln.variadicArgs[i]).second)
					throw CmdlnException("file arguments must be unique");
			}
		}
	}

}



namespace xorinator::runtime {

	bool runMux(const CommandLine& cmdln) {
		using xorinator::byte_t;

		checkPaths(cmdln);

		#ifdef XORINATOR_UNIX_PERM_CHECK
			if(! (cmdln.options & cli::OptionBits::eForce)) {
				checkFilePermission<04>(cmdln.firstArg);
				for(const auto& file : cmdln.variadicArgs) {
					checkFilePermission<02>(file); }
			}
		#endif

		auto muxIn = VirtualInputStream(cmdln.firstArg);
		auto muxOut = StaticVector<VirtualOutputStream>(cmdln.variadicArgs.size());
		auto outputBuffer = StaticVector<byte_t>(muxOut.size());
		auto rngKeys = StaticVector<::RngKey>(cmdln.rngKeys.size());
		auto rngKeyViews = StaticVector<::RngKey::View>(rngKeys.size());
		auto rngKeyIterators = StaticVector<::RngKey::View::Iterator>(rngKeyViews.size());
		auto rng = mk_rng();

		for(size_t i=0; const std::string& key : cmdln.rngKeys) {
			rngKeys[i] = keyFromGenerator(key);
			rngKeyViews[i] = rngKeys[i].view(0);
			/* The next line is a bit of a hack: an iterator has no copy
			 * assignment operator, but it needs to be initialized somehow;
			 * unfortunately, the StaticVector initializes its content in the
			 * first place through the default constructor.
			 * Technically, placement new initialization is illegal here,
			 * but the default constructor for Iterator zero-initializes
			 * its members. The only dubious case is "generators_", a
			 * unique_ptr, but initializing it with nullptr should make it
			 * "overridable". A proper way of doing this must be investigated. */
			new (&rngKeyIterators[i]) ::RngKey::View::Iterator(rngKeyViews[i].begin());
			++i;
		}

		muxIn.get().exceptions(std::ios_base::badbit);
		for(unsigned i=0; const std::string& path : cmdln.variadicArgs) {
			muxOut[i] = path;
			muxOut[i].get().exceptions(std::ios_base::badbit);
			++i;
		}

		char inputChar;
		while(muxIn.get().get(inputChar)) {
			byte_t xorSum = 0;
			for(size_t i=1; i < muxOut.size(); ++i) {
				outputBuffer[i] = random<byte_t>(rng);
				xorSum = xorSum ^ outputBuffer[i];
			}
			for(auto& keyIter : rngKeyIterators) {
				xorSum = xorSum ^ *keyIter;
				++keyIter;
			}
			outputBuffer[0] = byte_t(inputChar) ^ xorSum;
			for(size_t i=0; auto& output : muxOut) {
				output.get().put(outputBuffer[i++]); }
		}
		for(auto& output : muxOut) {
			output.get().flush(); }

		return true;
	}


	bool runDemux(const CommandLine& cmdln) {
		using xorinator::byte_t;

		checkPaths(cmdln);

		#ifdef XORINATOR_UNIX_PERM_CHECK
			if(! (cmdln.options & cli::OptionBits::eForce)) {
				checkFilePermission<02>(cmdln.firstArg);
				for(const auto& file : cmdln.variadicArgs) {
					checkFilePermission<04>(file); }
			}
		#endif

		auto demuxOut = VirtualOutputStream(cmdln.firstArg);
		auto demuxIn = StaticVector<VirtualInputStream>(cmdln.variadicArgs.size());
		auto rngKeys = StaticVector<::RngKey>(cmdln.rngKeys.size());
		auto rngKeyViews = StaticVector<::RngKey::View>(rngKeys.size());
		auto rngKeyIterators = StaticVector<::RngKey::View::Iterator>(rngKeyViews.size());

		for(size_t i=0; const std::string& key : cmdln.rngKeys) {
			rngKeys[i] = keyFromGenerator(key);
			rngKeyViews[i] = rngKeys[i].view(0);
			/* The next line is a bit of a hack: see the similar situation in
			 * ::runMux. */
			new (&rngKeyIterators[i]) ::RngKey::View::Iterator(rngKeyViews[i].begin());
			++i;
		}

		demuxOut.get().exceptions(std::ios_base::badbit);
		for(size_t i=0; const std::string& path : cmdln.variadicArgs) {
			demuxIn[i] = path;
			demuxIn[i].get().exceptions(std::ios_base::badbit);
			++i;
		}

		char inputChar;
		decltype(demuxIn)::SizeType openInputs = demuxIn.size();
		while(true /* openInputs > 0 */) {
			openInputs = 0;
			byte_t xorSum = 0;
			for(auto& input : demuxIn) {
				if(input.get().get(inputChar)) {
					xorSum = byte_t(inputChar) ^ xorSum;
					++openInputs;
				}
			}
			if(openInputs > 0) {
				for(auto& keyIter : rngKeyIterators) {
					xorSum = xorSum ^ *keyIter;
					++keyIter;
				}
				demuxOut.get().put(xorSum);
			} else {
				break;
			}
		}
		demuxOut.get().flush();

		return true;
	}


	bool usage(const CommandLine& cmdln) {
		static constexpr auto strNeedsQuotes = [](const std::string& str) {
			static constexpr auto charIsAllowed = [](char c) {
				#define TEST_CHAR(COND_) if(COND_)  return true;
					TEST_CHAR(c >= 'a' && c <= 'z')
					TEST_CHAR(c >= 'A' && c <= 'Z')
					TEST_CHAR(c >= '-' && c <= '9')
					TEST_CHAR(c == '_')
				#undef TEST_CHAR
				return false;
			};
			for(char c : str) {
				if(! charIsAllowed(c)) { return true; }
			}
			return false;
		};
		std::string zeroArg = strNeedsQuotes(cmdln.zeroArg)?
			('"' + cmdln.zeroArg + '"') :
			(cmdln.zeroArg);
		std::cerr << "Usage:\n"
			<< "   " << zeroArg << " multiplex [OPTIONS] [--] FILE_IN FILE_OUT [FILE_OUT...]\n"
			<< "   " << zeroArg << " demultiplex [OPTIONS] [--] FILE_OUT FILE_IN [FILE_IN...]\n"
			<< "   " << zeroArg << " help | ?\n"
			<< '\n'
			<< "Options:\n"
			<< "   -k PASSPHRASE | --key PASSPHRASE  (add a RNG as a one-time pad)\n"
			<< "   -q | --quiet  (suppress error messages)\n"
			<< "   -f | --force  (skip permission checks)\n"
			<< '\n'
			<< "Aliases for \"multiplex\": mux, m\n"
			<< "Aliases for \"demultiplex\": demux, dmx, d\n" << std::endl;
		return false;
	}


	bool run(const CommandLine& cmdln) {
		using namespace std::string_literals;
		switch(cmdln.cmdType) {
			case CmdType::eMultiplex:  return runMux(cmdln);
			case CmdType::eDemultiplex:  return runDemux(cmdln);
			case CmdType::eNone:  return usage(cmdln);
			case CmdType::eError:  default:
				throw InvalidCommandLineException("invalid subcommand"s);
		}
	}

}