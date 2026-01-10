# COEgen

If any questions, welcome for Issues & PRs   
如果有疑问，欢迎提交Issues或PR

**WARNING! This Project is not been fully long-term productive tested and without warranty of any kind, use at your own risk!**   
**警告！该项目未经充分的长期生产环境测试，作者不作任何保证，使用需要你自己衡量**

### License

This project is licensed under the LGPL-2.1-or-later license. **DO NOT** download or clone this project until you have read and agree the LICENSE.     
该项目采用 `LGPL-2.1 以及之后版本` 授权。当你下载或克隆项目时，默认已经阅读并同意该协定。

### Overview

This Project is used to convert binary to FPGA and Memory Compiler ROM coe text.  
Only Little Endian format is supported.

### TODO
1. Support non power-of-two bit width.
2. Asm output in char, and custom array name

### Usage
```coegen -s <style> -b <width> [-o <output>] <input>```
- `style`: output text format, style supported: `xilinx`, `gowin`, `asm`
- `width`: bit width of memory block in bytes
- `input`: input binary file path
- `output`: print to output file instead of console
