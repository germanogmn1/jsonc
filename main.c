#include <stdio.h>
#include <x86intrin.h>
#include "jsonc.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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
		// json_print(node);
	} else {
		printf("ERROR: %s\n", json_get_error());
	}

	start = __rdtsc();
	json_free(&node);
	uint64_t free_duration = __rdtsc() - start;

	printf("\njson_parse: %lu\njson_free: %lu\n",
		parse_duration, free_duration);
}
