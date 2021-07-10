# Xorinator

## Information

Xorinator is a command-line utility for encrypting and decrypting files using the one-time pad technique. More specifically, it allows files to be "split" into multiple files, then put the pieces back together.

Generally, the one-time pad technique involves using a randomly generated key (the one-time pad) that is as big as the message. With Xorinator, multiple one-time pads can be used, at the expense of a much higher overhead in total space used: for example, splitting (hereinafter "multiplexing") one 8kb file into 5 will result in (8 × 5 = 40)kb being used, potentially distributed over 5 different storage devices.

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

## Usage

Xorinator provides a command-line tool as an executable file, `xor` (or `xor.exe`, for NT-based systems).

The syntax of the command expects:

1. a subcommand, either "`multiplex`" ("`mux`", "`m`") or "`demultiplex`" ("`demux`", "`dmx`", "`d`");
2. [optional] a passphrase, preceded by "`--key `" or "`-k `";
3. one file path, which points to the source (for multiplexing) or to the destination (for demultiplexing);
4. one or more file paths, which point to the destination files (for multiplexing) or the sources (for demultiplexing).

Notably, using "`-`" as a file name will read or write to the standard input/output, depending on the context.

### Examples

```bash
#!/bin/bash

# Symmetric key encription and decryption (suboptimal):
# the file "secret.txt" is encrypted to "secret.xor",
# then it is decrypted and printed onto the terminal.
xor mux --key hunter2 secret.txt secret.xor
xor dmx --key hunter2 - secret.xor

# 3-file multiplexing, without passphrase:
# the "xor" command reads arbitrary text from the standard
# input, then encrypts it onto "secret.1.xor",
# "secret.2.xor" and "secret.3.xor".
# In order to rebuild the input, the 3 generated files
# need to be demultiplexed.
xor mux - secret.1.xor secret.2.xor secret.3.xor
xor dmx secret.decrypted.txt xecret.*.xor
```
