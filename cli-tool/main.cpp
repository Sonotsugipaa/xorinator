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

#include "clparser.hpp"
#include "runtime.hpp"

using xorinator::cli::CommandLine;
using xorinator::cli::InvalidCommandLineException;



int main(int argc, char** argv) {
	CommandLine cmdln;
	try {
		cmdln = CommandLine(argc, argv);
		return xorinator::runtime::run(cmdln)? EXIT_SUCCESS : EXIT_FAILURE;
	}
	#define CATCH_EX(EX_) catch(EX_& ex) { \
		if(! (cmdln.options & xorinator::cli::OptionBits::eQuiet)) \
		std::cerr << "[" #EX_ "]\n" << ex.what() << std::endl; \
	}
		CATCH_EX(InvalidCommandLineException)
		CATCH_EX(std::exception)
	#undef CATCH_EX
	return EXIT_FAILURE;
}
