#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <dirent.h>

#include <time.h>
#include <sys/times.h>


typedef struct task {
	int pid;
	int uptime;

	struct task* next;
} task_t;
task_t* queue = 0;



void error(char* msg) {
	printf("%s\n", msg);
	exit(-1);
}

int read_uptime(int pid) {
	char path[64];
	memset(path, 0, 64);	
	sprintf(path, "/proc/%d/uptime", pid);

	FILE* fp = fopen(path, "rb");
	if(!fp)
		return 0;


	int uptime = 0;
	fscanf(fp, "%d", &uptime);
	fclose(fp);


	return uptime;
}


void dump_proc(int pid) {
	task_t* tmp = queue;	
	
	while(tmp) {
		if(tmp->pid == pid)
			goto proc_found;

		tmp = tmp->next;
	}

	tmp = malloc(sizeof(task_t));
	tmp->pid = pid;
	tmp->uptime = read_uptime(pid);
	tmp->next = queue;
	queue = tmp;

proc_found:		
	
	if(tmp->uptime != 0)
		printf("%d:\t%gms\t%g%%\n", tmp->pid, (float)tmp->uptime, (float)(read_uptime(pid) - tmp->uptime) / (float)CLOCKS_PER_SEC * 100.0f);
	else
		printf("%d:\tN/A\tN/A\n", tmp->pid);
}


int main(int argc, char** argv) {
	struct dirent* ent;
	DIR* fd = opendir("/proc");
	if(!fd)
		error("Could not open proc directory");


	printf("PID:\tUPTIME\tUSAGE\n");

	for(;;) {
		while((ent = readdir(fd))) {
			if(ent->d_name[0] >= '0' && ent->d_name[0] <= '9') {
				dump_proc(atoi(ent->d_name));
			}
		}

		int t0 = time(NULL);
		while(t0 == time(NULL))
			sched_yield();
	}

	closedir(fd);
	return 0;
}