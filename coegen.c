#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define OPT_STRING 		"b:o:"
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

size_t fsize(FILE* fd) {
	size_t pos = ftell(fd);
	fseek(fd, 0, SEEK_END);
	size_t result = ftell(fd);
	fseek(fd, pos, SEEK_SET);
	return result;
}

int main(int argc, char* argv[]) {
	int size = 0;
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
	case '?':
		return -1;
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

	//fprintf(result, "memory_initialization_radix=16;\nmemory_initialization_vector=\n"); //for Xilinx
	fprintf(result, "#File_format=Hex\n#Address_depth=%d\n#Data_width=%d\n", DIV_ROUND_UP(fsize(src),size), size*8);	//for GOWIN

	for (;;) {
		unsigned char buf[64];
		memset(buf, 0, sizeof(buf));
		fread(buf, 1, size, src);
		for (int i = size - 1; i >= 0; i--)
			fprintf(result, "%02x", buf[i]);
		if (feof(src)) {
			//fprintf(result, ";\n"); //for Xilinx
			break;
		}
		fprintf(result, "\n");
	}
	return 0;
}
