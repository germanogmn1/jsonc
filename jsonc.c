#include "jsonc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <float.h>

static
bool str_start_with(char *str, char *substr) {
	while (*substr) {
		if (*str++ != *substr++)
			return false;
	}
	return true;
}

static
void eat_whitespace(char **p) {
	while (**p == ' ' || **p == '\t' || **p == '\n')
		(*p)++;
}

static
char *parse_string(char **p) {
	char *result = malloc(sizeof(char) * 100);
	size_t i = 0;

	assert(**p == '"');
	(*p)++;

	for (;;) {
		if (**p < ' ') {
			assert(!"control characters not permitted");
		} if (**p == '\\') {
			(*p)++;
			switch (**p) {
			case '"':
			case '\\':
			case '/':
				result[i++] = **p;
				break;
			case 'b':
				result[i++] = '\b';
				break;
			case 'f':
				result[i++] = '\f';
				break;
			case 'n':
				result[i++] = '\n';
				break;
			case 'r':
				result[i++] = '\r';
				break;
			case 't':
				result[i++] = '\t';
				break;
			case 'u': {
				uint16_t codepoint = 0;
				for (int i = 3; i >= 0; i--) {
					(*p)++;
					int digit;
					if (**p >= '0' && **p <= '9') {
						digit = **p - '0';
					} else if (**p >= 'a' && **p <= 'f') {
						digit = 10 + (**p - 'a');
					} else if (**p >= 'A' && **p <= 'F') {
						digit = 10 + (**p - 'A');
					} else {
						assert(!"invalid hex digit");
					}
					codepoint += digit << (i * 4);
				}

				// UTF-8 encoding
				if (codepoint <= 0x7f) {
					result[i++] = (char)codepoint;
				} else if (codepoint <= 0x7ff) {
					uint8_t b1 = 3 << 6;
					b1 += (codepoint & 0x7c0) >> 6;
					uint8_t b2 = 1 << 7;
					b2 += codepoint & 0x3f;

					result[i++] = b1;
					result[i++] = b2;
				} else {
					uint8_t b1 = 7 << 5;
					b1 += (codepoint & 0xf000) >> 12;
					uint8_t b2 = 1 << 7;
					b2 += (codepoint & 0xfc0) >> 6;
					uint8_t b3 = 1 << 7;
					b3 += codepoint & 0x3f;

					result[i++] = b1;
					result[i++] = b2;
					result[i++] = b3;
				}
			} break;
			default:
				assert(!"unknown escape sequence");
			}
		} else if (**p == '"') {
			result[i++] = '\0';
			(*p)++;
			break;
		} else {
			result[i++] = **p;
		}
		(*p)++;
	}

	return result;
}

static
json_node parse_node(char **p);

static
json_object parse_object(char **p) {
	json_object result = {};
	result.entries = malloc(sizeof(json_object_entry) * 100);

	assert(**p == '{');
	(*p)++;

	for (;;) {
		eat_whitespace(p);
		if (**p == '}') {
			(*p)++;
			break;
		} else if (**p == '"') {
			json_object_entry *entry = result.entries + result.count++;
			entry->key = parse_string(p);
			eat_whitespace(p);
			assert(**p == ':');
			(*p)++;
			eat_whitespace(p);
			entry->value = parse_node(p);
			eat_whitespace(p);
			if (**p == ',') {
				(*p)++;
			} else if (**p != '}') {
				assert(!"expected comma or end of object");
			}
		} else {
			assert(!"expected string with object key");
		}
	}
	return result;
}

static
json_array parse_array(char **p) {
	json_array result = {};
	result.elements = malloc(sizeof(json_node) * 100);
	assert(**p == '[');
	(*p)++;

	for (;;) {
		eat_whitespace(p);
		if (**p == ']') {
			(*p)++;
			break;
		}
		json_node *elem = result.elements + result.count++;
		*elem = parse_node(p);

		eat_whitespace(p);
		if (**p == ',') {
			(*p)++;
		} else if (**p != ']') {
			assert(!"expected comma or end of array");
		}
	}

	return result;
}

static
double parse_number(char **p) {
	int sign;
	if (**p == '-') {
		sign = -1;
		(*p)++;
	} else {
		sign = 1;
	}

	int number;
	if (**p == '0') {
		number = 0;
		(*p)++;
	} else if (**p >= '1' && **p <= '9') {
		while (**p >= '0' && **p <= '9') {
			number = number * 10 + (**p - '0');
			(*p)++;
		}
	} else {
		assert(!"invalid number");
	}

	int decimal_digits = 0;
	if (**p == '.') {
		(*p)++;
		while (**p >= '0' && **p <= '9') {
			number = number * 10 + (**p - '0');
			(*p)++;
			++decimal_digits;
		}
		assert(decimal_digits > 0);
	}

	if (**p == 'e' || **p == 'E') {
		(*p)++;
		if (**p == '+' || **p == '-') {
			(*p)++;
			// parse sign
		}

		if (**p >= '0' && **p <= '9') {
			// parse number
		} else {
			assert(!"invalid exponent");
		}
	}

	// DBL_MIN_EXP DBL_MAX_EXP
	return 3.5;
}

static
json_node parse_node(char **p) {
	json_node result = {};

	if (**p == '{') {
		result.type = JSON_OBJECT;
		result.value.object = parse_object(p);
	} else if (**p == '[') {
		result.type = JSON_ARRAY;
		result.value.array = parse_array(p);
	} else if (**p == '"') {
		result.type = JSON_STRING;
		result.value.string = parse_string(p);
	} else if (**p == '-' || (**p >= '0' && **p <= '9')) {
		result.type = JSON_NUMBER;
		result.value.number = parse_number(p);
	} else if (str_start_with(*p, "true")) {
		*p += 4;
		result.type = JSON_BOOL;
		result.value.boolean = true;
	} else if (str_start_with(*p, "false")) {
		*p += 5;
		result.type = JSON_BOOL;
		result.value.boolean = false;
	} else if (str_start_with(*p, "null")) {
		*p += 4;
		result.type = JSON_NULL;
	} else {
		assert(!"invalid JSON value");
	}

	return result;
}

extern
json_node json_parse(char *data) {
	char **p = &data;
	eat_whitespace(p);

	json_node root = {};
	if (**p == '{' || **p == '[') {
		root = parse_node(p);
	} else {
		assert(!"root object must be object or array");
	}

	return root;
}

static
void ind(size_t i) {
	while (i--) printf("\t");
}

static
void print_indented(json_node node, size_t indent) {
	switch (node.type) {
	case JSON_NULL:
		printf("null");
		break;
	case JSON_OBJECT: {
		json_object object = node.value.object;
		ind(indent);
		printf("{\n");
		indent++;
		for (size_t i = 0; i < object.count; i++) {
			json_object_entry *entry = object.entries + i;
			ind(indent);
			printf("\"%s\": ", entry->key);
			print_indented(entry->value, indent);
			if (i < object.count - 1)
				printf(",\n");
			else
				printf("\n");
		}
		indent--;
		ind(indent);
		printf("}");
	} break;
	case JSON_ARRAY: {
		json_array array = node.value.array;

		ind(indent);
		printf("[\n");
		indent++;

		for (size_t i = 0; i < array.count; i++) {
			json_node *elem = array.elements + i;
			ind(indent);
			print_indented(*elem, indent);

			if (i < array.count - 1)
				printf(",\n");
			else
				printf("\n");
		}

		indent--;
		ind(indent);
		printf("]");
	} break;
	case JSON_STRING:
		printf("\"%s\"", node.value.string);
		break;
	case JSON_NUMBER:
		printf("%f", node.value.number);
		break;
	case JSON_BOOL:
		if (node.value.boolean)
			printf("true");
		else
			printf("false");
		break;
	}
}

extern
void json_print(json_node json) {
	print_indented(json, 0);
	printf("\n");
}
