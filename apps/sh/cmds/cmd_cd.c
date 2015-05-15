#include "../sh.h"
#include <unistd.h>


int cmd_cd(char** argv) {
	if(argv[1])
		return chdir(argv[1]);

	return 0;
}
