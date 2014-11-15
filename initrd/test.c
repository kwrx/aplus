#include <stdio.h>


int main(int argc, char** argv) {
	printf("Hello World\n");

	int i;
	for(i = 0; i < argc; i++)
		printf("{%d} = \"%s\";\n", i, argv[i]);

	for(;;);
	return 0;
}
