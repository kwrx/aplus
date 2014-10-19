//
//  list.h
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

#ifndef _LIST_H
#define _LIST_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <aplus/spinlock.h>
#include <aplus/mm.h>

typedef uint32_t listval_t;

typedef struct list_body {
	listval_t value;
	
	struct list_body* next;
} list_body_t;

typedef struct list {
	list_body_t* body;
	
	spinlock_t lock;
	size_t size;
} list_t;


static inline int list_empty(list_t* list) {
	if(list)
		return list->size == 0;
	else
		return 1; 	/* empty */
}

static inline int list_add(list_t* list, listval_t v) {
	list_body_t* val = (list_body_t*) kmalloc(sizeof(list_body_t));
	if(!val)
		return -1;
		
	spinlock_lock(&list->lock);
		
	val->value = v;
	val->next = list->body;
	
	list->body = val;
	list->size += 1;
	
	spinlock_unlock(&list->lock);
	return 0;
}

static inline int list_remove(list_t* list, listval_t v) {
	spinlock_lock(&list->lock);
	
	list_body_t* body = list->body;
	list_body_t* prev = 0;
	
	while(body) {
		if(body->value == v) {
			if(prev)
				prev->next = body->next;
			else
				list->body = body->next;
				
			body->value = 0;
			kfree(body);
			break;
		}
		
		prev = body;
		body = body->next;
	}
	
	spinlock_unlock(&list->lock);
	return 0;
}

static inline int list_clear(list_t* list) {
	spinlock_lock(&list->lock);
	
	list_body_t* body = list->body;
	list_body_t* tmp = list->body;
	
	while(body) {
		tmp = body->next;
		kfree(body);
		body = tmp;
	}
	
	list->body = 0;
	list->size = 0;
	
	spinlock_unlock(&list->lock);
	return 0;
}


static inline void list_clone(list_t* dest, list_t* src) {
	spinlock_lock(&src->lock);
	
	for(list_body_t* i = src->body; i; i = i->next) {
		list_add(dest, i->value);
	}
	
	spinlock_unlock(&src->lock);
}

static inline listval_t list_prev(list_t* list, listval_t val) {
	for(list_body_t* i = list->body; i; i = i->next) {
		if(i->value == val)
			if(i->next)
				return i->next->value;
	}
	
	return (listval_t) NULL;
}

static inline listval_t list_next(list_t* list, listval_t val) {
	for(list_body_t* i = list->body; i; i = i->next) {
		if(i->next)
			if(i->next->value == val)
				return i->value;
	}
	
	return (listval_t) NULL;
}

static inline listval_t list_tail(list_t* list) {
	if(list->body)
		return list->body->value;
		
	return (listval_t) NULL;
}

static inline listval_t list_head(list_t* list) {

	if(!list->body)
		return (listval_t) NULL;
	
	list_body_t* tmp = list->body;
	while(tmp->next)
		tmp = tmp->next;
		
	return (listval_t) tmp->value;
}


#define list_safe_begin(list)								\
	spinlock_lock(&list->lock)
	
#define list_safe_end(list)									\
	spinlock_unlock(&list->lock)
	
#define list_foreach(value, list)							\
	for(listval_t value = list_head(list); 					\
		value; 												\
		value = list_next(list, value)						\
		)
		
#define list_foreach_inverse(value, list)					\
	for(listval_t value = list_tail(list); 					\
		value; 												\
		value = list_prev(list, value)						\
		)

#define list_init(list)										\
	list = (list_t*) kmalloc(sizeof(list_t));				\
	list->body = 0;											\
	list->size = 0;											\
	list->lock = 0
	
#define list_destroy(list)									\
	list_clear(list);										\
	kfree(list)


#endif