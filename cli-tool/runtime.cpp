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

#include <xorinator.hpp>

#include "runtime.hpp"

using xorinator::cli::CommandLine;
using xorinator::cli::CmdType;
using xorinator::cli::InvalidCommandLineException;
using xorinator::StaticVector;

using Rng = std::mt19937;
using RngKey = xorinator::RngKey<512>;



namespace {

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
		if(cmdln.firstArg.empty())
			throw std::runtime_error("invalid file \"\"");
		if(cmdln.variadicArgs.size() + cmdln.rngKeys.size() < 2)
			throw std::runtime_error("a (de)muxing operation needs two or more keys");
	}

}



namespace xorinator::runtime {

	bool runMux(const CommandLine& cmdln) {
		using xorinator::byte_t;

		checkPaths(cmdln);

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
			<< "   -k PASSPHRASE | --key PASSPHRASE\n"
			<< "   -q | --quiet\n"
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