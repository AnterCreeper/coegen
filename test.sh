#!/bin/sh
set -eu

tmp=${TMPDIR:-/tmp}/coegen-test-$$
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp"

cc -std=c11 -Wall -Wextra -Werror -O2 coegen.c -o "$tmp/coegen"

printf '\170\126\064\022' > "$tmp/word.bin"
"$tmp/coegen" -f verilog -w 32 "$tmp/word.bin" > "$tmp/little.txt"
test "$(cat "$tmp/little.txt")" = 00010010001101000101011001111000

"$tmp/coegen" -f verilog -w 32 -e big "$tmp/word.bin" > "$tmp/big.txt"
test "$(cat "$tmp/big.txt")" = 01111000010101100011010000010010

printf '\274\012' > "$tmp/width12.bin"
"$tmp/coegen" -f verilog -w 12 -d 3 "$tmp/width12.bin" > "$tmp/width12.txt"
test "$(sed -n '1p' "$tmp/width12.txt")" = 101010111100
test "$(sed -n '2p' "$tmp/width12.txt")" = 000000000000
test "$(sed -n '3p' "$tmp/width12.txt")" = 000000000000

printf '\012\274' > "$tmp/width12-big.bin"
"$tmp/coegen" -f verilog -w 12 -e big "$tmp/width12-big.bin" > "$tmp/width12-big.txt"
test "$(cat "$tmp/width12-big.txt")" = 101010111100

printf '\274\372' > "$tmp/overflow-bits.bin"
if "$tmp/coegen" -f verilog -w 12 "$tmp/overflow-bits.bin" >/dev/null 2>&1; then
	exit 1
fi

printf '\001\002\003' > "$tmp/too-large.bin"
if "$tmp/coegen" -f verilog -w 16 -d 1 "$tmp/too-large.bin" >/dev/null 2>&1; then
	exit 1
fi

if "$tmp/coegen" -f smic -w 32 "$tmp/word.bin" >/dev/null 2>&1; then
	exit 1
fi

printf '\170\126\064\022' > "$tmp/exact.bin"
"$tmp/coegen" -f xilinx -b 4 "$tmp/exact.bin" > "$tmp/exact.coe"
test "$(grep -c '^00000000' "$tmp/exact.coe")" = 0
test "$(grep -c '^12345678;' "$tmp/exact.coe")" = 1

"$tmp/coegen" -f gowin -b 4 "$tmp/exact.bin" > "$tmp/exact.hex"
test "$(sed -n '3p' "$tmp/exact.hex")" = "#Data_width=32"
test "$(sed -n '4p' "$tmp/exact.hex")" = "12345678"

"$tmp/coegen" -f asm -s test_data "$tmp/exact.bin" > "$tmp/exact.S"
test "$(grep -c '0x12345678$' "$tmp/exact.S")" = 1

printf '\012\003' > "$tmp/width4.bin"
"$tmp/coegen" -f xilinx -w 4 "$tmp/width4.bin" > "$tmp/width4.coe"
test "$(sed -n '3p' "$tmp/width4.coe")" = "a,"
test "$(sed -n '4p' "$tmp/width4.coe")" = "3;"

printf '\274\012' > "$tmp/xilinx-width12.bin"
"$tmp/coegen" -f xilinx -w 12 -d 2 "$tmp/xilinx-width12.bin" > "$tmp/width12.coe"
test "$(sed -n '3p' "$tmp/width12.coe")" = "abc,"
test "$(sed -n '4p' "$tmp/width12.coe")" = "000;"

printf '\002\253' > "$tmp/gowin-width10-big.bin"
"$tmp/coegen" -f gowin -w 10 -e big -d 2 "$tmp/gowin-width10-big.bin" > "$tmp/width10.hex"
test "$(sed -n '2p' "$tmp/width10.hex")" = "#Address_depth=2"
test "$(sed -n '3p' "$tmp/width10.hex")" = "#Data_width=10"
test "$(sed -n '4p' "$tmp/width10.hex")" = "2ab"
test "$(sed -n '5p' "$tmp/width10.hex")" = "000"

if "$tmp/coegen" -f xilinx -w 12 "$tmp/overflow-bits.bin" >/dev/null 2>&1; then
	exit 1
fi

printf '\001\002\003' > "$tmp/misaligned.bin"
if "$tmp/coegen" -f gowin -w 16 "$tmp/misaligned.bin" >/dev/null 2>&1; then
	exit 1
fi

if "$tmp/coegen" -f xilinx -b 2 -w 16 "$tmp/exact.bin" >/dev/null 2>&1; then
	exit 1
fi

printf 'coegen tests passed\n'
