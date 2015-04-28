//
//  desc.c
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

#ifdef __i386__

#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>


#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/times.h>

#include <arch/i386/i386.h>


#define FAULT_MASK		0x3FFFFFE3
#define PIT_FREQ		(CLOCKS_PER_SEC)
#define PIT_INC			1


typedef struct gdt_entry {
	uint16_t limit_l, base_l;
	uint8_t base_m, access, granularity, base_h;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct idt_entry {
	uint16_t base_l, sel;
	uint8_t null, flags;
	uint16_t base_h;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed)) idt_ptr_t;

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
} __attribute__((packed)) tss_entry_t;



static char *exception_messages[] = {
	"Division By Zero",
	"Debug",
	"Non Maskable Interrupt",
	"Breakpoint",
	"Into Detected Overflow",
	"Out of Bounds",
	"Invalid Opcode",
	"No Coprocessor",

	"Double Fault",
	"Coprocessor Segment Overrun",
	"Bad TSS",
	"Segment Not Present",
	"Stack Fault",
	"General Protection Fault",
	"Page Fault",
	"Unknown Interrupt",

	"Coprocessor Fault",
	"Alignment Check",
	"Machine Check",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",

	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};

static void* irqs[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static gdt_entry_t gdt_e[6];
static gdt_ptr_t gdt_p;

static idt_entry_t idt_e[256];
static idt_ptr_t idt_p;

static tss_entry_t tss_entry;

static uint32_t pit_ticks;
static uint32_t pit_seconds;
static uint32_t pit_days;

uint8_t keyboard_ready = 0;


extern void schedule();
extern void syscall_handler(regs_t*);

extern int kernel_stack;
extern int kernel_stack_end;


__asm__ (
	".section .text		\n\
	.global gdt_load	\n\
	gdt_load:			\n\
	lgdt [gdt_p]		\n\
						\n\
	mov ax, 0x10		\n\
	mov ds, ax			\n\
	mov es, ax			\n\
	mov fs, ax			\n\
	mov gs, ax			\n\
	mov ss, ax			\n\
	jmp 0x08:.done		\n\
						\n\
	.done:				\n\
	mov ax, 0x2B		\n\
	ltr ax				\n\
	ret					\n\
						\n\
						\n\
						\n\
						\n\
	.global idt_load	\n\
	idt_load:			\n\
	lidt [idt_p]		\n\
	ret					\n\
						\n\
						\n"
);


void isr_handler(regs_t* r) {
	arch_panic(exception_messages[r->int_no], r);
}

void irq_handler(regs_t* r) {

	if(irqs[r->int_no - 32])
		((void (*)(regs_t*)) irqs[r->int_no - 32]) (r);

	if(r->int_no >= 40)
		outb(0xA0, 0x20);
		
	outb(0x20, 0x20);
}

void pit_handler(regs_t* r) {
	pit_ticks += PIT_INC;
	
	if(unlikely(pit_ticks >= PIT_FREQ)) {
		pit_ticks = 0;
		pit_seconds += 1;
	}
	
	if(unlikely(pit_seconds >= 86400)) {
		pit_seconds = 0;
		pit_days += 1;
	}
	
	schedule();
}


void pagefault_handler(regs_t* r) {
	uint32_t faultaddr;
	__asm__ __volatile__("mov eax, cr2" : "=a"(faultaddr));

	kprintf("mmu: Page fault at 0x%x (%x, %s, %s, %s, %s, %s)\n",
		faultaddr,
		r->err_code,
		r->err_code & 0x01 ? "P" : "N/A",
		r->err_code & 0x02 ? "W" : "R",
		r->err_code & 0x04 ? "U" : "S",
		r->err_code & 0x08 ? "RSVD" : "\b\b",
		r->err_code & 0x10 ? "I/D" : "\b\b" 
	);

	arch_panic("Page fault", r);
}





int desc_init() {

	__asm__ ("cli");
	

	#define gdt_set(n, b, l, a, g) 					\
		gdt_e[n].base_l = (b & 0xFFFF);				\
		gdt_e[n].base_m = (b >> 16) & 0xFF;			\
		gdt_e[n].base_h = (b >> 24) & 0xFF;			\
		gdt_e[n].limit_l = (l & 0xFFFF);			\
		gdt_e[n].granularity = (l >> 16) & 0x0F;	\
		gdt_e[n].granularity |= g & 0xF0;			\
		gdt_e[n].access = a
				
	
	memset(&gdt_e, 0, sizeof(gdt_entry_t) * 6);
		
	gdt_p.limit = sizeof(gdt_entry_t) * 6 - 1;
	gdt_p.base = (uint32_t) gdt_e;
		
	gdt_set(0, 0, 0, 0, 0);
	gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
	gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
	gdt_set(4, 0, 0xFFFFFFFF, 0XF2, 0xCF);
	gdt_set(5, ((int) &tss_entry), ((int) &tss_entry + sizeof(tss_entry_t)), 0xE9, 0x00);


	memset(&tss_entry, 0, sizeof(tss_entry_t));
	tss_entry.ss0 = 0x10;
	tss_entry.esp = (uint32_t) &kernel_stack_end;
	tss_entry.cs = 0x0B;
	tss_entry.ss =
	tss_entry.ds =
	tss_entry.es =
	tss_entry.fs =
	tss_entry.gs = 0x13;

	gdt_load();

	
	
	
	idt_p.limit = sizeof(idt_entry_t) * 256 - 1;
	idt_p.base = (uint32_t) idt_e;
	
	memset(idt_e, 0, sizeof(idt_entry_t) * 256);
	
	
	#define idt_set(i, b, s, f)						\
		idt_e[i].base_l = b & 0xFFFF;				\
		idt_e[i].base_h = (b >> 16) & 0xFFFF;		\
		idt_e[i].sel = s;							\
		idt_e[i].null = 0;							\
		idt_e[i].flags = f
	
	
	#define _i(n)									\
		extern void isr##n();						\
		idt_set(n, (uint32_t)isr##n, 0x08, 0x8E)

		
	_i(0);
	_i(1);
	_i(2);
	_i(3);
	_i(4);
	_i(5);
	_i(6);
	_i(7);
	_i(8);
	_i(9);
	_i(10);
	_i(11);
	_i(12);
	_i(13);
	_i(14);
	_i(15);
	_i(16);
	_i(17);
	_i(18);
	_i(19);
	_i(20);
	_i(21);
	_i(22);
	_i(23);
	_i(24);
	_i(25);
	_i(26);
	_i(27);
	_i(28);
	_i(29);
	_i(30);
	_i(31);
	_i(0x80);

	
	idt_load();

	
	#ifdef _i
	#undef _i
	#endif
	
	#define _i(n)										\
		extern void irq##n();							\
		idt_set(n + 32, (uint32_t)irq##n, 0x08, 0x8E)
		
	
	outb(0x20, 0x11);
	outb(0xA0, 0x11);
	outb(0x21, 0x20);
	outb(0xA1, 0x28);
	outb(0x21, 0x04);
	outb(0xA1, 0x02);
	outb(0x21, 0x01);
	outb(0xA1, 0x01);
	outb(0x21, 0x00);
	outb(0xA1, 0x00);
	
	_i(0);
	_i(1);
	_i(2);
	_i(3);
	_i(4);
	_i(5);
	_i(6);
	_i(7);
	_i(8);
	_i(9);
	_i(10);
	_i(11);
	_i(12);
	_i(13);
	_i(14);
	_i(15);
	
	
	uint32_t freq = 1193180 / PIT_FREQ;
	outb(0x43, 0x36);
	outb(0x40, (uint8_t) (freq & 0xFF));
	outb(0x40, (uint8_t) ((freq >> 8) & 0xFF));
	
	irq_set(0, (void*) pit_handler);
	
	__asm__ ("sti");

	return 0;
}



int irq_set(int n, void* handler) {
	if(n > 16) {
		errno = EINVAL;
		return -1;
	}
	
	irqs[n] = handler;
	return 0;
}

int irq_unset(int n) {
	if(n > 16) {
		errno = EINVAL;
		return -1;
	}
	
	irqs[n] = 0;
	return 0;
}

void* irq_get(int n) {
	if(n > 16) {
		errno = EINVAL;
		return NULL;
	}
	
	return irqs[n];
}



static uint8_t rtc(uint8_t addr) {
	outb(0x70, addr);
	
	uint8_t r = 0;
	__asm__("inb 0x71" : "=a"(r));
	return r;
}

uint32_t timer_gettime() {

	#define BCD2BIN(bcd) 	((((bcd) & 0x0F) + ((bcd) / 16) * 10))
	#define BCD2BIN2(bcd)	(((((bcd) & 0x0F) + ((bcd & 0x70) / 16) * 10)) | (bcd & 0x80))

	
	static struct tm t;
	t.tm_sec = BCD2BIN(rtc(0));
	t.tm_min = BCD2BIN(rtc(2));
	t.tm_hour = BCD2BIN2(rtc(4)) + 2;
	t.tm_mday = BCD2BIN(rtc(7));
	t.tm_mon = BCD2BIN(rtc(8)) - 1;
	t.tm_year = (BCD2BIN(rtc(9)) + 100);
	t.tm_wday = 0;
	t.tm_yday = 0;
	t.tm_isdst = 0;
	
	return (uint32_t) mktime(&t);
}

uint32_t timer_getticks() {
	return ((pit_days * 86400) * PIT_FREQ) + (pit_seconds * PIT_FREQ) + pit_ticks;
}

uint32_t timer_getms() {
	return timer_getticks();
}

uint32_t timer_getfreq() {
	return PIT_FREQ;
}


void go_usermode() {
#ifdef DEBUG
	kprintf("aplus: loading user mode\n");
#endif

	
	extern void __go_usermode();
#if 0
	__go_usermode();
#endif

}


EXPORT_SYMBOL(timer_gettime);
EXPORT_SYMBOL(timer_getticks);
EXPORT_SYMBOL(timer_getms);
EXPORT_SYMBOL(timer_getfreq);

EXPORT_SYMBOL(irq_set);
EXPORT_SYMBOL(irq_unset);
EXPORT_SYMBOL(irq_get);

#endif
