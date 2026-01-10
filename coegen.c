#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define OPT_STRING 		"b:o:s:"
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

enum COE_STYLE {
	NONE = 0,
	XILINX,
	GOWIN,
	ASM
};
typedef enum COE_STYLE coe_style_t;

size_t fsize(FILE* fd) {
	size_t pos = ftell(fd);
	fseek(fd, 0, SEEK_END);
	size_t result = ftell(fd);
	fseek(fd, pos, SEEK_SET);
	return result;
}

int main(int argc, char* argv[]) {
	int size = 0;
	coe_style_t style = NONE;
	FILE* result = stdout;

	int ret;
	while ((ret = getopt(argc, argv, OPT_STRING)) != -1)
	switch (ret) {
	case 'b':
		size = atoi(optarg);
		break;
	case 'o':
		result = fopen(optarg, "w");
		break;
	case 's':
		if (strcasecmp(optarg, "xilinx") == 0) {
			style = XILINX;
			break;
		}
		if (strcasecmp(optarg, "gowin") == 0) {
			style = GOWIN;
			break;
		}
		if (strcasecmp(optarg, "asm") == 0) {
			style = ASM;
			break;
		}
		break;
	case '?':
		return -1;
	}
	if (style == NONE) {
		fprintf(stderr, "%s: error: no specific output style\n", argv[0]);
		return -1;
	}
	if (style == ASM) {
		if (size == 0) size = 4;
		if (size != 4) {
			fprintf(stderr, "%s: error: only 32 bit width supported for style \'asm\'\n", argv[0]);
			return -1;
		}
	}
	if (size == 0) {
		fprintf(stderr, "%s: error: no specific bit length\n", argv[0]);
		return -1;
	}
	if (optind >= argc) {
		fprintf(stderr, "%s: error: no input files\n", argv[0]);
		return -1;
	}

	FILE* src = fopen(argv[optind], "rb");
	if(!src) {
		fprintf(stderr, "%s: error: no such file or directory\n", argv[0]);
		return -1;
	}

	if (style == XILINX)
		fprintf(result, "memory_initialization_radix=16;\nmemory_initialization_vector=\n"); //for Xilinx
	if (style == GOWIN)
		fprintf(result, "#File_format=Hex\n#Address_depth=%d\n#Data_width=%d\n", DIV_ROUND_UP(fsize(src),size), size*8); //for GOWIN
	if (style == ASM) {
		fprintf(result, "\t.type\tdata,@object\n");
		fprintf(result, "\t.data\n");
		fprintf(result, "\t.globl\tdata\n");
		fprintf(result, "\t.p2align\t4\n");
		fprintf(result, "data:\n");
	}

	for (;;) {
		unsigned char buf[64];
		memset(buf, 0, sizeof(buf));
		fread(buf, 1, size, src);
		if (style == ASM)
			fprintf(result, "\t.4byte\t0x");
		for (int i = size - 1; i >= 0; i--)
			fprintf(result, "%02x", buf[i]);
		if (feof(src)) {
			if (style == XILINX)
				fprintf(result, ";\n"); //for Xilinx
			if (style == ASM)
				fprintf(result, "\n\t.size\tdata, %d\n", fsize(src));
			break;
		}
		fprintf(result, "\n");
	}
	return 0;
}
