#include <stdio.h>
#include <pthread.h>

int main(int argc, char** argv) {
	printf("Hello World\n");

	printf("argc: %d\n", argc);

	int i;
	for(i = 0; i < 1; i++)
		printf("{%d} = \"%s\";\n", i, argv[i]);



	pthread_barrier_t b;
	pthread_barrier_init(&b, NULL, 1);

	return 0;
}
