#include <stdbool.h>
#include <stddef.h>

typedef enum {
	JSON_NULL,
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_NUMBER,
	JSON_BOOL,
} json_node_type;

struct json_object_entry;
struct json_node;

typedef struct {
	struct json_object_entry *entries;
	size_t count;
} json_object;

typedef struct {
	struct json_node *elements;
	size_t count;
} json_array;

typedef struct json_node {
	json_node_type type;
	union {
		json_object object;
		char *string;
		json_array array;
		bool boolean;
		double number;
	} value;
} json_node;

typedef struct json_object_entry {
	char *key;
	json_node value;
} json_object_entry;

typedef struct {
	char *error;
	json_node json;
} json_parse_result;

char* json_get_error();
bool json_parse(char *data, json_node *out_json);
void json_print(json_node json);
