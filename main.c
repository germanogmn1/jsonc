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

	json_node node;
	uint64_t start = __rdtsc();
	bool ok = json_parse(buffer, &node);
	uint64_t parse_duration = __rdtsc() - start;

	if (ok) {
		#if 1
		json_print(node);
		#else
		assert(node.type == JSON_ARRAY);
		for (uint32_t i = 0; i < node.array.count; i++) {
			json_node element = node.array.elements[i];
			assert(element.type == JSON_OBJECT);
			json_object obj = element.object;
			json_node* friends = json_get(&obj, "friends");
			assert(friends);
			assert(friends->type == JSON_ARRAY);
			for (uint32_t j = 0; j < friends->array.count; j++) {
				json_node e = friends->array.elements[j];
				assert(e.type == JSON_OBJECT);
				json_node* name = json_get(&e.object, "name");
				assert(name);
				assert(name->type == JSON_STRING);
				printf("* %s\n", name->string);
			}
		}
		#endif
	} else {
		printf("ERROR: %s\n", json_get_error());
	}

	start = __rdtsc();
	json_free(&node);
	uint64_t free_duration = __rdtsc() - start;

	printf("\njson_parse: %lu\njson_free: %lu\n",
		parse_duration, free_duration);
}
