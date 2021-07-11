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

#pragma once

#include <memory>
#include <string>
#include <exception>
#include <stdexcept>
#include <initializer_list>



namespace xorinator {

	template<typename T>
	class StaticVector {
	public:
		using PtrType = T*;
		using ConstPtrType = const T*;
		using SizeType = std::size_t;
		#if __cpp_constexpr >= __cpp_constexpr_dynamic_alloc
			#define SV_CONSTEXPR constexpr
		#else
			#define SV_CONSTEXPR inline
		#endif

	private:
		SizeType size_;
		PtrType container_;

	public:
		SV_CONSTEXPR StaticVector(SizeType size = 0):
				size_(size),
				container_((size <= 0)? nullptr : new T[size])
		{ }

		template<typename U>
		SV_CONSTEXPR StaticVector(std::initializer_list<U> init_list):
				StaticVector(init_list.size())
		{
			for(SizeType i=0; auto& elem : init_list) {
				container_[i++] = T(std::move(elem)); }
		}

		SV_CONSTEXPR StaticVector(const StaticVector& cp):
				size_(cp.size_),
				container_((size <= 0)? nullptr : new T[size])
		{
			for(SizeType i=0; const auto& elem : cp) {
				container_[i++] = std::move(elem); }
		}

		SV_CONSTEXPR StaticVector(StaticVector&& mv):
				size_(std::move(mv.size_)),
				container_(std::move(mv.container_))
		{
			mv.size_ = 0;
			mv.container_ = 0;
		}

		SV_CONSTEXPR ~StaticVector() {
			if(container_ != nullptr) {
				delete[] container_; }
		}

		SV_CONSTEXPR StaticVector& operator=(const StaticVector& cp) {
			this->~StaticVector();
			return *(new (this) StaticVector(cp));
		}

		SV_CONSTEXPR StaticVector& operator=(StaticVector&& mv) {
			this->~StaticVector();
			return *(new (this) StaticVector(std::move(mv)));
		}

		SV_CONSTEXPR T& operator[](SizeType i) { return container_[i]; }
		SV_CONSTEXPR const T& operator[](SizeType i) const { return container_[i]; }

		SV_CONSTEXPR T* data() { return container_; }
		SV_CONSTEXPR const T* data() const { return container_; }

		SV_CONSTEXPR SizeType size() const { return size_; }
		SV_CONSTEXPR SizeType empty() const { return size_ <= 0; }

		SV_CONSTEXPR T& front() { return operator[](0); }
		SV_CONSTEXPR const T& front() const { return operator[](0); }

		SV_CONSTEXPR T& back() { return operator[](size() - 1); }
		SV_CONSTEXPR const T& back() const { return operator[](size() - 1); }

		SV_CONSTEXPR T* begin() { return &front(); }
		SV_CONSTEXPR const T* begin() const { return &front(); }

		SV_CONSTEXPR T* end() { return &front() + size_; }
		SV_CONSTEXPR const T* end() const { return &front() + size_; }

		#undef SV_CONSTEXPR
	};

}



namespace xorinator::cli {

	enum class CmdType { eNone, eError, eMultiplex, eDemultiplex };


	struct OptionBits {
		using IntType = uint_fast8_t;
		constexpr static IntType eNone = 0;
		#define OPTION_BIT_(NAME_, POS_) constexpr static IntType NAME_ = 1 << POS_;
			OPTION_BIT_(eQuiet, 0)
		#undef OPTION_BIT_
	};

	using Options = OptionBits::IntType;


	class InvalidCommandLineException : public std::runtime_error {
	public:
		InvalidCommandLineException(std::string msg): std::runtime_error(msg) { }
	};


	struct CommandLine {
		CmdType cmdType;
		std::string zeroArg, firstArg;
		StaticVector<std::string> variadicArgs;
		StaticVector<std::string> rngKeys;
		Options options;

		CommandLine();
		CommandLine(int argc, char const * const * argv);
	};

}
