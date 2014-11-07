#include <stdio.h>
#include <signal.h>
#include <assert.h>


static int __test_signals__(int sig) {
	printf("received signal %d\n", sig);

	return 0;
}

static int test_stdio() {
	return printf("Bla bla bla\n");
}

static int test_args(int argc, char** argv) {
	if(argc == 0)
		return 0;

	int i;
	for(i = 0; i < argc; i++)
		printf("argv[%d] => \"%s\"\n", i, argv[i]);

	return 1;
}


static int test_signals() {
	assert(signal(SIGUSR1, __test_signals__) != SIG_ERR);
	return raise(SIGUSR1);
}


int main(int argc, char** argv) {
	
	assert(test_stdio() && "Test stdio failed");
	assert(test_args(argc, argv) && "Test arguments failed");
	assert(test_signals() >= 0 && "Test signals failed");
	
	printf("Test OK\n");
	return 0;
}
