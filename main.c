#include <stdio.h>
#include "jsonc.h"

char *raw_json = "{\n"
	"    \"key\\t\\t1\": \"some text he\\u04bce\",\n"
	"    \"key2\": null,\n"
	"    \"key3\\uf123\": true,\n"
	"    \"key4\": [false, 35, \"foo\"]\n"
	"}\n";

int main() {
	printf("---\n%s---\n", raw_json);
	json_node node = json_parse(raw_json);
	json_print(node);
}
