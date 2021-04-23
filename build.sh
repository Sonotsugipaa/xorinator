#!/bin/bash

set -e # Terminate the script when an error occurs

config="${1:-"Debug"}"
generator="${generator:-"Ninja"}"

srcpath="$(realpath .)"
dstpath=build-"$config"

mkdir -p "$dstpath"
cd "$dstpath"

cmake -DCMAKE_BUILD_TYPE="$config" "$srcpath" -G "$generator"
cmake --build . --config "$config"
