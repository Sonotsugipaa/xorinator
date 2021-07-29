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

#include <cli-tool/runtime.hpp>

#include <iostream>
#include <fstream>
#include <filesystem>



namespace {

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;


	template<typename KeyView>
	std::string key_to_str(const KeyView& kv) {
		std::string r;
		for(auto byte : kv) {
			r.push_back(byte); }
		return r;
	}


	template<uint64_t k1, uint64_t k2, unsigned bytes_beg, unsigned bytes_end, bool eq>
	utest::ResultType test_rng64_to_rng64(std::ostream&) {
		std::string str1, str2;
		{
			auto rngKey = xorinator::RngKey<64>({ k1 });
			auto rngKeyView = rngKey.view(bytes_beg, bytes_end);
			str1 = key_to_str(rngKeyView);
		} {
			auto rngKey = xorinator::RngKey<64>({ k2 });
			auto rngKeyView = rngKey.view(bytes_beg, bytes_end);
			str2 = key_to_str(rngKeyView);
		}
		return (str1 == str2) == eq? eSuccess : eFailure;
	}


	template<uint64_t k, unsigned bytes_beg, unsigned bytes_end, bool eq>
	utest::ResultType test_rng64_to_rng128(std::ostream&) {
		std::string str1, str2;
		{
			auto rngKey = xorinator::RngKey<64>({ k });
			auto rngKeyView = rngKey.view(bytes_beg, bytes_end);
			str1 = key_to_str(rngKeyView);
		} {
			auto rngKey = xorinator::RngKey<128>({ k, k+1 });
			auto rngKeyView = rngKey.view(bytes_beg, bytes_end);
			str2 = key_to_str(rngKeyView);
		}
		return (str1 == str2) == eq? eSuccess : eFailure;
	}


	template<uint64_t k1, uint64_t k2, unsigned bytes_beg, unsigned bytes_end, bool eq>
	utest::ResultType test_rng128_to_rng128(std::ostream&) {
		std::string str1, str2;
		{
			auto rngKey = xorinator::RngKey<128>({ k1, k1+1 });
			auto rngKeyView = rngKey.view(bytes_beg, bytes_end);
			str1 = key_to_str(rngKeyView);
		} {
			auto rngKey = xorinator::RngKey<128>({ k2, k2+1 });
			auto rngKeyView = rngKey.view(bytes_beg, bytes_end);
			str2 = key_to_str(rngKeyView);
		}
		return (str1 == str2) == eq? eSuccess : eFailure;
	}


	utest::ResultType test_rngkey512(std::ostream& os) {
		constexpr static unsigned genBytes = 8;
		constexpr static std::array<xorinator::RngKey<512>::word_t, genBytes> hardcodedResult = {
			120, 162, 20, 32, 247, 3, 34, 211 };
		constexpr static auto print_gen = [](std::ostream& os, decltype(hardcodedResult) gen) {
			static_assert(genBytes > 0);
			os << gen.front();
			for(size_t i=1; i < gen.size(); ++i) {
				os << '.' << gen[i]; }
		};

		xorinator::RngKey<512> key = std::string_view("deterministic variable-length key");
		auto keyView = key.view(0, genBytes);
		std::array<xorinator::RngKey<512>::word_t, genBytes> gen;
		for(unsigned i=0; const auto& genByte : keyView) {
			gen[i++] = genByte; }

		if(hardcodedResult != gen) {
			os << "Expected ";
			print_gen(os, hardcodedResult);
			os << "\nGenerated ";
			print_gen(os, gen);
			os << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

}



int main(int, char**) {
	constexpr unsigned SHORT_CHARS = 12;
	constexpr unsigned LONG_CHARS = 4096*4;
	auto batch = utest::TestBatch(std::cout);
	batch
		.run("789a.0x40 == 789a.0x40", test_rng64_to_rng64<0x123456789a, 0x123456789a, 0, SHORT_CHARS, true>)
		.run("4321.0x40 != 789a.0x40", test_rng64_to_rng64<0xa987654321, 0x123456789a, 0, SHORT_CHARS, false>)
		.run("4321.0x40 == 4321.0x40 (offset)", test_rng64_to_rng64<0xa987654321, 0xa987654321, 3, 3+SHORT_CHARS, true>)
		.run("789a.0x40 == 789a.0x40 (long)", test_rng64_to_rng64<0x123456789a, 0x123456789a, 0, LONG_CHARS, true>)
		.run("4321.0x40 != 789a.0x40 (long)", test_rng64_to_rng64<0x123456789a, 0xa987654321, 0, LONG_CHARS, false>)
		.run("4321.0x80 == 4321.0x80", test_rng128_to_rng128<0xa987654321, 0xa987654321, 0, SHORT_CHARS, true>)
		.run("4321.0x40 != 4321.0x80", test_rng64_to_rng128<0xa987654321, 0, SHORT_CHARS, false>)
		.run("Deterministic key from string", test_rngkey512);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
