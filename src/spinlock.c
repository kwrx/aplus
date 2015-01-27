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


/**
 *	\brief Acquire a spinlock.
 *	\param spin Spinlock address.
 */
void spinlock_lock(spinlock_t* spin) {
	if(unlikely((*spin & SPINLOCK_FLAGS_FASTLOCK) == 0))
		spinlock_waiton(*spin & SPINLOCK_FLAGS_LOCKED);
	else
		fastlock_waiton(*spin & SPINLOCK_FLAGS_LOCKED);

	*spin |= SPINLOCK_FLAGS_LOCKED;
}


/**
 *	\brief Unlock a spinlock.
 *	\param spin Spinlock address.
 */
void spinlock_unlock(spinlock_t* spin) {
	*spin &= ~SPINLOCK_FLAGS_LOCKED;
}


/**
 *	\brief Try to acquire a spinlock
 *	\param spin Spinlock address.
 *	\return 0 for success else -1.
 */
int spinlock_trylock(spinlock_t* spin) {
	if(unlikely(*spin & SPINLOCK_FLAGS_LOCKED))
		return -1;
		
	*spin |= SPINLOCK_FLAGS_LOCKED;
	return 0;
}


/**
 *	\brief Yield current task if a false condition was given by spinlock_waiton().
 *	\see spinlock_waiton
 */
void __spinlock_waiton() {
	schedule_yield();
}

/**
 *	\brief Put CPU in pause for a while if a false condition was given by fastlock_waiton().
 *	\see fastlock_waiton
 */
void __fastlock_waiton() {
	cpu_wait();
}
