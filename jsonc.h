#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	JSON_NULL,
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOL,
} json_node_type;

typedef enum {
	JSON_SUCCESS,
	JSON_ESYNTAX,
	JSON_EALLOC,
} json_error;

struct json_object_entry;
struct json_node;

typedef struct {
	struct json_object_entry *buckets;
	uint32_t capacity;
} json_object;

typedef struct {
	struct json_node *elements;
	uint32_t count;
} json_array;

typedef struct json_node {
	json_node_type type;
	union {
		json_object object;
		json_array array;
		char *string;
		bool boolean;
		double number;
	};
} json_node;

typedef struct json_object_entry {
	char *key;
	json_node value;
} json_object_entry;

typedef struct {
	char *error;
	json_node json;
} json_parse_result;

char *json_get_error();
bool json_parse(char *data, json_node *out_json);
void json_free(json_node *node);
json_node *json_get(json_object *obj, char *key);
size_t json_generate(json_node *node, char **out, char *indent);
