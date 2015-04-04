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
#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/list.h>


/**
 *	\brief Acquire a spinlock.
 *	\param spin Spinlock address.
 */
void spinlock_lock(spinlock_t* spin) {
#if HAVE_LOCK
	spinlock_waiton(spin);

	__sync_lock_test_and_set(spin, SPINLOCK_FLAGS_LOCKED);
#endif
}

/**
 *	\brief Acquire a fastlock.
 *	\param spin Spinlock address.
 */
void fastlock_lock(spinlock_t* spin) {
#if HAVE_LOCK
	fastlock_waiton(spin);

	__sync_lock_test_and_set(spin, SPINLOCK_FLAGS_LOCKED);
#endif
}


/**
 *	\brief Unlock a spinlock.
 *	\param spin Spinlock address.
 */
void spinlock_unlock(spinlock_t* spin) {
#if HAVE_LOCK
	__sync_lock_release(spin);
#endif
}

/**
 *	\brief Unlock a fastlock.
 *	\param spin Spinlock address.
 */
void fastlock_unlock(spinlock_t* spin) {
#if HAVE_LOCK
	__sync_lock_release(spin);
#endif
}


/**
 *	\brief Try to acquire a spinlock
 *	\param spin Spinlock address.
 *	\return 0 for success else -1.
 */
int spinlock_trylock(spinlock_t* spin) {
#if HAVE_LOCK
	if(unlikely(!__sync_bool_compare_and_swap(spin, SPINLOCK_FLAGS_UNLOCKED, SPINLOCK_FLAGS_LOCKED)))
		return -1;
#endif

	return 0;
}

/**
 *	\brief Try to acquire a fastlock
 *	\param spin Spinlock address.
 *	\return 0 for success else -1.
 */
int fastlock_trylock(spinlock_t* spin) {
#if HAVE_LOCK
	if(unlikely(!__sync_bool_compare_and_swap(spin, SPINLOCK_FLAGS_UNLOCKED, SPINLOCK_FLAGS_LOCKED)))
		return -1;
#endif

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


EXPORT_SYMBOL(spinlock_lock);
EXPORT_SYMBOL(fastlock_lock);

EXPORT_SYMBOL(spinlock_trylock);
EXPORT_SYMBOL(fastlock_trylock);

EXPORT_SYMBOL(spinlock_unlock);
EXPORT_SYMBOL(fastlock_unlock);
