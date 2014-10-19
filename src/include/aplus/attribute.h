#ifndef _ATTRIBUTE_H
#define _ATTRIBUTE_H

#include <aplus.h>
#include <aplus/list.h>

#include <stdint.h>
#include <string.h>
#include <sys/types.h>


#define __UNION2(x, y, z)	\
	x ## y ## z

#define __UNION(x, y, z)	\
	__UNION2(x, y, z)

#define ATTRIBUTE(name, value)																		\
	attribute_t __UNION(attribute_, value, __COUNTER__) __attribute__((section(".attribute"))) = {	\
		name, (uint32_t) &value																		\
	}																								\



typedef struct attribute {
	char name[0x80];
	uint32_t value;
} __attribute__((aligned(0x100))) attribute_t;


static inline list_t* attribute(const char* name) {
	extern int attribute_start;
	extern int attribute_end;
	
	uint32_t attr_s = (uint32_t) &attribute_start;
	uint32_t attr_e = (uint32_t) &attribute_end;
	
	list_t* tmp;
	list_init(tmp);
	
	while(attr_s < attr_e) {
		attribute_t* attr = (attribute_t*) attr_s;

		if(strcmp(attr->name, name) == 0)
			list_add(tmp, (listval_t) attr->value);
			
		attr_s += sizeof(attribute_t);
	}
	
	return tmp;
}


#endif
