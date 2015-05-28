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
//  along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <errno.h>



extern inode_t* vfs_root;
extern task_t* current_task;


bootargs_t __mbd;
bootargs_t* mbd = &__mbd;




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
	arch_pre_init();


	vfs_init();
	schedule_init();
	tty_init();

#if HAVE_NETWORK
	//network_init();
#endif

	arch_post_init();
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


	init_modules();


	char* __argv[] = {
		"/dev/ramdisk/bin/init",
		NULL
	};


	char* __envp[] = {
		"PATH=/bin:/usr/bin:/usr/local/bin",
		"SHELL=/usr/bin/sh",
		"USER=warex",
		"OS=aPlus",
		"TMPDIR=/tmp",
		"HOME=/usr",
		"ROOTDEV=/dev/cd0",
		"ROOTFS=iso9660",
		"SDL_VIDEODRIVER=dummy",
		NULL
	};


	//if(sys_fork() == 0)
		sys_execve(__argv[0], __argv, __envp);

	
	sysidle();
}


EXPORT_SYMBOL_OBJ(mbd);

