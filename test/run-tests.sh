#!/bin/sh
set -e

_dirname="$(realpath "$(dirname "$0")")"
if [ ! "$_dirname" = "$(pwd)" ]; then
	echo "Forcing CWD=\"$_dirname\""$'\n'
	cd "$_dirname"
fi

unset _error
find tests/ -maxdepth 1 -executable -name 'UnitTest-*' | while read _file; do
	_file_pretty="$(dirname "$_file")/"$'\033[33m'"$(basename "$_file")"$'\033[m'
	echo $'\n\nRunning '"\"$_file_pretty\""$'\n'
	"$_file" \
	&& echo $'\n'"\"$_file_pretty\""$': \033[1;33mOK\033[m' \
	|| (_error=1; echo $'\n'"\"$_file_pretty\""$': \033[1;31mFailure\033[m')
done; unset _file
unset _file_pretty

[ -n "$_error" ] && exit 1
unset _error
