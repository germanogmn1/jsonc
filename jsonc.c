#include "jsonc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <float.h>
#include <assert.h>

#define FIRST_ALLOC 5
#define GROW_RATE 1.5

/* TODO:
 * - store object as hash map
 */

typedef struct {
	char *at;
	char *error;
	char *line_start;
	size_t line;
} parser_state;

#define ERROR_MESSAGE_SIZE 100
static char error_message[ERROR_MESSAGE_SIZE];

extern
char *json_get_error() {
	return error_message;
}

static
bool str_start_with(char *str, char *substr) {
	while (*substr) {
		if (*str++ != *substr++)
			return false;
	}
	return true;
}

static
void eat_whitespace(parser_state *p) {
	while (*p->at == ' ' || *p->at == '\t' || *p->at == '\n') {
		if (*p->at++ == '\n') {
			p->line++;
			p->line_start = p->at;
		}
	}
}

static
char *parse_string(parser_state *p) {
	if (*p->at != '"') {
		p->error = "invalid token at start of string";
		return 0;
	}
	p->at++;

	// get rough size of string for allocation
	size_t len = 0;
	for (char *s = p->at; *s != '"'; s++) {
		if (*s == '\\')
			s++;
		len++;
	}

	char *result = malloc(sizeof(char) * len + 1);
	size_t i = 0;

	for (;;) {
		if (*p->at < ' ') {
			p->error = "control characters not permitted";
			return 0;
		} if (*p->at == '\\') {
			p->at++;
			switch (*p->at) {
			case '"':
			case '\\':
			case '/':
				result[i++] = *p->at;
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
					p->at++;
					int digit;
					if (*p->at >= '0' && *p->at <= '9') {
						digit = *p->at - '0';
					} else if (*p->at >= 'a' && *p->at <= 'f') {
						digit = 10 + (*p->at - 'a');
					} else if (*p->at >= 'A' && *p->at <= 'F') {
						digit = 10 + (*p->at - 'A');
					} else {
						p->error = "invalid hex digit";
						return 0;
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
				p->error = "unknown escape sequence";
				return 0;
			}
		} else if (*p->at == '"') {
			result[i++] = '\0';
			p->at++;
			break;
		} else {
			result[i++] = *p->at;
		}
		// assert(i <= len);
		p->at++;
	}

	return result;
}

static
json_node parse_node(parser_state *p);

static
json_object parse_object(parser_state *p) {
	json_object result = {};

	if (*p->at != '{') {
		p->error = "invalid token at start of object";
		return result;
	}
	p->at++;

	if (*p->at == '}') {
		p->at++;
		return result;
	}

	size_t capacity = FIRST_ALLOC;
	result.entries = malloc(sizeof(json_object_entry) * capacity);

	for (;;) {
		eat_whitespace(p);
		if (*p->at == '}') {
			p->at++;
			break;
		} else if (*p->at == '"') {
			if (result.count >= capacity) {
				capacity *= GROW_RATE;
				printf("realloc to %zu\n", capacity);
				result.entries = realloc(result.entries,
					sizeof(json_object_entry) * capacity);
			}
			json_object_entry *entry = result.entries + result.count++;
			entry->key = parse_string(p);
			if (p->error)
				return result;

			eat_whitespace(p);
			if (*p->at != ':') {
				p->error = "expected colon after property key";
				return result;
			}
			p->at++;
			eat_whitespace(p);
			entry->value = parse_node(p);
			if (p->error)
				return result;

			eat_whitespace(p);
			if (*p->at == ',') {
				p->at++;
			} else if (*p->at != '}') {
				p->error = "expected comma or end of object";
				return result;
			}
		} else {
			p->error = "expected string with object key";
			return result;
		}
	}
	return result;
}

static
json_array parse_array(parser_state *p) {
	json_array result = {};

	if (*p->at != '[') {
		p->error = "invalid token at start of array";
		return result;
	}
	p->at++;

	if (*p->at == ']') {
		p->at++;
		return result;
	}

	size_t capacity = FIRST_ALLOC;
	result.elements = malloc(sizeof(json_node) * capacity);

	for (;;) {
		eat_whitespace(p);
		if (*p->at == ']') {
			p->at++;
			break;
		}

		if (result.count >= capacity) {
			capacity *= GROW_RATE;
			printf("realloc to %zu\n", capacity);
			result.elements = realloc(result.elements,
				sizeof(json_node) * capacity);
		}

		json_node *elem = result.elements + result.count++;
		*elem = parse_node(p);
		if (p->error)
			return result;

		eat_whitespace(p);
		if (*p->at == ',') {
			p->at++;
		} else if (*p->at != ']') {
			p->error = "expected comma or end of array";
			return result;
		}
	}

	return result;
}

static
double parse_number(parser_state *p) {
	bool negative = false;
	if (*p->at == '-') {
		negative = true;
		p->at++;
	}

	double number = 0;
	if (*p->at == '0') {
		number = 0;
		p->at++;
	} else if (*p->at >= '1' && *p->at <= '9') {
		while (*p->at >= '0' && *p->at <= '9') {
			number = number * 10 + (*p->at - '0');
			p->at++;
		}
	} else {
		p->error = "invalid number";
		return number;
	}

	int decimal_digits = 0;
	int exponent = 0;
	if (*p->at == '.') {
		p->at++;
		if (!(*p->at >= '0' && *p->at <= '9')) {
			p->error = "expected number after decimal separator";
			return number;
		}
		while (*p->at >= '0' && *p->at <= '9') {
			number = number * 10 + (*p->at - '0');
			p->at++;
			++decimal_digits;
		}
		exponent -= decimal_digits;
	}

	if (negative)
		number = -number;

	if (*p->at == 'e' || *p->at == 'E') {
		p->at++;
		if (*p->at == '+') {
			negative = false;
			p->at++;
		} else if (*p->at == '-') {
			negative = true;
			p->at++;
		}

		if (*p->at >= '0' && *p->at <= '9') {
			int n = 0;
			while (*p->at >= '0' && *p->at <= '9') {
				n = n * 10 + (*p->at - '0');
				p->at++;
			}
			if (negative) {
				exponent -= n;
			} else {
				exponent += n;
			}
		} else {
			p->error = "invalid exponent";
			return number;
		}
	}

	if (exponent < DBL_MIN_EXP) {
		p->error = "number exponent too low";
		return number;
	} else if (exponent > DBL_MAX_EXP) {
		p->error = "number exponent too high";
		return number;
	}

	if (exponent < 0) {
		for (int i = exponent; i < 0; i++)
			number /= 10;
	} else {
		for (int i = 0; i < exponent; i++)
			number *= 10;
	}

	return number;
}

static
json_node parse_node(parser_state *p) {
	json_node result = {};

	if (*p->at == '{') {
		result.type = JSON_OBJECT;
		result.value.object = parse_object(p);
	} else if (*p->at == '[') {
		result.type = JSON_ARRAY;
		result.value.array = parse_array(p);
	} else if (*p->at == '"') {
		result.type = JSON_STRING;
		result.value.string = parse_string(p);
	} else if (*p->at == '-' || (*p->at >= '0' && *p->at <= '9')) {
		result.type = JSON_NUMBER;
		result.value.number = parse_number(p);
	} else if (str_start_with(p->at, "true")) {
		p->at += 4;
		result.type = JSON_BOOL;
		result.value.boolean = true;
	} else if (str_start_with(p->at, "false")) {
		p->at += 5;
		result.type = JSON_BOOL;
		result.value.boolean = false;
	} else if (str_start_with(p->at, "null")) {
		p->at += 4;
		result.type = JSON_NULL;
	} else {
		p->error = "invalid JSON value";
		return result;
	}

	return result;
}

extern
bool json_parse(char *data, json_node *out_json) {
	parser_state p = {};
	p.at = data;
	p.line = 1;
	p.line_start = p.at;
	eat_whitespace(&p);

	json_node root = {};
	if (*p.at == '{' || *p.at == '[') {
		root = parse_node(&p);
	} else {
		p.error = "root object must be object or array";
	}

	if (!p.error) {
		eat_whitespace(&p);
		if (*p.at)
			p.error = "expected end of string";
	}

	if (p.error) {
		size_t col = p.at - p.line_start + 1;
		snprintf(error_message, ERROR_MESSAGE_SIZE, "%s (line %zu, col %zu)",
			p.error, p.line, col);
		return false;
	}

	*out_json = root;
	return true;
}

extern
void json_free(json_node *node) {
	switch (node->type) {
	case JSON_OBJECT: {
		json_object object = node->value.object;
		for (int i = 0; i < object.count; i++) {
			json_object_entry e = object.entries[i];
			free(e.key);
			json_free(&e.value);
		}
		free(object.entries);
	} break;
	case JSON_ARRAY: {
		json_array array = node->value.array;
		for (int i = 0; i < array.count; i++) {
			json_free(array.elements + i);
		}
		free(array.elements);
	} break;
	case JSON_STRING:
		free(node->value.string);
		break;
	default:
		;
	}
}

// debug

static
void ind(size_t i) {
	while (i--)	printf("\t");
}

static
void print_indented(json_node node, size_t indent) {
	switch (node.type) {
	case JSON_NULL:
		printf("null");
		break;
	case JSON_OBJECT: {
		json_object object = node.value.object;
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
