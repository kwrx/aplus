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
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>


extern inode_t* vfs_root;
extern task_t* current_task;

bootargs_t __mbd;
bootargs_t* mbd = &__mbd;



int vmm_alloc() {}
int task_fork() {}
int task_init() {}
int task_switch_ack() {}
int task_switch() {}
int vmm_free() {}
int task_clone() {}
int vmm_init() {}

/**
 *	\brief Put Kernel Task in optimized loop for powersave.
 */
static void sysidle() {
	schedule_setpriority(TASK_PRIORITY_MIN);


	for(;;)
		cpu_idle();
}


/**
 *	\brief Entry point for kernel.
 */
int main() {
	arch_init();

	//vfs_init();
	//schedule_init();
	//tty_init();

#if HAVE_NETWORK
	//netif_init();
#endif

	go_usermode();




	struct utsname u;
	sys_uname(&u);

	kprintf("%s %s %s %s %s\n", u.sysname, u.nodename, u.release, u.version, u.machine);

	if(unlikely(mbd->ramdisk.ptr == 0))
		panic("no initrd module found");
	

	uint32_t addr = (uint32_t) mm_vaddr((void*) mbd->ramdisk.ptr);
	uint32_t endp = (uint32_t) mm_vaddr((void*) (mbd->ramdisk.ptr + mbd->ramdisk.size));

	kprintf("initrd: module found at addess: 0x%x (%d KB)\n", addr, (endp - addr) / 1024);

	
	
	if(unlikely(!mkramdev("/dev/ram0", addr, endp - addr)))
		panic("initrd: cannot create /dev/ram0");

	if(unlikely(sys_mount("/dev/ram0", "/dev/ramdisk", "iso9660", 0, 0) != 0))
		panic("initrd: cannot mount ramdisk");


	if(unlikely(sys_mount(NULL, "/dev/proc", "procfs", 0, 0) != 0))
		panic("procfs: cannot mount /dev/proc");

	if(unlikely(sys_mount(NULL, "/dev/tmp", "tmpfs", 0, 0) != 0))
		panic("tmpfs: cannot mount /dev/tmp");

	if(unlikely(sys_symlink("/dev/proc", "/proc") != 0))
		panic("procfs: cannot create link for /proc");

	if(unlikely(sys_symlink("/dev/tmp", "/tmp") != 0))
		panic("tmpfs: cannot link for /tmp");


	sys_clone(sysidle, NULL, NULL, CLONE_VM | CLONE_FILES | CLONE_FS | CLONE_SIGHAND);
	
	char* __argv[] = {
		"/dev/ramdisk/bin/init",
		NULL
	};


	char* __envp[] = {
		"PATH=/bin:/usr/bin:/usr/local/bin:/dev/ramdisk/bin",
		"SHELL=/bin/dash",
		"USER=liveuser",
		"TMPDIR=/tmp",
		"SDL_VIDEODRIVER=dummy",
		"SCREEN_WIDTH=800",
		"SCREEN_HEIGHT=600",
		"SCREEN_BPP=32",
		NULL
	};



	//if(sys_fork() == 0)
		//sys_execve(__argv[0], __argv, __envp);
	
	for(;;);
}


