
//
//  main.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <stdint.h>
#include <stddef.h>

#include <aplus.h>
#include <aplus/task.h>

#include <grub.h>

extern uint32_t end;
uint32_t end_kernel = 0;

static char* argv_init[] = {
	"init",
	0
};

static char* env_init[] = {
	"PATH=/ramdisk;/bin",
	"USER=liveuser",
	"HOME=/",
	"OS=aPlus",
	"SH=/bin/sh",
	0
};


void sysidle() {
	task_setpriority(TASK_PRIORITY_MIN);

	int pid = fork();
	kprintf("fork() return %d from %d\n", pid, getpid());

	for(;;)
		__asm__ __volatile__ ("pause");
}



int symlinks_init() {
	symlink("/dev/tty0", "/dev/stdin");
	symlink("/dev/tty0", "/dev/stdout");
	symlink("/dev/tty0", "/dev/stderr");
}

int mounts_init() {
	mount("", "/proc", "procfs", NULL, NULL);
}

int main() {

	end_kernel = (uint32_t) &end;
	if(mbd->mods_count > 0)
		end_kernel = ((uint32_t*)mbd->mods_addr)[1];


	mm_init();
	video_init();
	desc_init();
	vfs_init();
	task_init();
	rootfs_init();
	tty_init();
	initrd_init();
	symlinks_init();
	mounts_init();


	task_idle();
	task_create(NULL, sysidle);


	execve("/ramdisk/init", argv_init, env_init);
}