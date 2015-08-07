#include <stdio.h>
#include <x86intrin.h>
#include "jsonc.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#define BUFFER_SIZE 1024 * 1024
char buffer[BUFFER_SIZE];

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "error: missing json file parameter\n");
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("error to open file");
		return 1;
	}
	read(fd, buffer, BUFFER_SIZE);

	json_t node;
	uint64_t start = __rdtsc();
	bool ok = json_parse(buffer, &node);
	uint64_t parse_duration = __rdtsc() - start;

	uint64_t gen_duration = 0;
	if (ok) {
		{
			printf("\n--- Parsing and printing JSON ---\n\n");
			char *enc;

			start = __rdtsc();
			json_generate(&node, &enc, "    ");
			gen_duration = __rdtsc() - start;

			printf("%s\n", enc);
			free(enc);
		}

		{
			printf("\n--- Querying JSON ---\n\n");
			assert(node.type == JSON_ARRAY);
			for (uint32_t i = 0; i < node.array.count; i++) {
				json_t element = node.array.elements[i];
				assert(element.type == JSON_OBJECT);
				json_object_t obj = element.object;
				json_t* friends = json_get(&obj, "friends");
				assert(friends);
				assert(friends->type == JSON_ARRAY);
				for (uint32_t j = 0; j < friends->array.count; j++) {
					json_t e = friends->array.elements[j];
					assert(e.type == JSON_OBJECT);
					json_t* name = json_get(&e.object, "name");
					assert(name);
					assert(name->type == JSON_STRING);
					printf("* %s\n", name->string);
				}
			}
		}
		{
			printf("\n--- Generate JSON ---\n\n");

			json_t root = json_object();
			json_set(&root.object, "name", json_str("Lorem Ipsum"));
			json_set(&root.object, "foo", json_number(42));
			json_t arr = json_array();
			json_append(&arr.array, json_null());
			json_append(&arr.array, json_bool(true));
			json_append(&arr.array, json_str("fooes"));
			json_append(&arr.array, json_object());
			json_set(&root.object, "array", arr);

			char *res;
			json_generate(&root, &res, "  ");
			json_free(&root);
			printf("%s\n", res);
			free(res);
		}
	} else {
		printf("ERROR: %s\n", json_get_error());
	}

	start = __rdtsc();
	json_free(&node);
	uint64_t free_duration = __rdtsc() - start;

	printf("\njson_parse: %lu\njson_free: %lu\njson_generate: %lu\n",
		parse_duration, free_duration, gen_duration);
}
