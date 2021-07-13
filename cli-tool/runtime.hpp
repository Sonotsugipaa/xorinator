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
#include <fstream>
#include <cassert>
#include <stdexcept>

#include "clparser.hpp"



namespace xorinator::runtime {

	class FilePermissionException : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};


	/** A std::ifstream / std::ofstream wrapper, that replaces the stream when
	 * constructed from the path "-". */
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

		VirtualStream(const std::string& path) {
			if(path != "-") {
				preallocated_ = false;
				stream_ = new file_stream_t(path, std::ios_base::binary);
			} else {
				preallocated_ = true;
				if constexpr(isInputStream) { stream_ = &std::cin; }
				else if constexpr(isOutputStream) { stream_ = &std::cout; }
				else { assert(false && "this assertion shouldn't ever be reachable"); }
			}
		}

		~VirtualStream() {
			if(stream_ != nullptr) {
				if(! preallocated_)  delete stream_;
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

	/** A std::ifstream wrapper, that replaces the stream when
	 * constructed from the path "-". */
	using VirtualInputStream = VirtualStream<std::istream, std::ifstream>;

	/** A std::ofstream wrapper, that replaces the stream when
	 * constructed from the path "-". */
	using VirtualOutputStream = VirtualStream<std::ostream, std::ofstream>;


	bool runMux(const cli::CommandLine&);

	bool runDemux(const cli::CommandLine&);

	bool usage(const cli::CommandLine&);

	bool run(const cli::CommandLine&);

}