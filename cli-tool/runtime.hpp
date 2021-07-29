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
#include <random>

#include "clparser.hpp"



namespace xorinator {

	using byte_t = uint8_t;
	using DefaultKeyRndDevice = std::mt19937_64;


	template<unsigned keyBits, typename RndEngine = DefaultKeyRndDevice>
	class RngKey {
	public:
		using word_t = uint64_t;
		constexpr static unsigned bit_count = keyBits;
		static_assert(0 == bit_count % std::numeric_limits<byte_t>::digits);
		static_assert(bit_count >= std::numeric_limits<word_t>::digits);
		constexpr static unsigned byte_count = bit_count / std::numeric_limits<byte_t>::digits;
		constexpr static unsigned word_count = byte_count * sizeof(byte_t) / sizeof(word_t);
		static_assert(sizeof(word_t) % sizeof(byte_t) == 0);
		static_assert(byte_count % word_count == 0);

		std::array<word_t, word_count> words;

		inline explicit RngKey(const decltype(words)& init = { }):
				words(init)
		{ }

		template<typename Elem>
		inline RngKey(const Elem* data) {
			memcpy(words.data(), data, words.size() * sizeof(word_t));
		}

		inline RngKey(std::string_view sv) {
			word_t rndEngineSeed = 0;
			for(unsigned i=0; char c : sv) {
				rndEngineSeed = rndEngineSeed | (static_cast<word_t>(c) << word_t(CHAR_BIT * i));
				i = (i+1) % (sizeof(word_t) / sizeof(c));
			}
			auto rng = DefaultKeyRndDevice(rndEngineSeed);
			for(auto& dst : words) {
				dst = rng(); }
		}

		class View {
			friend RngKey;
		private:
			const decltype(RngKey::words)* words_;
			size_t beg_;
			size_t end_;

			inline View(const decltype(RngKey::words)& words, size_t begin, size_t end):
					words_(&words), beg_(begin), end_(end)
			{ }

		public:
			inline View(): words_(nullptr), beg_(0), end_(0) { }

			class Iterator {
			private:
				using RndEnginePtr = std::unique_ptr<RndEngine>;
				std::unique_ptr<std::array<RndEnginePtr, word_count>> generators_;
				size_t remaining_;
				word_t current_gen_;
				unsigned current_gen_rsh_;

				void regen_() {
					for(auto& gen : *generators_) {
						current_gen_ ^= (*gen)(); }
				}

			public:
				inline Iterator():
						generators_(nullptr),
						remaining_(0),
						current_gen_(0),
						current_gen_rsh_(0)
				{ }

				inline Iterator(const decltype(RngKey::words)& words, size_t beg, size_t end):
						generators_(std::make_unique<std::array<RndEnginePtr, word_count>>()),
						remaining_(end - beg),
						current_gen_(0),
						current_gen_rsh_(0)
				{
					for(size_t i=0; i < generators_->size(); ++i) {
						(*generators_)[i] = std::make_unique<RndEngine>(RndEngine(words[i]));
						(*generators_)[i]->discard(beg);
					}
					regen_();
				}

				inline Iterator& operator++() {
					current_gen_rsh_ = (current_gen_rsh_ + bit_count) % (std::numeric_limits<word_t>::digits);
					if(current_gen_rsh_ == 0) {
						regen_(); }
					if(remaining_ > 0) {
						--remaining_; }
					return *this;
				}

				inline byte_t operator*() const {
					return
						(current_gen_ >> (current_gen_rsh_ * std::numeric_limits<byte_t>::digits))
						& ((1 << std::numeric_limits<byte_t>::digits) - 1);
				}

				inline bool operator==(const Iterator& rh) const {
					return remaining_ == rh.remaining_; }

				inline bool operator!=(const Iterator& rh) const {
					return ! operator==(rh); }
			};

			inline const Iterator begin() const { return Iterator(*words_, beg_, end_); }
			inline const Iterator end() const { return Iterator(); }
		};

		inline const View view(size_t begin, size_t end) const {
			return View(words, begin, end); }

		/** Constructs a RngKey view with size 0: it can be used
		 * when the key has an unknown length. */
		inline const View view(size_t begin) const {
			return View(words, begin, begin); }
	};

}



namespace xorinator::runtime {

	class FilePermissionException : public std::runtime_error {
	public:
		using std::runtime_error::runtime_error;
	};


	bool runMux(const cli::CommandLine&);

	bool runDemux(const cli::CommandLine&);

	bool usage(const cli::CommandLine&);

	bool run(const cli::CommandLine&);

}
