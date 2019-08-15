/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _X86_INTR_H
#define _X86_INTR_H

#include <stdint.h>



#define X86_GDT_MAX             (6)
#define X86_IDT_MAX             (256)


#define SEG_DESCTYPE(x)         ((x) << 0x04)
#define SEG_PRES(x)             ((x) << 0x07)
#define SEG_SAVL(x)             ((x) << 0x0C)
#define SEG_LONG(x)             ((x) << 0x0D)
#define SEG_SIZE(x)             ((x) << 0x0E)
#define SEG_GRAN(x)             ((x) << 0x0F)
#define SEG_PRIV(x)             (((x) & 0x03) << 0x05)
 
#define SEG_DATA_RDWR           0x02
#define SEG_CODE_EXRD           0x0A


#define IDT_ISR_TRAP            0x8F
#define IDT_ISR_INTERRUPT       0x8E
#define IDT_ISR_DPL(x)          ((x) << 5)


typedef struct {

    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;

} __attribute__((packed)) tss32_t;

typedef struct {
    uint32_t null;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved_0;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved_1;
    uint32_t iopb;
} __attribute__((packed)) tss64_t;


#if defined(__i386__)
typedef tss32_t tss_t;
#elif defined(__x86_64__)
typedef tss64_t tss_t;
#endif


extern void x86_lgdt(void);
extern void x86_ltr(void);
extern void x86_lidt(void);

extern void x86_gdt_init_percpu(uint16_t);
extern void x86_idt_init_percpu(uint16_t);

void intr_init();

#endif