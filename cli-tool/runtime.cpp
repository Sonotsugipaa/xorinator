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

using Rng = std::mt19937_64;
using RngKey = xorinator::RngKey<512>;



namespace {

	constexpr unsigned RNG_RESET_AFTER = 512; // Reset a RNG after X bytes


	#ifdef XORINATOR_UNIX_PERM_CHECK

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
				mode_t offset = 6;
				if(statResult.st_uid != prUid) {
					if(statResult.st_gid == prGid)
						offset = 3;
					else
						offset = 0;
				}
				if(! (statResult.st_mode & (rwxBit << offset))) {
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


	class RngAdapter {
	private:
		using rtype = Rng::result_type;
		using dtype = std::random_device::result_type;
		using byte_t = xorinator::byte_t;
		static_assert(0 == sizeof(rtype) % sizeof(byte_t));
		static constexpr unsigned rtype_bytes = sizeof(rtype) / sizeof(byte_t);
		static constexpr unsigned dtype_bytes = sizeof(rtype) / sizeof(byte_t);

		std::random_device rndDev_;
		Rng rng_;
		rtype rngState_;
		dtype rndDevState_;
		unsigned rngStateByteIndex_;
		unsigned rndDevByteIndex_;
		unsigned rngByteIndex_;

		dtype fwdDevRandom_() {
			static constexpr unsigned bits = std::numeric_limits<byte_t>::digits;
			if(rndDevByteIndex_ >= dtype_bytes) {
				rndDev_();
				rndDevByteIndex_ = 0;
			}
			auto r = byte_t(rngState_ >> rtype((rndDevByteIndex_++) * bits));
			return r;
		}

	public:
		static std::random_device mkRandomDevice() {
			return std::random_device();
		}

		RngAdapter():
				rndDev_(),
				rng_(rndDev_()),
				rngState_(rng_()),
				rngStateByteIndex_(0),
				rndDevByteIndex_(0),
				rngByteIndex_(0)
		{ }

		byte_t operator()() {
			static constexpr unsigned bits = std::numeric_limits<byte_t>::digits;
			if(rngStateByteIndex_ >= rtype_bytes) {
				if(rngByteIndex_ >= RNG_RESET_AFTER) {
					rng_ = Rng(fwdDevRandom_());
					rngByteIndex_ = 0;
				}
				rngState_ = rng_();
				rngStateByteIndex_ = 0;
			}
			auto r = byte_t(rngState_ >> rtype((rngStateByteIndex_++) * bits));
			++rngByteIndex_;
			return r;
		}
	};


	template<typename uint_t>
	uint_t random(RngAdapter& rng) {
		using byte_t = xorinator::byte_t;
		static_assert(std::numeric_limits<uint_t>::is_integer);
		static_assert(! std::numeric_limits<uint_t>::is_signed);
		uint_t r = 0;
		for(unsigned i=0; i < (sizeof(uint_t) / sizeof(byte_t)); ++i) {
			r = r | (rng() << (i * std::numeric_limits<byte_t>::digits));
		}
		return r;
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
		auto rng = std::mt19937(hash);
		for(RngKey::word_t& word : key) {
			word = rng();
		}
		return RngKey(key);
	}


	/** Check non-fatal semantic errors. */
	void checkArgumentUsage(const CommandLine& cmdln) {
		static constexpr std::string_view pre = "Warning: ";
		if(cmdln.options & xorinator::cli::OptionBits::eQuiet) return;
		if((cmdln.cmdType != CmdType::eMultiplex) && (cmdln.litterSize != 0)) {
			std::cerr << pre << "the \"--litter\" argument has no effect for this subcommand." << std::endl;
		}
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
		if(cmdln.variadicArgs.empty()) {
			if(cmdln.cmdType == CmdType::eMultiplex)
				throw CmdlnException("a multiplexing operation needs one or more output files");
			if(cmdln.cmdType == CmdType::eDemultiplex)
				throw CmdlnException("a demultiplexing operation needs one or more input files");
		}
		if((! (cmdln.options & xorinator::cli::OptionBits::eForce)) && (
				(cmdln.cmdType == CmdType::eMultiplex) || (cmdln.cmdType == CmdType::eDemultiplex)
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

		#ifdef XORINATOR_UNIX_PERM_CHECK
			if(! (cmdln.options & cli::OptionBits::eForce)) {
				checkFilePermission<04>(cmdln.firstArg);
				for(const auto& file : cmdln.variadicArgs) {
					checkFilePermission<02>(file); }
			}
		#endif

		checkPaths(cmdln);
		checkArgumentUsage(cmdln);

		auto rndDev = std::random_device();
		auto muxIn = VirtualInputStream(cmdln.firstArg);
		auto muxOut = StaticVector<VirtualOutputStream>(cmdln.variadicArgs.size());
		auto outputBuffer = StaticVector<byte_t>(muxOut.size());
		auto rngKeys = StaticVector<::RngKey>(cmdln.rngKeys.size());
		auto rngKeyViews = StaticVector<::RngKey::View>(rngKeys.size());
		auto rngKeyIterators = StaticVector<::RngKey::View::Iterator>(rngKeyViews.size());
		RngAdapter rng;

		for(size_t i=0; const std::string& key : cmdln.rngKeys) {
			rngKeys[i] = keyFromGenerator(key);
			rngKeyViews[i] = rngKeys[i].view(0);
			rngKeyIterators[i].~Iterator(); // Much like Thanos, this is inevitable. Hopefully this can and does get optimized away.
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

		if(cmdln.litterSize > 0) {
			size_t noLitterIndex = random<size_t>(rng) % muxOut.size();
			for(size_t i=0; auto& output : muxOut) {
				using lit_t = decltype(cmdln.litterSize);
				if((i++) != noLitterIndex) {
					lit_t litterSize = random<lit_t>(rng) % cmdln.litterSize;
					for(lit_t i=0; i < litterSize; ++i) {
						output.get().put(rng());
					}
				}
			}
		}

		for(auto& output : muxOut) {
			output.get().flush(); }

		return true;
	}


	bool runDemux(const CommandLine& cmdln) {
		using xorinator::byte_t;

		#ifdef XORINATOR_UNIX_PERM_CHECK
			if(! (cmdln.options & cli::OptionBits::eForce)) {
				checkFilePermission<02>(cmdln.firstArg);
				for(const auto& file : cmdln.variadicArgs) {
					checkFilePermission<04>(file); }
			}
		#endif

		checkPaths(cmdln);
		checkArgumentUsage(cmdln);

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
		while([&]() {
			byte_t xorSum = 0;
			for(auto& input : demuxIn) {
				if(input.get().get(inputChar)) {
					xorSum = byte_t(inputChar) ^ xorSum;
				} else {
					return false;
				}
			}
			for(auto& keyIter : rngKeyIterators) {
				xorSum = xorSum ^ *keyIter;
				++keyIter;
			}
			demuxOut.get().put(xorSum);
			return true;
		} ());
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
			<< "   -g NUM | --litter NUM  (add red herring bytes when generating one-time pads)\n"
			<< '\n'
			<< "Aliases for \"multiplex\": mux, m\n"
			<< "Aliases for \"demultiplex\": demux, dmx, d" << std::endl;
		return false;
	}


	bool run(const CommandLine& cmdln) {
		using namespace std::string_literals;
		switch(cmdln.cmdType) {
			case CmdType::eMultiplex:
				return runMux(cmdln);
			case CmdType::eDemultiplex:
				return runDemux(cmdln);
			case CmdType::eNone:
				return usage(cmdln);
			case CmdType::eError:  default:
				throw InvalidCommandLineException("invalid subcommand"s);
		}
	}

}
