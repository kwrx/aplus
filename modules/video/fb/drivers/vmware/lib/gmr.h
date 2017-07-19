/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

/*
 * gmr.h --
 *
 *      Utilities for creating and Guest Memory Regions (GMRs)
 */

#ifndef __GMR_H__
#define __GMR_H__

#include "svga_reg.h"

/*
 * Macros for physical memory pages, in our flat memory model.
 * Physical Page Numbers (PPNs) are identity-mapped to virtual
 * addresses in Metalkit.
 */


#define PAGE_SHIFT         12
#define PAGE_MASK          (PAGE_SIZE - 1)
#define PPN_POINTER(ppn)   ((void*)((ppn)*PAGE_SIZE))
typedef uint32_t PPN;


/*
 * Global GMR state, read in by GMR_Init.
 */

typedef struct GMRState {
   uint32_t maxIds;
   uint32_t maxDescriptorLen;
   uint32_t maxPages;
} GMRState;

extern GMRState gGMR;


/*
 * Trivial memory heap, used as system memory backings for GMRs. We
 * never actually free any memory, we just allocate starting from the
 * top of the binary image and we grow upward until we hit physical
 * memory which isn't present.
 */

void Heap_Reset(void);
void *Heap_Alloc(uint32_t bytes);
PPN Heap_AllocPages(uint32_t numPages);
void Heap_Discard(void *data, uint32_t bytes);
void Heap_DiscardPages(PPN firstPage, uint32_t numPages);


/*
 * Read GMR capabilities into gGMR, and Panic if GMRs are not
 * supported.
 */

void GMR_Init(void);
void GMR2_Init(void);


/*
 * Creating GMR descriptors.
 */

PPN GMR_AllocDescriptor(SVGAGuestMemDescriptor *descArray,
                        uint32_t numDescriptors);


/*
 * Creating/destroying GMRs
 */

void GMR_Define(uint32_t gmrId,
                SVGAGuestMemDescriptor *descArray,
                uint32_t numDescriptors);
PPN GMR_DefineContiguous(uint32_t gmrId, uint32_t numPages);
PPN GMR_DefineEvenPages(uint32_t gmrId, uint32_t numPages);
void GMR_FreeAll(void);


#endif /* __GMR_H__ */
