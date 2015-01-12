#include <aplus.h>
#include <aplus/syscall.h>

#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>


time_t sys_time(time_t* ptr) {
	
	struct timeval tv;
	struct timezone tz;

	sys_gettimeofday(&tv, &tz);


	if(likely(ptr))
		*ptr = tv.tv_sec;

	return tv.tv_sec;
}



/* SYSCALL(sys_time, -1); */
