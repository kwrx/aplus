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

typedef volatile int spinlock_t;

void spinlock_lock(spinlock_t* spin);
void spinlock_unlock(spinlock_t* spin);
int spinlock_trylock(spinlock_t* spin); 
void __spinlock_waiton();


#define lock()										\
	static spinlock_t __func__##_lock = 0;			\
	spinlock_lock(&__func__##_lock)
	
#define unlock()									\
	spinlock_unlock(&__func__##_lock)
	
	

#define spinlock_waiton(cond)						\
	while(cond)										\
		__spinlock_waiton()


#endif
