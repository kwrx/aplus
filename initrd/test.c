#include <stdio.h>


int main(int argc, char** argv) {
	printf("Hello World\n");

	printf("argc: %d\n", argc);

	int i;
	for(i = 0; i < 1; i++)
		printf("{%d} = \"%s\";\n", i, argv[i]);

	return 0;
}
