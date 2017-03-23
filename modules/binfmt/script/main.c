#include <aplus.h>
#include <aplus/binfmt.h>
#include <aplus/debug.h>
#include <aplus/mm.h>
#include <aplus/module.h>
#include <aplus/task.h>
#include <libc.h>


MODULE_NAME("binfmt/script");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


static void fake_start(char** argv, char** env) {
	sys_exit(0);
}

static int script_check(void* image) {
	if(strncmp((const char*) image, "#!", 2) != 0)
		return E_ERR;


	image = (void*) ((uintptr_t) image + 2);

	char* p = (char*) image;
	if((p = strchr(p, '\n')))
		*p++ = 0;

	char* argv[32];
	memset(argv, 0, sizeof(argv));


	int i = 0;
    for(char* p = strtok((char*) image, " "); p; p = strtok(NULL, " "))
        argv[i++] = p;

	argv[i++] = p;
	argv[i++] = NULL;

	sys_execve(argv[0], argv, current_task->environ);
	return E_OK;
}

static void* script_load(void* image, void** address, size_t* size) {
	return fake_start;
}



int init(void) {
	binfmt_register("SCRIPT", script_check, script_load);

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
