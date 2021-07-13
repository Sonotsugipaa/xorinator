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

#include "test_tools.hpp"

#include <iostream>



namespace {

	std::string stretch_str(const std::string& str, unsigned n) {
		n = std::max(std::string::size_type(0), n - str.size());
		return str + ' ' + std::string(n, '.');
	}


	std::string strSuccess(const std::string& name, unsigned pad) {
		using namespace utest;
		return stretch_str(name, pad) + "... " +
			colorResult<ResultType::eSuccess>("Success");
	}

	std::string strFailure(const std::string& name, unsigned pad) {
		using namespace utest;
		return stretch_str(name, pad) + "... " +
			colorResult<ResultType::eFailure>("Failure");
	}

	std::string strNeutral(const std::string& name, unsigned pad) {
		using namespace utest;
		return stretch_str(name, pad) + "... " +
			colorResult<ResultType::eNeutral>("Review output");
	}

}



namespace utest {


	template<>
	std::string colorResult<ResultType::eSuccess>(const std::string& str) {
		return "\033[1;34m" + str + "\033[m";
	}

	template<>
	std::string colorResult<ResultType::eFailure>(const std::string& str) {
		return "\033[1;31m" + str + "\033[m";
	}

	template<>
	std::string colorResult<ResultType::eNeutral>(const std::string& str) {
		return "\033[1;36m" + str + "\033[m";
	}



	TestBatch::TestBatch(std::ostream& os):
			_os(os),
			_longest(0)
	{ }


	TestBatch::~TestBatch() {
		auto lock = std::lock_guard(_mtx_vec_access);
		for(const std::string& success : _successes) {
			std::cout << strSuccess(success, _longest) << '\n'; }
		for(const std::string& failure : _failures) {
			std::cout << strFailure(failure, _longest) << '\n'; }
		for(const std::string& neutral : _neutrals) {
			std::cout << strNeutral(neutral, _longest) << '\n'; }
	}


	unsigned TestBatch::failures() const {
		return _failures.size();
	}

}
