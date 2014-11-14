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

#ifndef _APLUS_LIST_H
#define _APLUS_LIST_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>


#define __list_malloc(x)		malloc(x)
#define __list_free(x)			free(x)
#define __list_lock(x)			(void) x
#define __list_unlock(x)		(void) x
#define __list_lock_t			int


typedef uint32_t listval_t;

typedef struct list_body {
	listval_t value;
	
	struct list_body* next;
} list_body_t;

typedef struct list {
	list_body_t* body;
	
	__list_lock_t lock;
	size_t size;
} list_t;



/**
 * \brief Check if list is empty or unitialized.
 * \param list Pointer to list.
 * \return true or false.
 */
static inline int list_empty(list_t* list) {
	if(list)
		return list->size == 0;
	else
		return 1; 	/* empty */
}


/**
 * \brief Add value to the list.
 * \param list Pointer to list.
 * \param v Value to add.
 * \return 0 if success, otherwise -1.
 */
static inline int list_add(list_t* list, listval_t v) {
	list_body_t* val = (list_body_t*) __list_malloc(sizeof(list_body_t));
	if(!val)
		return -1;
		
	__list_lock(&list->lock);
		
	val->value = v;
	val->next = list->body;
	
	list->body = val;
	list->size += 1;
	
	__list_unlock(&list->lock);
	return 0;
}


/**
 * \brief Remove value to the list.
 * \param list Pointer to list.
 * \param v Value to remove.
 * \return 0 if success, otherwise -1.
 */
static inline int list_remove(list_t* list, listval_t v) {
	__list_lock(&list->lock);
	
	list_body_t* body = list->body;
	list_body_t* prev = 0;
	
	while(body) {
		if(body->value == v) {
			if(prev)
				prev->next = body->next;
			else
				list->body = body->next;
				
			body->value = 0;
			__list_free(body);
			break;
		}
		
		prev = body;
		body = body->next;
	}
	
	__list_unlock(&list->lock);
	return 0;
}


/**
 * \brief Erase all values in the list.
 * \param list Pointer to list.
 * \return 0 if success, otherwise -1.
 */
static inline int list_clear(list_t* list) {
	__list_lock(&list->lock);
	
	list_body_t* body = list->body;
	list_body_t* tmp = list->body;
	
	while(body) {
		tmp = body->next;
		__list_free(body);
		body = tmp;
	}
	
	list->body = 0;
	list->size = 0;
	
	__list_unlock(&list->lock);
	return 0;
}


/**
 * \brief Copy all values in the src's list to dest's list.
 * \param dest Pointer to list.
 * \param src Pointer to list.
 */
static inline void list_clone(list_t* dest, list_t* src) {
	__list_lock(&src->lock);
	
	for(list_body_t* i = src->body; i; i = i->next) {
		list_add(dest, i->value);
	}
	
	__list_unlock(&src->lock);
}


/**
 * \brief Get previous value in the list.
 * \param list Pointer to list.
 * \param val A Value in the list.
 * \return Previous value if success, otherwise NULL.
 */
static inline listval_t list_prev(list_t* list, listval_t val) {
	for(list_body_t* i = list->body; i; i = i->next) {
		if(i->value == val)
			if(i->next)
				return i->next->value;
	}
	
	return (listval_t) NULL;
}


/**
 * \brief Get next value in the list.
 * \param list Pointer to list.
 * \param val A Value in the list.
 * \return Next value if success, otherwise NULL.
 */
static inline listval_t list_next(list_t* list, listval_t val) {
	for(list_body_t* i = list->body; i; i = i->next) {
		if(i->next)
			if(i->next->value == val)
				return i->value;
	}
	
	return (listval_t) NULL;
}

/**
 * \brief Get last value in the list.
 * \param list Pointer to list.
 * \return Last value if success, otherwise NULL.
 */
static inline listval_t list_tail(list_t* list) {
	if(list->body)
		return list->body->value;
		
	return (listval_t) NULL;
}

/**
 * \brief Get first value in the list.
 * \param list Pointer to list.
 * \return First value if success, otherwise NULL.
 */
static inline listval_t list_head(list_t* list) {

	if(!list->body)
		return (listval_t) NULL;
	
	list_body_t* tmp = list->body;
	while(tmp->next)
		tmp = tmp->next;
		
	return (listval_t) tmp->value;
}


/**
 * \brief Lock a list for safe operations on it.
 * \param list Pointer to list.
 */
#define list_safe_begin(list)								\
	__list_lock(&list->lock)
	
/**
 * \brief Unlock a list.
 * \param list Pointer to list.
 */
#define list_safe_end(list)									\
	__list_unlock(&list->lock)
	

/**
 * \brief Iterating a list and save each value in "value".
 * \param value Value where save each iteration.
 * \param list Pointer to list.
 */
#define list_foreach(value, list)							\
	for(listval_t value = list_head(list); 					\
		value; 												\
		value = list_next(list, value)						\
		)
	
/**
 * \brief Iterating a list and save each value in "value" conversely.
 * \param value Value where save each iteration.
 * \param list Pointer to list.
 */	
#define list_foreach_reverse(value, list)					\
	for(listval_t value = list_tail(list); 					\
		value; 												\
		value = list_prev(list, value)						\
		)


/**
 * \brief Get size of list.
 * \param list Pointer to list.
 */
#define list_size(list)										\
	list->size

/**
 * \brief Initialize a list.
 * \param list Pointer to list.
 */	
#define list_init(list)										\
	list = (list_t*) __list_malloc(sizeof(list_t));			\
	list->body = 0;											\
	list->size = 0;											\
	list->lock = 0

/**
 * \brief Destroy a list.
 * \param list Pointer to list.
 */		
#define list_destroy(list)									\
	list_clear(list);										\
	__list_free(list)



/**
 * \example list.h

	// Initialization

	list_t test = NULL;
	list_init(test);

	for(int i = 0; i < 10; i++)
		list_add(test, (listval_t) i);


	// Iterate list.
	// Output: 0, 1, 2, 3, ... 9

	list_foreach(v, test)
		printf("%d\n", v);

	

	// Iterate list conversly.
	// Output: 9, 8, 7, 6, ... 0

	list_foreach_reverse(v, test)
		printf("%d\n", v);



	// Remove a value

	list_remove(test, (listval_t) 2);

	
	// Destroy

	list_destroy(test);
 */

#endif
