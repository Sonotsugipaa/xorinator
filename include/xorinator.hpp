#pragma once

#include <string_view>
#include <istream>
#include <array>
#include <limits>
#include <random>
#include <memory>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <climits>



namespace xorinator {

	using byte_t = uint8_t;
	using DefaultKeyRndDevice = std::mt19937_64;


	class FullKey : public std::basic_string<byte_t> {
	public:
		using std::basic_string<byte_t>::basic_string;

		inline FullKey(const std::string& str) {
			this->reserve(str.size());
			for(auto c : str) {
				this->push_back(c); }
		}

		class View {
			friend FullKey;
		private:
			const basic_string* key_;
			basic_string::size_type beg_, end_;

			inline View() { }

			inline View(const basic_string& k, size_type begin, size_type end):
					key_(&k), beg_(begin), end_(end)
			{ }

		public:
			inline const byte_t* begin() const { return key_->data() + beg_; }
			inline const byte_t* end() const { return key_->data() + end_; }
		};

		inline const View view(size_type begin, size_type end) const {
			return View(*this, begin, end); }
	};


	class StreamKey {
	private:
		std::istream* file_;

	public:
		inline StreamKey(std::istream& input):
				file_(&input)
		{ }

		class View {
			friend StreamKey;
		private:
			std::istream* fptr_;
			size_t offset_;

			inline View(std::istream* fptr, size_t offset): fptr_(fptr), offset_(offset) { }

		public:
			class Iterator {
			private:
				std::istream* fptr_;
				byte_t buffer_;  static_assert(sizeof(byte_t) == sizeof(std::istream::char_type));

				inline void get_() {
					buffer_ = fptr_->get() * (! fptr_->fail());
				}

			public:
				inline Iterator(): fptr_(nullptr) { }

				inline Iterator(std::istream* fptr, size_t begin): fptr_(fptr) {
					fptr_->clear();
					fptr_->seekg(begin, std::istream::beg);
					get_();
				}

				inline Iterator& operator++() { get_();  return *this; }

				inline byte_t operator*() const { return buffer_; }

				inline bool operator==(const Iterator& rh) const {
					if((fptr_ == nullptr) || fptr_->eof()) {
						return (rh.fptr_ == nullptr) || rh.fptr_->eof();
					} else {
						return (fptr_ == rh.fptr_) && (fptr_->tellg() == rh.fptr_->tellg());
					}
				}

				inline bool operator!=(const Iterator& rh) const { return ! ((*this) == rh); }
			};

			inline const Iterator begin() const { return Iterator(fptr_, offset_); }
			inline const Iterator end() const { return Iterator(); }
		};

		inline const View view(size_t begin, size_t end) const {
			(void) end;
			return View(file_, begin);
		}
	};


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
