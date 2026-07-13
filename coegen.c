#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define OPT_STRING "b:d:e:f:o:s:w:"
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

enum COE_STYLE {
	NONE = 0,
	XILINX,
	GOWIN,
	ASM,
	SMIC
};
typedef enum COE_STYLE coe_style_t;

enum BYTE_ORDER {
	LITTLE_ENDIAN_ORDER = 0,
	BIG_ENDIAN_ORDER
};
typedef enum BYTE_ORDER byte_order_t;

static size_t fsize(FILE *fd) {
	long pos = ftell(fd);
	long end;
	if (pos < 0 || fseek(fd, 0, SEEK_END) != 0)
		return SIZE_MAX;
	end = ftell(fd);
	if (end < 0 || fseek(fd, pos, SEEK_SET) != 0)
		return SIZE_MAX;
	return (size_t)end;
}

static int parse_positive(const char *text, size_t *value) {
	char *end;
	unsigned long long parsed;
	errno = 0;
	parsed = strtoull(text, &end, 10);
	if (errno != 0 || *text == '\0' || *end != '\0' || parsed == 0 ||
	    parsed > SIZE_MAX)
		return -1;
	*value = (size_t)parsed;
	return 0;
}

static void print_smic_word(FILE *result, const unsigned char *buf,
			    size_t width_bits, byte_order_t order) {
	size_t width_bytes = DIV_ROUND_UP(width_bits, 8);
	for (size_t output_bit = width_bits; output_bit > 0; output_bit--) {
		size_t value_bit = output_bit - 1;
		size_t value_byte = value_bit / 8;
		size_t input_byte = order == LITTLE_ENDIAN_ORDER
			? value_byte
			: width_bytes - 1 - value_byte;
		fputc((buf[input_byte] & (1u << (value_bit % 8))) ? '1' : '0', result);
	}
	fputc('\n', result);
}

static int validate_unused_bits(const unsigned char *buf, size_t width_bits,
				byte_order_t order) {
	size_t remainder = width_bits % 8;
	size_t width_bytes = DIV_ROUND_UP(width_bits, 8);
	size_t most_significant_byte;
	unsigned int unused_mask;
	if (remainder == 0)
		return 0;
	most_significant_byte = order == LITTLE_ENDIAN_ORDER ? width_bytes - 1 : 0;
	unused_mask = 0xffu << remainder;
	return (buf[most_significant_byte] & unused_mask) == 0 ? 0 : -1;
}

int main(int argc, char *argv[]) {
	size_t size = 0;
	size_t width_bits = 0;
	size_t depth = 0;
	char *varname = NULL;
	coe_style_t style = NONE;
	byte_order_t order = LITTLE_ENDIAN_ORDER;
	FILE *result = stdout;
	FILE *src = NULL;
	unsigned char *buf = NULL;
	int status = 1;
	int ret;

	while ((ret = getopt(argc, argv, OPT_STRING)) != -1) {
		switch (ret) {
		case 'b':
			if (parse_positive(optarg, &size) != 0) {
				fprintf(stderr, "%s: error: invalid byte width\n", argv[0]);
				goto done;
			}
			break;
		case 'd':
			if (parse_positive(optarg, &depth) != 0) {
				fprintf(stderr, "%s: error: invalid memory depth\n", argv[0]);
				goto done;
			}
			break;
		case 'e':
			if (strcasecmp(optarg, "little") == 0)
				order = LITTLE_ENDIAN_ORDER;
			else if (strcasecmp(optarg, "big") == 0)
				order = BIG_ENDIAN_ORDER;
			else {
				fprintf(stderr, "%s: error: byte order must be little or big\n", argv[0]);
				goto done;
			}
			break;
		case 'o':
			result = fopen(optarg, "w");
			if (result == NULL) {
				fprintf(stderr, "%s: error: cannot open output file\n", argv[0]);
				goto done;
			}
			break;
		case 'f':
			if (strcasecmp(optarg, "xilinx") == 0)
				style = XILINX;
			else if (strcasecmp(optarg, "gowin") == 0)
				style = GOWIN;
			else if (strcasecmp(optarg, "asm") == 0)
				style = ASM;
			else if (strcasecmp(optarg, "smic") == 0)
				style = SMIC;
			break;
		case 's':
			varname = optarg;
			break;
		case 'w':
			if (parse_positive(optarg, &width_bits) != 0) {
				fprintf(stderr, "%s: error: invalid bit width\n", argv[0]);
				goto done;
			}
			break;
		default:
			goto done;
		}
	}
	if (style == NONE) {
		fprintf(stderr, "%s: error: no specific output style\n", argv[0]);
		goto done;
	}
	if (style == SMIC) {
		if (width_bits == 0) {
			fprintf(stderr, "%s: error: style 'smic' requires -w <bits>\n", argv[0]);
			goto done;
		}
		size = DIV_ROUND_UP(width_bits, 8);
	} else {
		if (width_bits != 0 || depth != 0 || order != LITTLE_ENDIAN_ORDER) {
			fprintf(stderr, "%s: error: -w, -d, and -e apply only to style 'smic'\n", argv[0]);
			goto done;
		}
		if (style == ASM && size == 0)
			size = 4;
		if (size == 0) {
			fprintf(stderr, "%s: error: no specific byte width\n", argv[0]);
			goto done;
		}
	}
	if (style == ASM && (size != 4 || varname == NULL)) {
		fprintf(stderr, "%s: error: style 'asm' requires -b 4 and -s <varname>\n", argv[0]);
		goto done;
	}
	if (optind >= argc) {
		fprintf(stderr, "%s: error: no input files\n", argv[0]);
		goto done;
	}

	src = fopen(argv[optind], "rb");
	if (src == NULL) {
		fprintf(stderr, "%s: error: no such file or directory\n", argv[0]);
		goto done;
	}
	size_t input_size = fsize(src);
	if (input_size == SIZE_MAX) {
		fprintf(stderr, "%s: error: cannot determine input size\n", argv[0]);
		goto done;
	}
	size_t input_words = DIV_ROUND_UP(input_size, size);
	size_t output_words = depth == 0 ? input_words : depth;
	if (input_words > output_words) {
		fprintf(stderr, "%s: error: input requires %zu words but depth is %zu\n",
			argv[0], input_words, output_words);
		goto done;
	}

	buf = calloc(size, 1);
	if (buf == NULL) {
		fprintf(stderr, "%s: error: out of memory\n", argv[0]);
		goto done;
	}
	if (style == XILINX)
		fprintf(result, "memory_initialization_radix=16;\nmemory_initialization_vector=\n");
	if (style == GOWIN)
		fprintf(result, "#File_format=Hex\n#Address_depth=%zu\n#Data_width=%zu\n",
			output_words, size * 8);
	if (style == ASM) {
		fprintf(result, "\t.type\tdata,@object\n\t.data\n\t.globl\t%s\n", varname);
		fprintf(result, "\t.p2align\t4\n%s:\n", varname);
	}

	for (size_t word = 0; word < output_words; word++) {
		size_t bytes_read;
		memset(buf, 0, size);
		bytes_read = fread(buf, 1, size, src);
		if (ferror(src)) {
			fprintf(stderr, "%s: error: failed to read input\n", argv[0]);
			goto done;
		}
		if (style == SMIC) {
			if (validate_unused_bits(buf, width_bits, order) != 0) {
				fprintf(stderr, "%s: error: word %zu has non-zero bits outside width %zu\n",
					argv[0], word, width_bits);
				goto done;
			}
			print_smic_word(result, buf, width_bits, order);
			continue;
		}
		if (bytes_read == 0)
			break;
		if (style == ASM)
			fprintf(result, "\t.4byte\t0x");
		for (size_t i = size; i > 0; i--)
			fprintf(result, "%02x", buf[i - 1]);
		if (style == XILINX && word + 1 == output_words)
			fprintf(result, ";\n");
		else
			fprintf(result, "\n");
	}
	if (style == ASM)
		fprintf(result, "\t.size\t%s, %zu\n", varname, input_size);
	status = 0;

done:
	free(buf);
	if (src != NULL)
		fclose(src);
	if (result != stdout && fclose(result) != 0)
		status = 1;
	return status;
}
