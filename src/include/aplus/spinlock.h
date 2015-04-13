//
//  spinlock.h
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


#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#define SPINLOCK_FLAGS_UNLOCKED						0
#define SPINLOCK_FLAGS_LOCKED						1

typedef volatile int spinlock_t;
#define fastlock_t spinlock_t

void spinlock_lock(spinlock_t* spin);
void spinlock_unlock(spinlock_t* spin);
int spinlock_trylock(spinlock_t* spin); 

void fastlock_lock(spinlock_t* spin);
void fastlock_unlock(spinlock_t* spin);
int fastlock_trylock(spinlock_t* spin); 

void __spinlock_waiton();
void __fastlock_waiton();


#define spinlock_waiton(cond)						\
	while(											\
			!__sync_bool_compare_and_swap(			\
				cond,								\
				SPINLOCK_FLAGS_UNLOCKED,			\
				SPINLOCK_FLAGS_LOCKED				\
			)										\
	)												\
		__spinlock_waiton()

#define fastlock_waiton(cond)						\
	while(											\
			!__sync_bool_compare_and_swap(			\
				cond,								\
				SPINLOCK_FLAGS_UNLOCKED,			\
				SPINLOCK_FLAGS_LOCKED				\
			)										\
	)												\
		__fastlock_waiton()


#define spinlock_init(spin, flags)					\
	*(spin) = flags
#define fastlock_init(spin, flags)					\
	spinlock_init(spin, flags)


#endif
