#include <stdio.h>
#include <x86intrin.h>
#include "jsonc.h"
#include <stdint.h>

char * raw_json = "{\n"
	"	\"key\\t\\t1\": \"some text he\\u04bce\",\n"
	"	\"key2\": null,\n"
	"	\"key3\\uf123\": true,\n"
	"	\"key4\": [\n"
	"		false,\n"
	"		35.827,\n"
	"		\"foo\",\n"
	"		[1e3, 1E+6, 456.789E-3, 0.123e3]\n"
	"	]\n"
	"}\n";

int main() {
	printf("---\n%s---\n", raw_json);

	uint64_t start = __rdtsc();
	json_node node = json_parse(raw_json);
	uint64_t duration = __rdtsc() - start;
	printf("\n{{{ %lu }}}\n", duration);

	json_print(node);
}
