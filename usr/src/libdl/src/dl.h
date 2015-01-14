#ifndef _DL_H
#define _DL_H

typedef struct rtsym {
	char name[255];
	void* addr;
	int flags;

	struct rtsym* next;
} rtsym_t;


typedef struct {
	char name[255];
	int flags;
	void* image;
	int imagesz;
	int memsz;

	rtsym_t* symbols;
} dll_t;

#endif
