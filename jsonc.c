#include "jsonc.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>
#include <string.h>

#define ARRAY_INIT_SIZE 10
#define ARRAY_GROW_RATE 1.5

#define OBJECT_INIT_SIZE 20
#define OBJECT_GROW_RATE 2

/* TODO:
 * - implement a proper json_print
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
bool str_equals(char *a, char *b) {
	for (;;) {
		if (*a != *b)
			return false;
		if (*a == '\0')
			return true;
		a++;
		b++;
	}
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
	if (!result) {
		p->error = "out of memory";
		return 0;
	}
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
		assert(i <= len);
		p->at++;
	}

	return result;
}

static
json_node parse_node(parser_state *p);


static
uint32_t murmur3_32(char *key) {
	uint32_t c1 = 0xcc9e2d51;
	uint32_t c2 = 0x1b873593;
	uint32_t r1 = 15;
	uint32_t r2 = 13;
	uint32_t m = 5;
	uint32_t n = 0xe6546b64;

	uint32_t hash = 42; // seed

	int len = 0;
	char *s = key;
	while (*s++)
		len++;

	int nblocks = len / 4;
	uint32_t *blocks = (uint32_t *) key;
	int i;
	for (i = 0; i < nblocks; i++) {
		uint32_t k = blocks[i];
		k *= c1;
		k = (k << r1) | (k >> (32 - r1));
		k *= c2;

		hash ^= k;
		hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
	}

	uint8_t *tail = (uint8_t *) (key + nblocks * 4);
	uint32_t k1 = 0;

	switch (len & 3) {
	case 3:
		k1 ^= tail[2] << 16;
	case 2:
		k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];

		k1 *= c1;
		k1 = (k1 << r1) | (k1 >> (32 - r1));
		k1 *= c2;
		hash ^= k1;
	}

	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);

	return hash;
}

static
json_object_entry *object_find_bucket(json_object *obj, char *key) {
	uint32_t hash = murmur3_32(key);
	uint32_t bucket = hash % obj->capacity;
	json_object_entry *result = 0;

	// linear probing
	for (uint32_t i = bucket; i < obj->capacity; i++) {
		json_object_entry *entry = obj->buckets + i;
		if (!entry->key || str_equals(key, entry->key)) {
			result = entry;
			break;
		}
	}

	return result;
}

static
void object_rehash(json_object *obj) {
	json_object new_obj = {};
	new_obj.capacity = OBJECT_GROW_RATE * obj->capacity;
	new_obj.buckets = calloc(new_obj.capacity, sizeof(json_object_entry));
	if (!new_obj.buckets)
		return;

	for (uint32_t i = 0; i < obj->capacity; i++) {
		json_object_entry *old_entry = obj->buckets + i;
		if (old_entry->key) {
			json_object_entry *new_entry = object_find_bucket(&new_obj, old_entry->key);
			new_entry->key = old_entry->key;
			new_entry->value = old_entry->value;
		}
	}

	free(obj->buckets);
	*obj = new_obj;
}

static
bool object_set(json_object *obj, char *key, json_node value) {
	json_object_entry *entry = object_find_bucket(obj, key);
	while (!entry) {
		object_rehash(obj);
		if (!obj->buckets)
			return false;
		entry = object_find_bucket(obj, key);
	}
	if (entry->key) // replacing existing key
		free(entry->key);

	entry->key = key;
	entry->value = value;
	return true;
}

extern
json_node *json_get(json_object *obj, char *key) {
	json_object_entry *entry = object_find_bucket(obj, key);
	if (entry->key) {
		return &entry->value;
	} else {
		return 0;
	}
}

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

	result.capacity = OBJECT_INIT_SIZE;
	result.buckets = calloc(result.capacity, sizeof(json_object_entry));
	if (!result.buckets) {
		p->error = "out of memory";
		return result;
	}

	for (;;) {
		eat_whitespace(p);
		if (*p->at == '}') {
			p->at++;
			break;
		} else if (*p->at == '"') {
			char *key = parse_string(p);
			if (p->error)
				return result;

			eat_whitespace(p);
			if (*p->at != ':') {
				p->error = "expected colon after property key";
				return result;
			}
			p->at++;
			eat_whitespace(p);

			json_node value = parse_node(p);
			if (p->error)
				return result;

			if (!object_set(&result, key, value)) {
				p->error = "out of memory";
				return result;
			}

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

	uint32_t capacity = ARRAY_INIT_SIZE;
	result.elements = malloc(capacity * sizeof(json_node));
	if (!result.elements) {
		p->error = "out of memory";
		return result;
	}

	for (;;) {
		eat_whitespace(p);
		if (*p->at == ']') {
			p->at++;
			break;
		}

		if (result.count >= capacity) {
			capacity *= ARRAY_GROW_RATE;
			result.elements = realloc(result.elements,
				sizeof(json_node) * capacity);
			if (!result.elements) {
				p->error = "out of memory";
				return result;
			}
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
		result.object = parse_object(p);
	} else if (*p->at == '[') {
		result.type = JSON_ARRAY;
		result.array = parse_array(p);
	} else if (*p->at == '"') {
		result.type = JSON_STRING;
		result.string = parse_string(p);
	} else if (*p->at == '-' || (*p->at >= '0' && *p->at <= '9')) {
		result.type = JSON_NUMBER;
		result.number = parse_number(p);
	} else if (str_start_with(p->at, "true")) {
		p->at += 4;
		result.type = JSON_BOOL;
		result.boolean = true;
	} else if (str_start_with(p->at, "false")) {
		p->at += 5;
		result.type = JSON_BOOL;
		result.boolean = false;
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
		json_object object = node->object;
		for (int i = 0; i < object.capacity; i++) {
			json_object_entry e = object.buckets[i];
			if (e.key) {
				free(e.key);
				json_free(&e.value);
			}
		}
		free(object.buckets);
	} break;
	case JSON_ARRAY: {
		json_array array = node->array;
		for (int i = 0; i < array.count; i++) {
			json_free(array.elements + i);
		}
		free(array.elements);
	} break;
	case JSON_STRING:
		free(node->string);
		break;
	default:
		;
	}
}

#if 1
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
		json_object object = node.object;
		printf("{\n");
		indent++;
		bool first = true;
		for (uint32_t i = 0; i < object.capacity; i++) {
			json_object_entry *entry = object.buckets + i;
			if (!entry->key)
				continue;
			if (first) {
				first = false;
			} else {
				printf(",\n");
			}
			ind(indent);
			printf("\"%s\": ", entry->key);
			print_indented(entry->value, indent);
		}
		printf("\n");
		indent--;
		ind(indent);
		printf("}");
	} break;
	case JSON_ARRAY: {
		json_array array = node.array;

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
		printf("\"%s\"", node.string);
		break;
	case JSON_NUMBER:
		printf("%f", node.number);
		break;
	case JSON_BOOL:
		if (node.boolean)
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
#endif
