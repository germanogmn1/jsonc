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
} json_type;

struct json_entry_t;
struct json_t;

typedef struct {
	struct json_entry_t *buckets;
	uint32_t capacity;
} json_object_t;

typedef struct {
	struct json_t *elements;
	uint32_t count;
	uint32_t capacity;
} json_array_t;

typedef struct json_t {
	json_type type;
	union {
		json_object_t object;
		json_array_t array;
		char *string;
		bool boolean;
		double number;
	};
} json_t;

typedef struct json_entry_t {
	char *key;
	json_t value;
} json_entry_t;

char *json_get_error();
bool json_parse(char *data, json_t *out_json);
void json_free(json_t *node);

bool json_append(json_array_t *array, json_t value);
json_t *json_get(json_object_t *obj, char *key);
bool json_set(json_object_t *obj, char *key, json_t value);

size_t json_generate(json_t *node, char **out, char *indent);

json_t json_str(char *str);
json_t json_array();
json_t json_object();
json_t json_number(double n);
json_t json_bool(bool val);
json_t json_null();
