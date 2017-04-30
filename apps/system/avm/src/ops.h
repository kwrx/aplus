#ifndef _OPS_H
#define _OPS_H

#include <avm.h>

#define R8(x)								\
	{								\
		(x) = *(u1*) buffer;					\
		buffer = (void*) ((long) buffer + 1);			\
	}

#define R16(x)								\
	{								\
		(x) = SWAP(*(u2*) buffer, 16);				\
		buffer = (void*) ((long) buffer + 2);			\
	}

#define R32(x)								\
	{								\
		(x) = SWAP(*(u4*) buffer, 32);				\
		buffer = (void*) ((long) buffer + 4);			\
	}

#define R64(x)								\
	{								\
		(x) = SWAP(*(u8*) buffer, 64);				\
		buffer = (void*) ((long) buffer + 8);			\
	}
	
#define RXX(x, s)							\
	{								\
		if(likely(x)) memcpy(x, buffer, s);			\
		buffer = (void*) ((long) buffer + s);			\
	}





#ifndef NAN
#define NAN				(0.0 / 0.0)
#endif

#ifndef INFINITY
#define INFINITY			(1.0 / 0.0)
#endif

struct avm_ops {
	void* (*calloc) (size_t, size_t);
	void (*free) (void*);
	int (*open) (const char*, int, ...);
	int (*close) (int);
	off_t (*lseek) (int, off_t, int);
	int (*read) (int, void*, size_t);
	int (*yield) (void);
	pid_t (*getpid) (void);
	int (*printf) (const char*, ...);
};


extern struct avm_ops* avm;



int java_class_load(java_assembly_t*, void*, int);
int java_attribute_load(java_assembly_t* assembly, void* buffer, java_attribute_t** attributes, u2 attr_count);

void java_native_init(void);
void java_assembly_init(void);

void java_assembly_flush(void);
void java_library_flush(void);
void java_native_flush(void);
void java_object_flush(void);


const char* strfmt(const char* fmt, ...);


#endif
