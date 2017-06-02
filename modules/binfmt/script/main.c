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
    for(char* s = strtok((char*) image, " "); s; s = strtok(NULL, " "))
        argv[i++] = s;
	argv[i++] = NULL;


	int fd = sys_open(tmpnam(NULL), O_CREAT | O_RDWR, S_IFREG | 0666);
	KASSERT(fd >= 0);

	fd = sys_fcntl(fd, F_DUPFD, STDIN_FILENO);
	KASSERT(fd == 0);

	fd = sys_write(STDIN_FILENO, p, strlen(p));
	KASSERT(fd == strlen(p));

	fd = sys_lseek(STDIN_FILENO, 0, SEEK_SET);
	KASSERT(fd == 0);

	sys_execve(argv[0], argv, current_task->environ);
	return E_ERR;
}

static void* script_load(void* image, void** address, symbol_t** symtab, size_t* size) {
	(void) image;
	(void) address;
	(void) symtab;
	(void) size;

	return fake_start;
}



int init(void) {
	binfmt_register("SCRIPT", script_check, script_load);

	return E_OK;
}



int dnit(void) {
	return E_OK;
}
