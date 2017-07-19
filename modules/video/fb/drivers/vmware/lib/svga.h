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
 * svga.h --
 *
 *      This is a simple example driver for the VMware SVGA device.
 *      It handles initialization, register accesses, low-level
 *      command FIFO writes, and host/guest synchronization.
 */

#ifndef __SVGA_H__
#define __SVGA_H__

#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/intr.h>
#include <aplus/mm.h>
#include <libc.h>

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#elif defined(__x86_64__)
#include <arch/x86_64/x86_64.h>
#endif

// XXX: Shouldn't have to do this here.
#define INLINE inline

#include "svga_reg.h"
#include "svga_escape.h"
#include "svga_overlay.h"
#include "svga3d_reg.h"

typedef struct SVGADevice {
   uint32_t pciAddr;
   uint32_t     ioBase;
   uint32_t    *fifoMem;
   uint8_t     *fbMem;
   uint32_t     fifoSize;
   uint32_t     fbSize;
   uint32_t     vramSize;

   uint32_t     deviceVersionId;
   uint32_t     capabilities;

   uint32_t     width;
   uint32_t     height;
   uint32_t     bpp;
   uint32_t     pitch;

   struct {
      uint32_t  reservedSize;
      int  usingBounceBuffer;
      uint8_t   bounceBuffer[1024 * 1024];
      uint32_t  nextFence;
   } fifo;

   volatile struct {
      uint32_t        pending;
      uint32_t        switchContext;
      void*         oldContext;
      void*          newContext;
      uint32_t        count;
   } irq;

} SVGADevice;

extern SVGADevice gSVGA;

int SVGA_Init(void);
void SVGA_Enable(void);
void SVGA_SetMode(uint32_t width, uint32_t height, uint32_t bpp);
void SVGA_Disable(void);
void SVGA_Panic(const char *err);
void SVGA_DefaultFaultHandler(int vector);

uint32_t SVGA_ReadReg(uint32_t index);
void SVGA_WriteReg(uint32_t index, uint32_t value);
uint32_t SVGA_ClearIRQ(void);
uint32_t SVGA_WaitForIRQ();

int SVGA_IsFIFORegValid(int reg);
int SVGA_HasFIFOCap(int cap);

void *SVGA_FIFOReserve(uint32_t bytes);
void *SVGA_FIFOReserveCmd(uint32_t type, uint32_t bytes);
void *SVGA_FIFOReserveEscape(uint32_t nsid, uint32_t bytes);
void SVGA_FIFOCommit(uint32_t bytes);
void SVGA_FIFOCommitAll(void);

uint32_t SVGA_InsertFence(void);
void SVGA_SyncToFence(uint32_t fence);
int SVGA_HasFencePassed(uint32_t fence);
void SVGA_RingDoorbell(void);

void * SVGA_AllocGMR(uint32_t size, SVGAGuestPtr *ptr);

/* 2D commands */

void SVGA_Update(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void SVGA_BeginDefineCursor(const SVGAFifoCmdDefineCursor *cursorInfo,
                            void **andMask, void **xorMask);
void SVGA_BeginDefineAlphaCursor(const SVGAFifoCmdDefineAlphaCursor *cursorInfo,
                                 void **data);
void SVGA_MoveCursor(uint32_t visible, uint32_t x, uint32_t y, uint32_t screenId);

void SVGA_BeginVideoSetRegs(uint32_t streamId, uint32_t numItems,
                            SVGAEscapeVideoSetRegs **setRegs);
void SVGA_VideoSetAllRegs(uint32_t streamId, SVGAOverlayUnit *regs, uint32_t maxReg);
void SVGA_VideoSetReg(uint32_t streamId, uint32_t registerId, uint32_t value);
void SVGA_VideoFlush(uint32_t streamId);

#endif /* __SVGA_H__ */
