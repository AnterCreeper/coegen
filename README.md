# COEgen
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/antercreeper/coegen)

If any questions, welcome for Issues & PRs   
如果有疑问，欢迎提交Issues或PR

**WARNING! This Project is not been fully long-term productive tested and without warranty of any kind, use at your own risk!**   
**警告！该项目未经充分的长期生产环境测试，作者不作任何保证，使用需要你自己衡量**

### License

This project is licensed under the LGPL-2.1-or-later license. **DO NOT** download or clone this project until you have read and agree the LICENSE.     
该项目采用 `LGPL-2.1 以及之后版本` 授权。当你下载或克隆项目时，默认已经阅读并同意该协定。

### Overview

This project converts byte-packed binary images to FPGA and memory-compiler ROM
initialization text. Input words use little-endian byte order by default; Xilinx,
Gowin, and Verilog output also supports big-endian input words.

### TODO
1. Asm output in char, and custom array name

### Usage
```text
coegen -f xilinx|gowin|verilog (-w <bits> | -b <bytes>) [-e little|big] [-d <words>] [-o <output>] <input>
coegen -f asm [-b 4] -s <varname> [-o <output>] <input>
```

- `style`: output text format; supported styles are `xilinx`, `gowin`, `asm`,
  and `verilog`
- `varname`: variable name for `asm`
- `bytes`: byte-aligned word width; retained for compatibility and equivalent
  to `-w <bytes * 8>` for `xilinx`, `gowin`, and `verilog`
- `bits`: logical output word width; it need not be byte-aligned
- `little|big`: byte order within each input word; default is `little`
- `words`: exact output depth; short inputs are zero-filled and oversized inputs
  are rejected
- `input`: input binary file path
- `output`: print to output file instead of console

Every input word occupies `ceil(bits / 8)` bytes, and the input file size must be
a multiple of that byte count. Words do not share a packed bitstream. Any unused
high bits in a non-byte-aligned word must be zero, accounting for the selected
byte order.

Xilinx and Gowin emit the minimum `ceil(bits / 4)` hexadecimal digits per word;
the leading digit is zero-padded when the logical width is not nibble-aligned.
Verilog emits exactly `bits` MSB-first binary digits per line for use with
`$readmemb`. ASM remains a fixed
32-bit little-endian formatter.
