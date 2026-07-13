#!/bin/sh
set -eu

tmp=${TMPDIR:-/tmp}/coegen-test-$$
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp"

cc -std=c11 -Wall -Wextra -Werror -O2 coegen.c -o "$tmp/coegen"

printf '\170\126\064\022' > "$tmp/word.bin"
"$tmp/coegen" -f smic -w 32 "$tmp/word.bin" > "$tmp/little.txt"
test "$(cat "$tmp/little.txt")" = 00010010001101000101011001111000

"$tmp/coegen" -f smic -w 32 -e big "$tmp/word.bin" > "$tmp/big.txt"
test "$(cat "$tmp/big.txt")" = 01111000010101100011010000010010

printf '\274\012' > "$tmp/width12.bin"
"$tmp/coegen" -f smic -w 12 -d 3 "$tmp/width12.bin" > "$tmp/width12.txt"
test "$(sed -n '1p' "$tmp/width12.txt")" = 101010111100
test "$(sed -n '2p' "$tmp/width12.txt")" = 000000000000
test "$(sed -n '3p' "$tmp/width12.txt")" = 000000000000

printf '\012\274' > "$tmp/width12-big.bin"
"$tmp/coegen" -f smic -w 12 -e big "$tmp/width12-big.bin" > "$tmp/width12-big.txt"
test "$(cat "$tmp/width12-big.txt")" = 101010111100

printf '\274\372' > "$tmp/overflow-bits.bin"
if "$tmp/coegen" -f smic -w 12 "$tmp/overflow-bits.bin" >/dev/null 2>&1; then
	exit 1
fi

printf '\001\002\003' > "$tmp/too-large.bin"
if "$tmp/coegen" -f smic -w 16 -d 1 "$tmp/too-large.bin" >/dev/null 2>&1; then
	exit 1
fi

printf '\170\126\064\022' > "$tmp/exact.bin"
"$tmp/coegen" -f xilinx -b 4 "$tmp/exact.bin" > "$tmp/exact.coe"
test "$(grep -c '^00000000' "$tmp/exact.coe")" = 0
test "$(grep -c '^12345678;' "$tmp/exact.coe")" = 1

printf 'coegen tests passed\n'
