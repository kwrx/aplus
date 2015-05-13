#include <sys/types.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "../coreutils.h"

int do_exec(char** argv) {
	exit(execvp(argv[0], argv));
}


int main(int argc, char** argv) {
	int r;
	if(argc > 1)
		do_sync(do_exec, &argv[1], r);

	struct tms t0;
	times(&t0);

	double real = (double) (t0.tms_utime + t0.tms_stime);
	double user = (double) t0.tms_utime;
	double sys = (double) t0.tms_stime;

	printf("\nreal %dm%2.3fs", (int)(real / CLOCKS_PER_SEC) / 60, (real / CLOCKS_PER_SEC));
	printf("\nuser %dm%2.3fs", (int)(user / CLOCKS_PER_SEC) / 60, (user / CLOCKS_PER_SEC));
	printf("\nsys %dm%2.3fs\n", (int)(sys / CLOCKS_PER_SEC) / 60, (sys / CLOCKS_PER_SEC));

	return 0;
}
