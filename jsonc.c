#include "jsonc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

/* TODO:
 * - parse number
 * - get number as int or float
 * - error handling
 * - proper memory management
 * - store object as hash map
 */

typedef struct {
	char *data;
	char *top;
} parser_state;

static
bool str_start_with(char *str, char *substr) {
	while (*substr) {
		if (*str++ != *substr++)
			return false;
	}
	return true;
}

static
void eat_whitespace(parser_state *parser) {
	for (;;) {
		char c = *parser->top;
		if (c == ' ' || c == '\t' || c == '\n')
			parser->top++;
		else
			break;
	}
}

static
uint8_t hex_to_num(char c) {
	uint8_t result;
	if (c >= '0' && c <= '9')
		result = c - '0';
	else if (c >= 'a' && c <= 'f')
		result = 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		result = 10 + c - 'A';
	else
		assert(!"invalid hex digit");
	return result;
}

static
char *parse_string(parser_state *parser) {
	char *result = malloc(sizeof(char) * 100);
	size_t i = 0;

	assert(*parser->top == '"');
	parser->top++;

	for (;;) {
		char c = *parser->top++;
		if (c < ' ') {
			assert(!"control characters not permitted");
		} if (c == '\\') {
			c = *parser->top++;
			switch (c) {
			case '"':
			case '\\':
			case '/':
				result[i++] = c;
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
				// TODO: check end of string
				char a = parser->top[0];
				char b = parser->top[1];
				char c = parser->top[2];
				char d = parser->top[3];
				parser->top += 4;

				uint16_t codepoint = hex_to_num(d) +
					(hex_to_num(c) << 4) +
					(hex_to_num(b) << 8) +
					(hex_to_num(a) << 12);

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
		} else if (c == '"') {
			result[i++] = '\0';
			break;
		} else {
			result[i++] = c;
		}
	}

	// printf("## %s ##\n", result);
	return result;
}

static
json_node parse_node(parser_state *parser);

static
json_object parse_object(parser_state *parser) {
	json_object result = {};
	result.entries = malloc(sizeof(json_object_entry) * 100);

	assert(*parser->top == '{');
	parser->top++;

	for (;;) {
		eat_whitespace(parser);
		char c = *parser->top;
		if (c == '}') {
			parser->top++;
			break;
		} else if (c == '"') {
			json_object_entry *entry = result.entries + result.count++;
			entry->key = parse_string(parser);
			eat_whitespace(parser);
			assert(*parser->top == ':');
			parser->top++;
			eat_whitespace(parser);
			entry->value = parse_node(parser);
			eat_whitespace(parser);
			// printf("\n~~%s~~\n", parser->top);
			char c = *parser->top++;
			if (c == '}')
				break;
			assert(c == ',');
		} else {
			assert(!"expected string with object key");
		}
	}
	return result;
}

static
json_array parse_array(parser_state *parser) {
	json_array result = {};
	result.elements = malloc(sizeof(json_node) * 100);
	assert(*parser->top == '[');
	parser->top++;

	for (;;) {
		eat_whitespace(parser);
		if (*parser->top == ']') {
			parser->top++;
			break;
		}
		json_node *elem = result.elements + result.count++;
		*elem = parse_node(parser);

		eat_whitespace(parser);
		char c = *parser->top++;
		if (c == ']')
			break;
		assert(c == ',');
	}

	return result;
}

static
double parse_number(parser_state *parser) {
	char c = *parser->top;
	bool negative = false;
	if (c == '-') {
		negative = true;
		parser->top++;
	}
	while (c == '.' || (c >= '0' && c <= '9')) {
		c = *++parser->top;
	}
	return 3.0;
}

static
json_node parse_node(parser_state *parser) {
	json_node result = {};

	char c = *parser->top;
	if (c == '{') {
		result.type = JSON_OBJECT;
		result.value.object = parse_object(parser);
	} else if (c == '[') {
		result.type = JSON_ARRAY;
		result.value.array = parse_array(parser);
	} else if (c == '"') {
		result.type = JSON_STRING;
		result.value.string = parse_string(parser);
	} else if (c == '-' || (c >= '0' && c <= '9')) {
		result.type = JSON_NUMBER;
		result.value.number = parse_number(parser);
	} else if (str_start_with(parser->top, "true")) {
		parser->top += 4;
		result.type = JSON_BOOL;
		result.value.boolean = true;
	} else if (str_start_with(parser->top, "false")) {
		parser->top += 5;
		result.type = JSON_BOOL;
		result.value.boolean = false;
	} else if (str_start_with(parser->top, "null")) {
		parser->top += 4;
		result.type = JSON_NULL;
	} else {
		assert(!"invalid JSON value");
	}

	return result;
}

extern
json_node json_parse(char *data) {
	parser_state parser;
	parser.data = data;
	parser.top = data;

	eat_whitespace(&parser);

	json_node root = {};
	char c = *parser.top;
	if (c == '{' || c == '[') {
		root = parse_node(&parser);
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
