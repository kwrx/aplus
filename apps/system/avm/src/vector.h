#ifndef _VECTOR_H
#define _VECTOR_H

#include "ops.h"


typedef struct vector {
	void* value;
	struct vector* next;
} vector_t;

static inline void vector_add(vector_t** head, void* value) {
	vector_t* v = (vector_t*) avm->calloc(1, sizeof(vector_t));
	ASSERT(v);

	v->value = value;
	v->next = *head;
	*head = v;
}


static inline int vector_size(vector_t** head) {
	int i = 0;
	vector_t* v;
	for(v = *head; v; v = v->next)
		i++;

	return i;
}

static inline void* vector_to_array(vector_t** vec, int length, int membsize) {
	long a = (long) avm->calloc(length, membsize);
	ASSERT(a);

	int idx = length - 1;
	
	vector_t* v = *vec;
	while(v) {
		memcpy((void*) (a + (idx * membsize)), v->value, membsize);
		
		idx--;
		
		vector_t* p = v;
		v = v->next;
		avm->free(p);
	}
	
	*vec = NULL;
	return (void*) a;
}

static inline int vector_destroy(vector_t** vec) {
	vector_t* v = *vec;
	while(v) {
		vector_t* p = v;
		v = v->next;
		avm->free(p);
	}

	*vec = NULL;
	return 0;
}

#endif
