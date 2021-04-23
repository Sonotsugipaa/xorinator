#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>

#include "clparser.hpp"
#include <xorinator.hpp>

using xorinator::cli::CommandLine;
using xorinator::cli::CmdType;
using xorinator::cli::InvalidCommandLine;
using xorinator::StaticVector;

namespace stdfs {
	using namespace std::filesystem;
}



namespace {

	template<typename stream_t, typename file_stream_t>
	class VirtualStream {
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

		VirtualStream(): stream_(nullptr), preallocated_(false) { }

		VirtualStream(stream_t& ref): stream_(&ref), preallocated_(true) { }

		VirtualStream(const std::string& path): preallocated_(true) {
			if constexpr(isInputStream) { stream_ = &std::cin; }
			else { stream_ = &std::cout; }
			if(path != "-") {
				stream_ = new file_stream_t(path, std::ios_base::binary);
				preallocated_ = false;
			}
		}

		~VirtualStream() {
			if(stream_ != nullptr) {
				if(preallocated_)  delete stream_;
				stream_ = nullptr;
			}
		}

		VirtualStream(VirtualStream&& mv):
				stream_(std::move(mv.stream_)),
				preallocated_(std::move(mv.preallocated_))
		{
			mv.stream_ = nullptr;
		}

		VirtualStream& operator=(VirtualStream&& mv) {
			this->~VirtualStream();
			new (this) VirtualStream(std::move(mv));
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

	using VirtualInputStream = VirtualStream<std::istream, std::ifstream>;
	using VirtualOutputStream = VirtualStream<std::ostream, std::ofstream>;


	using Rng = std::mt19937;

	Rng mk_rng() {
		static auto rndDev = std::random_device();
		Rng rng = Rng(rndDev());
		return rng;
	}

	template<typename uint_t>
	uint_t random(Rng rng) {
		static_assert(std::numeric_limits<uint_t>::is_integer);
		static_assert(! std::numeric_limits<uint_t>::is_signed);
		return rng();
	}


	bool runMux(const CommandLine& cmdln) {
		using xorinator::byte_t;
		auto muxIn = VirtualInputStream(cmdln.firstArg);
		auto muxOut = StaticVector<VirtualOutputStream>(cmdln.variadicArgs.size());
		auto outputBuffer = StaticVector<byte_t>(muxOut.size());
		auto rng = mk_rng();

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
		auto demuxOut = VirtualOutputStream(cmdln.firstArg);
		auto demuxIn = StaticVector<VirtualInputStream>(cmdln.variadicArgs.size());
		decltype(demuxIn)::SizeType openInputs = demuxIn.size();

		demuxOut.get().exceptions(std::ios_base::badbit);
		for(unsigned i=0; const std::string& path : cmdln.variadicArgs) {
			demuxIn[i] = path;
			demuxIn[i].get().exceptions(std::ios_base::badbit);
			++i;
		}

		char inputChar;
		while(openInputs > 0) {
			openInputs = 0;
			byte_t xorSum = 0;
			for(auto& input : demuxIn) {
				if(input.get().get(inputChar)) {
					xorSum = byte_t(inputChar) ^ xorSum;
					++openInputs;
				}
			}
			demuxOut.get().put(xorSum);
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
			<< '\t' << zeroArg << " multiplex [--] FILE_IN FILE_OUT [FILE_OUT...]\n"
			<< '\t' << zeroArg << " demultiplex [--] FILE_OUT FILE_IN [FILE_IN...]\n"
			<< '\t' << zeroArg << " help | ?\n"
			<< '\n'
			<< "Aliases for \"multiplex\": mux, m\n"
			<< "Aliases for \"demultiplex\": demux, dmx, d\n" << std::endl;
		return false;
	}


	bool run(CommandLine cmdln) {
		switch(cmdln.cmdType) {
			case CmdType::eMultiplex:  return runMux(cmdln);
			case CmdType::eDemultiplex:  return runDemux(cmdln);
			case CmdType::eNone:  return usage(cmdln);
			case CmdType::eError:  default:  return false;
		}
	}

}



int main(int argc, char** argv) {
	return run(CommandLine(argc, argv))? EXIT_SUCCESS : EXIT_FAILURE;
}
