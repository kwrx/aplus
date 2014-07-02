//
//  spinlock.c
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

#include <errno.h>
#include <aplus/spinlock.h>

int spinlock_lock(spinlock_t* lock) {
	if(!lock) {
		errno = -EINVAL;
		return 1;
	}
		
	if(*lock) {
		while(*lock)
			task_idle();
		task_wakeup();
	}
			
			
	*lock = 1;
	return 0;
}

int spinlock_unlock(spinlock_t* lock) {
	if(!lock) {
		errno = -EINVAL;
		return 1;
	}
		
	*lock = 0;
	return 0;
}

int spinlock_trylock(spinlock_t* lock) {
	if(!lock) {
		errno = -EINVAL;
		return 1;
	}
	
	if(*lock) {
		errno = -EBUSY;
		return 1;
	}
		
	*lock = 1;
	return 0;
}
