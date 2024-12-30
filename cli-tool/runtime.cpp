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
#include <cstring>
#include <cassert>
#include <concepts>
#include <array>
#include <unordered_set>
#include <bit>

#include "runtime.hpp"

using xorinator::cli::CommandLine;
using xorinator::cli::CmdType;
using xorinator::cli::InvalidCommandLineException;
using xorinator::StaticVector;



namespace {

	constexpr size_t RNG_BLOCK = 4096 * sizeof(std::random_device::result_type);


	/** A std::ifstream / std::ofstream wrapper, that replaces the stream when
	 * constructed from the path "-". */
	template<typename stream_t, typename file_stream_t>
	class StreamAdapter {
	private:
		constexpr static bool isInputStream = std::is_same_v<stream_t, std::istream>;
		constexpr static bool isOutputStream = std::is_same_v<stream_t, std::ostream>;
		static_assert(isInputStream || isOutputStream);
		static_assert((! isInputStream) || std::is_same_v<file_stream_t, std::ifstream>);
		static_assert((! isOutputStream) || std::is_same_v<file_stream_t, std::ofstream>);

		stream_t* stream_;
		bool preallocated_;

	public:
		using Native = stream_t;

		StreamAdapter(): stream_(nullptr), preallocated_(false) { }

		StreamAdapter(stream_t& ref): stream_(&ref), preallocated_(true) { }

		StreamAdapter(const std::string& path, bool noStdIo) {
			if(noStdIo || (path != "-")) {
				preallocated_ = false;
				stream_ = new file_stream_t(path, std::ios_base::binary);
			} else {
				preallocated_ = true;
				if constexpr(isInputStream) { stream_ = &std::cin; }
				else if constexpr(isOutputStream) { stream_ = &std::cout; }
				else { assert(false && "this assertion shouldn't ever be reachable"); }
			}
		}

		~StreamAdapter() {
			if(stream_ != nullptr) {
				if(! preallocated_)  delete stream_;
				stream_ = nullptr;
			}
		}

		StreamAdapter(StreamAdapter&& mv):
				stream_(std::move(mv.stream_)),
				preallocated_(std::move(mv.preallocated_))
		{
			mv.stream_ = nullptr;
		}

		StreamAdapter& operator=(StreamAdapter&& mv) {
			this->~StreamAdapter();
			new (this) StreamAdapter(std::move(mv));
			return *this;
		}

		stream_t& get() { return *stream_; }
		const stream_t& get() const { return *stream_; }

		stream_t& operator*() { return get(); }
		const stream_t& operator*() const { return get(); }

		stream_t& operator->() { return get(); }
		const stream_t& operator->() const { return get(); }

		operator stream_t&() { return get(); }
		operator const stream_t&() const { return get(); }
	};

	/** A std::ifstream wrapper, that replaces the stream when
	 * constructed from the path "-". */
	using InputStreamAdapter = StreamAdapter<std::istream, std::ifstream>;

	/** A std::ofstream wrapper, that replaces the stream when
	 * constructed from the path "-". */
	using OutputStreamAdapter = StreamAdapter<std::ostream, std::ofstream>;


	#ifdef XORINATOR_DEV_RANDOM
	class RngAdapter {
	public:
		using byte_t = xorinator::byte_t;

		RngAdapter():
			byteIndex_(0),
			bufferSize_(0),
			byteBuffer_(std::make_unique_for_overwrite<byte_t[]>(RNG_BLOCK)),
			hwRng_("/dev/random")
		{ }

		void fillBufferTo(size_t neededSize) {
			assert(neededSize <= RNG_BLOCK);
			if(neededSize > (RNG_BLOCK - bufferSize_ /* available space */)) [[unlikely]] {
				// Discarding bytes to make space is easier
				bufferSize_ = 0;
				byteIndex_ = 0;
			}
			if((bufferSize_ - byteIndex_ /* unused buffered bytes */) < neededSize) [[unlikely]] {
				if(hwRng_.eof()) [[unlikely]] throw std::runtime_error("failed to get random bytes from the OS: EOF");
				static_assert(sizeof(char) == sizeof(byte_t), "std::basic_istream<char*> reads chars");
				auto rdReq = neededSize + byteIndex_ - bufferSize_;
				hwRng_.read(reinterpret_cast<char*>(byteBuffer_.get()) + bufferSize_, rdReq);
				auto rd = size_t(hwRng_.gcount());
				assert(rd == rdReq || hwRng_.eof());
				bufferSize_ += rd;
			}
		}

		template <typename T>
		T generate() noexcept {
			fillBufferTo(sizeof(T));
			T r = { };
			assert(byteIndex_ + sizeof(T) <= RNG_BLOCK);
			memcpy(&r, byteBuffer_.get() + byteIndex_, sizeof(T));
			byteIndex_ += sizeof(T);
			return r;
		}

	private:
		size_t byteIndex_;
		size_t bufferSize_;
		std::unique_ptr<xorinator::byte_t[]> byteBuffer_;
		std::ifstream hwRng_;
	};
	#endif


	/** Check non-fatal semantic errors. */
	void checkArgumentUsage(const CommandLine& cmdln) {
		static constexpr std::string_view pre = "Warning: ";
		if(cmdln.options & xorinator::cli::OptionBits::eQuiet) return;
		if(cmdln.cmdType == CmdType::eDemultiplex) {
			if(cmdln.litterSize != 0) {
				std::cerr << pre << "the \"--litter\" argument has no effect for this subcommand." << std::endl;
			} else
			if(! cmdln.roKeys.empty()) {
				std::cerr << pre << "\"--nogen\" arguments are redundant for this subcommand." << std::endl;
			}
		}
	}


	void checkPaths(const CommandLine& cmdln) {
		using xorinator::cli::CmdType;
		using CmdlnException = xorinator::cli::InvalidCommandLineException;
		if(cmdln.firstArg.empty())
			throw CmdlnException("invalid file \"\"");
		if(cmdln.variadicArgs.size() + cmdln.roKeys.size() < 2) {
			if(cmdln.cmdType == CmdType::eMultiplex)
				throw CmdlnException("a multiplexing operation needs two or more keys");
			if(cmdln.cmdType == CmdType::eDemultiplex)
				throw CmdlnException("a demultiplexing operation needs two or more keys");
		}
		if(cmdln.cmdType == CmdType::eMultiplex) {
			if(cmdln.variadicArgs.empty())
				throw CmdlnException("a multiplexing operation needs one or more output files");
		}
		else if(cmdln.cmdType == CmdType::eMultiplex) {
			if(cmdln.variadicArgs.size() + cmdln.roKeys.size())
				throw CmdlnException("a demultiplexing operation needs one or more input files");
		}
		if((cmdln.cmdType == CmdType::eMultiplex) || (cmdln.cmdType == CmdType::eDemultiplex)) {
			auto paths = std::unordered_set<std::string>(cmdln.variadicArgs.size());
			paths.insert(cmdln.firstArg);
			for(size_t i=0; i < cmdln.variadicArgs.size(); ++i) {
				if(! paths.insert(cmdln.variadicArgs[i]).second)
					throw CmdlnException("file arguments must be unique");
			}
		}
	}


	struct Rd {
		bool              s/*uccess*/;
		xorinator::byte_t b/*yte*/;
	};
	auto readByte(std::istream& istr) -> Rd {
		if(! istr) [[unlikely]] return { false, '\0' };
		char r;
		istr.read(&r, sizeof(r));
		if(! istr) return { false, '\0' };
		// bit_cast, because std::basic_istream seemingly doesn't understand binary files;
		// keep in mind that `char` and `uint8_t` are EXPLICITLY different types.
		// On Linux/x86_64, numeric_limits<char   >::digits == 7, while
		//                  numeric_limits<uint8_t>::digits == 8.
		return { true, std::bit_cast<xorinator::byte_t>(r) };
	};

}



namespace xorinator::runtime {

	bool runMux(const CommandLine& cmdln) {
		using xorinator::byte_t;

		assert(cmdln.cmdType == cli::CmdType::eMultiplex);
		checkPaths(cmdln);
		checkArgumentUsage(cmdln);

		auto rndDev = std::random_device();
		auto muxIn = InputStreamAdapter(cmdln.firstArg, cmdln.firstLiteralArg <= 0);
		auto muxOut = std::vector<OutputStreamAdapter>(cmdln.variadicArgs.size());
		auto outputBuffer = StaticVector<byte_t>(muxOut.size());
		auto roKeyStreams = StaticVector<std::ifstream>(cmdln.roKeys.size());
		RngAdapter rng;

		for(size_t i=0; const std::string& key : cmdln.roKeys) {
			roKeyStreams[i] = std::ifstream(key);
			++i;
		}

		muxIn.get().exceptions(std::ios_base::badbit);
		for(unsigned i=0; const std::string& path : cmdln.variadicArgs) {
			muxOut[i] = OutputStreamAdapter(path, cmdln.firstLiteralArg <= (i+1));
			muxOut[i].get().exceptions(std::ios_base::badbit);
			++i;
		}

		char inputChar;
		while(muxIn.get().get(inputChar)) {
			byte_t xorSum = 0;
			for(size_t i=1; i < muxOut.size(); ++i) {
				outputBuffer[i] = rng.generate<byte_t>();
				xorSum = xorSum ^ outputBuffer[i];
			}
			for(auto& keyIstr : roKeyStreams) {
				auto rd = readByte(keyIstr);
				if(rd.s) [[likely]] xorSum = xorSum ^ rd.b;
				else                goto lbl_stop_multiplexing; // need to break two loops
			}
			outputBuffer[0] = byte_t(inputChar) ^ xorSum;
			for(size_t i=0; auto& output : muxOut) {
				output.get().put(outputBuffer[i++]); }
		}
		lbl_stop_multiplexing:

		if(cmdln.litterSize > 0) {
			size_t noLitterIndex = rng.generate<byte_t>() % muxOut.size();
			for(size_t i=0; auto& output : muxOut) {
				using lit_t = decltype(cmdln.litterSize);
				if((i++) != noLitterIndex) {
					lit_t litterSize = rng.generate<lit_t>() % cmdln.litterSize;
					for(lit_t i=0; i < litterSize; ++i) {
						output.get().put(rng.generate<byte_t>());
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

		assert(cmdln.cmdType == cli::CmdType::eDemultiplex);
		checkPaths(cmdln);
		checkArgumentUsage(cmdln);

		auto demuxOut = OutputStreamAdapter(cmdln.firstArg, cmdln.firstLiteralArg <= 0);
		auto demuxIn = StaticVector<InputStreamAdapter>(cmdln.variadicArgs.size() + cmdln.roKeys.size());

		demuxOut.get().exceptions(std::ios_base::badbit);
		for(size_t i=0; const std::string& path : cmdln.variadicArgs) {
			demuxIn[i] = InputStreamAdapter(path, cmdln.firstLiteralArg <= (i+1));
			demuxIn[i].get().exceptions(std::ios_base::badbit);
			++i;
		}
		for(size_t i = cmdln.variadicArgs.size(); const std::string& path : cmdln.roKeys) {
			demuxIn[i] = InputStreamAdapter(path, true);
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
			<< "   -q | --quiet  (suppress error messages)\n"
			<< "   -g NUM | --litter NUM  (add red herring bytes when generating one-time pads)\n"
			<< "   -G FILE_IN | --nogen FILE_IN  (treat FILE_IN as an already generated one-time pad)\n"
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
