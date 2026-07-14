#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define OPT_STRING "b:d:e:f:o:s:w:"
#define DIV_ROUND_UP(n, d) ((n) / (d) + ((n) % (d) != 0))

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

static int get_value_bit(const unsigned char *buf, size_t width_bytes,
			 size_t value_bit, byte_order_t order) {
	size_t value_byte = value_bit / 8;
	size_t input_byte = order == LITTLE_ENDIAN_ORDER
		? value_byte
		: width_bytes - 1 - value_byte;
	return (buf[input_byte] & (1u << (value_bit % 8))) != 0;
}

static void print_binary_word(FILE *result, const unsigned char *buf,
			      size_t width_bits, byte_order_t order) {
	size_t width_bytes = DIV_ROUND_UP(width_bits, 8);
	for (size_t output_bit = width_bits; output_bit > 0; output_bit--) {
		size_t value_bit = output_bit - 1;
		fputc(get_value_bit(buf, width_bytes, value_bit, order) ? '1' : '0', result);
	}
}

static void print_hex_word(FILE *result, const unsigned char *buf,
			   size_t width_bits, byte_order_t order) {
	static const char digits[] = "0123456789abcdef";
	size_t width_bytes = DIV_ROUND_UP(width_bits, 8);
	size_t output_digits = DIV_ROUND_UP(width_bits, 4);

	for (size_t output_digit = output_digits; output_digit > 0; output_digit--) {
		size_t first_bit = (output_digit - 1) * 4;
		unsigned int value = 0;
		for (size_t bit = 0; bit < 4 && first_bit + bit < width_bits; bit++)
			if (get_value_bit(buf, width_bytes, first_bit + bit, order))
				value |= 1u << bit;
		fputc(digits[value], result);
	}
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

static void print_header(FILE *result, coe_style_t style, size_t output_words,
			 size_t width_bits, const char *varname) {
	if (style == XILINX)
		fprintf(result, "memory_initialization_radix=16;\nmemory_initialization_vector=\n");
	else if (style == GOWIN)
		fprintf(result, "#File_format=Hex\n#Address_depth=%zu\n#Data_width=%zu\n",
			output_words, width_bits);
	else if (style == ASM) {
		fprintf(result, "\t.type\tdata,@object\n\t.data\n\t.globl\t%s\n", varname);
		fprintf(result, "\t.p2align\t4\n%s:\n", varname);
	}
}

static void print_word(FILE *result, coe_style_t style, const unsigned char *buf,
		       size_t width_bits, byte_order_t order, int last) {
	if (style == ASM)
		fprintf(result, "\t.4byte\t0x");

	if (style == SMIC)
		print_binary_word(result, buf, width_bits, order);
	else
		print_hex_word(result, buf, width_bits, order);

	if (style == XILINX)
		fprintf(result, last ? ";\n" : ",\n");
	else
		fputc('\n', result);
}

static void print_footer(FILE *result, coe_style_t style, const char *varname,
			 size_t input_size) {
	if (style == ASM)
		fprintf(result, "\t.size\t%s, %zu\n", varname, input_size);
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
	if (style == ASM && width_bits != 0) {
		fprintf(stderr, "%s: error: style 'asm' does not support -w\n", argv[0]);
		goto done;
	}
	if (style == ASM && (depth != 0 || order != LITTLE_ENDIAN_ORDER)) {
		fprintf(stderr, "%s: error: style 'asm' does not support -d or -e\n", argv[0]);
		goto done;
	}
	if (style == ASM && size == 0)
		size = 4;
	if (style == ASM && (size != 4 || varname == NULL)) {
		fprintf(stderr, "%s: error: style 'asm' requires -b 4 and -s <varname>\n", argv[0]);
		goto done;
	}
	if (style != ASM) {
		if (width_bits != 0 && size != 0) {
			fprintf(stderr, "%s: error: specify either -w <bits> or -b <bytes>, not both\n",
				argv[0]);
			goto done;
		}
		if (width_bits == 0) {
			if (size == 0) {
				fprintf(stderr, "%s: error: no specific word width\n", argv[0]);
				goto done;
			}
			if (size > SIZE_MAX / 8) {
				fprintf(stderr, "%s: error: byte width is too large\n", argv[0]);
				goto done;
			}
			width_bits = size * 8;
		} else
			size = DIV_ROUND_UP(width_bits, 8);
	}
	if (style == ASM)
		width_bits = 32;
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
	if (input_size % size != 0) {
		fprintf(stderr, "%s: error: input size is not aligned to the %zu-byte word width\n",
			argv[0], size);
		goto done;
	}
	size_t input_words = input_size / size;
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
	print_header(result, style, output_words, width_bits, varname);

	for (size_t word = 0; word < output_words; word++) {
		memset(buf, 0, size);
		if (word < input_words && fread(buf, 1, size, src) != size) {
			fprintf(stderr, "%s: error: failed to read input\n", argv[0]);
			goto done;
		}
		if (validate_unused_bits(buf, width_bits, order) != 0) {
			fprintf(stderr, "%s: error: word %zu has non-zero bits outside width %zu\n",
				argv[0], word, width_bits);
			goto done;
		}
		print_word(result, style, buf, width_bits, order,
			   word + 1 == output_words);
	}
	print_footer(result, style, varname, input_size);
	status = 0;

done:
	free(buf);
	if (src != NULL)
		fclose(src);
	if (result != stdout && fclose(result) != 0)
		status = 1;
	return status;
}
