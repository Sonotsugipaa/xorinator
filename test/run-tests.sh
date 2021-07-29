#!/bin/bash
set -e

_dirname="$(realpath "$(dirname "$0")")"
if [ ! "$_dirname" = "$(pwd)" ]; then
	echo "Forcing CWD=\"$_dirname\""$'\n'
	cd "$_dirname"
fi

function run_test {
	local _file_pretty="$(dirname "$1")/"$'\033[33m'"$(basename "$1")"$'\033[m'
	if "$1"; then
		echo $'\n'"^^^   \"$_file_pretty\""$': \033[1;33mOK\033[m   ^^^\n'
	else
		echo $'\n'"^^^   \"$_file_pretty\""$': \033[1;31mFailure\033[m   ^^^\n'
		echo 1 >"$2"
	fi
}

function run_all_tests {
	local _file
	find tests/ -maxdepth 1 -executable -name 'UnitTest-*' | while read _file; do
		run_test "$_file" "$1"
	done
}

_tmp="$(mktemp -p '' xorinator_tests.XXXXX)"
run_all_tests "$_tmp"
_error="$(cat "$_tmp")"
[ -n "$_error" ] && exit 1
unset _error
unset _tmp
unset _dirname
