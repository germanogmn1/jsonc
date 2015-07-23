#include "jsonc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
			++parser->top;
		else
			break;
	}
}

static
char *parse_string(parser_state *parser) {
	char *result = malloc(sizeof(char) * 100);
	size_t i = 0;

	for (;;) {
		char c = *parser->top++;
		if (c == '\\') {
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
			case 'u':
				// TODO: unicode
				break;
			default:
				assert(0);
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

	for (;;) {
		eat_whitespace(parser);
		char c = *parser->top++;
		if (c == '}') {
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
			assert(0);
		}
	}
	return result;
}

static
json_array parse_array(parser_state *parser) {
	json_array result = {};
	result.elements = malloc(sizeof(json_node) * 100);

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
json_node parse_node(parser_state *parser) {
	json_node result = {};

	// TODO: parse number

	char c = *parser->top++;
	if (c == '{') {
		result.type = JSON_OBJECT;
		result.value.object = parse_object(parser);
	} else if (c == '[') {
		result.type = JSON_ARRAY;
		result.value.array = parse_array(parser);
	} else if (c == '"') {
		result.type = JSON_STRING;
		result.value.string = parse_string(parser);
	} else if (str_start_with(parser->top - 1, "true")) {
		parser->top += 3;
		result.type = JSON_BOOL;
		result.value.boolean = true;
	} else if (str_start_with(parser->top - 1, "false")) {
		parser->top += 4;
		result.type = JSON_BOOL;
		result.value.boolean = false;
	} else if (str_start_with(parser->top - 1, "null")) {
		parser->top += 3;
		result.type = JSON_NULL;
	} else {
		assert(0);
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
		assert(0);
	}

	return root;
}
static
void ind(int i) {
	while (i--) printf("\t");
}

static
void print_indented(json_node node, int indent) {
	switch (node.type) {
	case JSON_NULL:
		printf("null");
		break;
	case JSON_OBJECT: {
		json_object object = node.value.object;
		ind(indent);
		printf("{\n");
		indent++;
		for (int i = 0; i < object.count; i++) {
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

		for (int i = 0; i < array.count; i++) {
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
		printf(".");
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

