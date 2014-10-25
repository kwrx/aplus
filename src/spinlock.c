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

#include <stdint.h>
#include <aplus/spinlock.h>
#include <aplus/list.h>


void spinlock_lock(spinlock_t* spin) {
	if((*spin & SPINLOCK_FLAGS_FASTLOCK) == 0)
		spinlock_waiton(*spin & SPINLOCK_FLAGS_LOCKED);
	else
		fastlock_waiton(*spin & SPINLOCK_FLAGS_LOCKED);

	*spin |= SPINLOCK_FLAGS_LOCKED;
}


void spinlock_unlock(spinlock_t* spin) {
	*spin &= ~SPINLOCK_FLAGS_LOCKED;
}

int spinlock_trylock(spinlock_t* spin) {
	if(*spin & SPINLOCK_FLAGS_LOCKED)
		return -1;
		
	*spin |= SPINLOCK_FLAGS_LOCKED;
	return 0;
}

void __spinlock_waiton() {
	schedule_yield();
}

void __fastlock_waiton() {
	__asm__ __volatile__("pause");
}
