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



int main(int argc, char** argv) {
	using xorinator::cli::CommandLine;
	using xorinator::cli::InvalidCommandLineException;
	using xorinator::runtime::FilePermissionException;
	#define IF_QUIET if(! (cmdln.options & xorinator::cli::OptionBits::eQuiet))
	#define PRINT_EX(EX_) "[" #EX_ "]\n" << ex.what()
	#define CATCH_EX(EX_) catch(EX_& ex) { \
		IF_QUIET std::cerr << PRINT_EX(EX_) << std::endl; \
	}

	CommandLine cmdln;
	try {
		cmdln = CommandLine(argc, argv);
		return xorinator::runtime::run(cmdln)? EXIT_SUCCESS : EXIT_FAILURE;
	}
	catch(FilePermissionException& ex) {
		IF_QUIET std::cerr
			<< PRINT_EX(FilePermissionException)
			<< "\nYou can try to bypass the access permissions with the \"--force\" option."
			<< std::endl;
	}
	CATCH_EX(InvalidCommandLineException)
	CATCH_EX(std::exception)
	return EXIT_FAILURE;
	#undef CATCH_EX
	#undef PRINT_EX
	#undef IF_QUIET
}
