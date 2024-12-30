# Xorinator

## Information

Xorinator is a command-line utility for encrypting and decrypting files using the one-time pad technique. More specifically, it allows files to be "split" into multiple one-time pad files (hereinafter "*OTPs*"), then put the pieces back together.

Generally, the one-time pad technique involves using a randomly generated key (the OTP) that is as big as the message. With Xorinator, multiple OTPs can be used, at the expense of a much higher overhead in total space used: for example, splitting (hereinafter "multiplexing") one 8kb file into 5 will result in (8 × 5 = 40)kb being used, potentially distributed over 5 different storage devices.

## License

Xorinator uses the [MIT license](https://mit-license.org/) (for the foreseeable future), a copy of which is included in the repository (and pasted at the beginning of multiple source files).

## Building and installing

Xorinator only depends on the C++ STL; it is meant to be compiled and linked using the GNU toolchain (+ CMake), although it has been partially tested with MSVC (with a bit of tinkering).

### On GNU/Linux systems

Use this command in the project's root directory in order to build it:

```bash
./build.sh Release
```

The executable file is `build-Release/cli-tool/xor`. In order to install it, either copy/move it to `/usr/local/bin/` or run the following command to let CMake decide the install directory:

```bash
cmake --install build-Release
```

#### On Arch Linux (and derivates)

Xorinator has an [AUR package](https://aur.archlinux.org/packages/xorinator/). In order to install it:
- Clone the AUR repository
  - `git clone https://aur.archlinux.org/xorinator.git`
- Enter the repository and build the package (the `core-devel` package group is required)
  - `cd xorinator && makepkg`
- Manually install the package
  - `pacman -U xorinator-VERSION.pkg.tar.zst`

Alternatively, use the following Bash one-liner (parentheses included):  
`(git clone 'https://aur.archlinux.org/xorinator.git' && cd xorinator && makepkg -i)`

## Usage

Xorinator provides a command-line tool as an executable file, `xor` (or `xor.exe`, for NT-based systems).

The syntax of the command expects:

1. a subcommand, either "`multiplex`" ("`mux`", "`m`") or "`demultiplex`" ("`demux`", "`dmx`", "`d`");
2. options, as described below;
3. one file path, which points to the source (for multiplexing) or to the destination (for demultiplexing);
4. one or more file paths, which point to the destination files (for multiplexing) or the sources (for demultiplexing).

Notably, using "`-`" as a file name will read or write to the standard input/output, depending on the context. Running `xor`, `xor ?` or `xor help` will print a description of the syntax.

### Options

Multiple options can be used anywhere in the argument list, unless the `--` argument is present anywhere - in which case all the arguments after it will not be interpreted as options.  
The option syntax partially follows the POSIX convention:
- unary short options may be conflated into a single argument (e.g. "`-abc`" instead of "`-a -b -c`");
- binary short options may have their values directly after the second character (e.g. "`-g4096`" instead of "`-g 4096`");
- binary long options may have their key and value in the same argument, with a "`=`" separating the two (e.g. "`--litter=4096`" instead of "`--litter 4096`").

#### `--quiet`

Suppress error messages that may result from incorrect input (e.g. `xor mux -`) or from runtime errors (e.g. upon opening an unreadable file).

#### `--litter NUM`

When performing multiplexing operations, the "`--litter NUM`" option causes up to `NUM` additional random characters to be written at the end of each output file (except one).

This is useful to hide the fact that all one-time pads have the same size as the original message.  
After a demultiplexing operation, the reconstructed message will be as big as the smallest input.

#### `--nogen FILE_IN`

When performing multiplexing operations, generate output files so that demultiplexing them along with `FILE_IN` will return the same file; `FILE_IN` will not be modified.  
Multiple `--nogen` options are commutative and cumulative.

Due to how demultiplexing works, `FILE_IN` is assumed to have a bigger or equal size compared to the input file.  
If that assumption fails, `FILE_OUT` will be truncated to the length of `FILE_IN`.

**Be careful**:  
if you intend to write  
`xor mux ifile key0 key1 --nogen FILE_IN`, but accidentally write  
`xor mux ifile key0 key1 FILE_IN`, the file "FILE_IN" *will be overwritten*.  
Make sure the command is correct before submitting it, or make a temporary copy
of important files involved in the process.

**Be careful 2**:  
If an even number of `--nogen` options with equal files is used, it's like not using them at all:  
`(a XOR b) XOR b  =  a XOR (b XOR b) =  a`  
If an odd number of `--nogen` options with equal files is used, it's like using only one:  
`((a XOR b) XOR b) XOR b  =  a XOR (b XOR (b XOR b))  =  a XOR (b XOR 0)  =  a XOR b`

### Examples

```bash
#!/bin/bash

# 3-file multiplexing:
# the "xor" command reads arbitrary text from the standard
# input, then encrypts it onto "secret.1.xor",
# "secret.2.xor" and "secret.3.xor".
# In order to rebuild the input, the 3 generated files
# need to be demultiplexed.
xor mux - secret.1.xor secret.2.xor secret.3.xor
xor dmx secret.decrypted.txt secret.*.xor

# 2-file multiplexing with one existing key:
# acts similarly to the previous example, but "SomeDocument.pdf"
# is an existing file used as a secret.
# Ideally existing keys are also random and kept secret,
# but the fact that the key isn't generated by the program implies
# that it is not necessarily unique to the plain-text data, negating
# the "One Time" part of "One Time Pad".
# !!!!  BEWARE  !!!!
# !!!! This example is very bad opsec.
# !!!! It uses a presumably formatted (non-random) file, which means partially
# !!!! retrieving plain-text data is possible if the attacker is in possession of
# !!!! every other key; in this case, "every other key" is a single key.
# !!!! Additionally, an attacker who has both "secret.txt" and "secret.xor" can
# !!!! trivially compute the content of "SomeDocument.pdf".
xor mux secret.txt --nogen SomeDocument.pdf secret.xor
xor dmx secret.decrypted.txt SomeDocument.pdf secret.xor

# 4-file multiplexing with two existing publicly available keys:
# basically provides the same level of secrecy of regular 2-file multiplexing,
# with an additional thin layer of security through obscurity.
xor mux secret.txt secret.1.xor secret.2.xor --nogen unlicense.txt --nogen ipsum-lorem.txt
xor dmx secret.decrypted.txt secret.1.xor secret.2.xor unlicense.txt ipsum-lorem.txt

# Running these two commands in order should create "hello.xor" containing
# 5 randomly generated bytes (plus EOL), then write "Hello" to the standard output.
xor mux <(echo Hello) --nogen <(echo 12345) hello.xor
xor dmx - <(echo 12345) hello.xor
```
