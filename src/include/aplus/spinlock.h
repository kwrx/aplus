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

typedef volatile unsigned int spinlock_t;

int spinlock_lock(spinlock_t* lock);
int spinlock_unlock(spinlock_t* lock);
int spinlock_trylock(spinlock_t* lock);


#define __lock							\
	static spinlock_t lock_##__func__; 	\
	spinlock_lock(&lock_##__func__);
	
#define __unlock						\
	spinlock_unlock(&lock_##__func__);

#endif