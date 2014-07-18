//
//  desc.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is kfree software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the kfree Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>

#include <grub.h>


#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/times.h>


#define FAULT_MASK		0x3FFFFFE3
#define PIT_FREQ		CLOCKS_PER_SEC


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

static gdt_entry_t gdt_e[5];
static gdt_ptr_t gdt_p;

static idt_entry_t idt_e[256];
static idt_ptr_t idt_p;


static uint64_t pit_ticks;
static uint64_t pit_seconds;
static uint64_t pit_days;

uint8_t keyboard_ready = 0;

extern uint32_t schedule(uint32_t);
extern task_t* current_task;

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
						\n\
						\n\
						\n\
						\n\
	.global isr_stub	\n\
	isr_stub:			\n\
	pusha				\n\
	push ds				\n\
	push es				\n\
	push fs				\n\
	push gs				\n\
						\n\
	mov ax, 0x10		\n\
	mov ds, ax			\n\
	mov es, ax			\n\
	mov fs, ax			\n\
	mov gs, ax			\n\
						\n\
	mov eax, esp		\n\
	push eax			\n\
	call isr_handler	\n\
						\n\
	pop eax				\n\
	pop gs				\n\
	pop fs				\n\
	pop es				\n\
	pop ds				\n\
	popa				\n\
	add esp, 8			\n\
	sti					\n\
	iret				\n\
						\n\
						\n\
						\n\
						\n\
						\n\
						\n\
	.global irq_stub	\n\
	irq_stub:			\n\
	pusha				\n\
	push ds				\n\
	push es				\n\
	push fs				\n\
	push gs				\n\
						\n\
	mov ax, 0x10		\n\
	mov ds, ax			\n\
	mov es, ax			\n\
	mov fs, ax			\n\
	mov gs, ax			\n\
						\n\
	mov eax, esp		\n\
	push eax			\n\
	call irq_handler	\n\
						\n\
	pop eax				\n\
	pop gs				\n\
	pop fs				\n\
	pop es				\n\
	pop ds				\n\
	popa				\n\
	add esp, 8			\n\
	sti					\n\
	iret				\n"
);


void isr_handler(regs_t* r) {
	char buf[64] = { 0 };
	memset(buf, 0, sizeof(64));
	
	int pid = 0;
	if(current_task)
		pid = current_task->pid;
	
	char* exe_name = "";
	if(current_task)
		if(current_task->exe)
			exe_name = current_task->exe->name;

	sprintf(buf, "Exception: (%u) %s (errno: %d; errcode: 0x%x; pid: %d; exe: \"%s\")\n", r->int_no, exception_messages[r->int_no], errno, r->err_code, pid, exe_name);
	video_puts(buf);
		
	if((FAULT_MASK >> r->int_no) & 1) {
		video_puts("Task crashed!\n");
		_exit(-r->err_code);
	}
}

void irq_handler(regs_t* r) {

	if(r->int_no >= 40)
		outb(0xA0, 0x20);
		
	outb(0x20, 0x20);

	
	if(irqs[r->int_no - 32])
		((void (*)(regs_t*)) irqs[r->int_no - 32]) (r);
}

uint32_t pit_handler(uint32_t esp) {
	pit_ticks += (1000 / PIT_FREQ);
	
	if(pit_ticks >= 1000) {
		pit_ticks = 0;
		pit_seconds += 1;
	}
	
	if(pit_seconds >= 86400) {
		pit_seconds = 0;
		pit_days += 1;
	}
	
	return schedule(esp);
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
				
	
	memset(&gdt_e, 0, sizeof(gdt_entry_t) * 5);
		
	gdt_p.limit = sizeof(gdt_entry_t) * 5 - 1;
	gdt_p.base = (uint32_t) gdt_e;
		
	gdt_set(0, 0, 0, 0, 0);
	gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
	gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
	gdt_set(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
	gdt_set(4, 0, 0xFFFFFFFF, 0XF2, 0xCF);

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
		errno = -EINVAL;
		return 1;
	}
	
	irqs[n] = handler;
	return 0;
}

int irq_unset(int n) {
	if(n > 16) {
		errno = -EINVAL;
		return 1;
	}
	
	irqs[n] = 0;
	return 0;
}



static uint8_t rtc(uint8_t addr) {
	outb(0x70, addr);
	
	uint8_t r = 0;
	__asm__("inb 0x71" : "=a"(r));
	return r;
}

uint64_t pit_gettime() {

	#define BCD2BIN(bcd) 	((((bcd) & 0x0F) + ((bcd) / 16) * 10))
	#define BCD2BIN2(bcd)	(((((bcd) & 0x0F) + ((bcd & 0x70) / 16) * 10)) | (bcd & 0x80))

	
	static struct tm t;
	t.tm_sec = BCD2BIN(rtc(0));
	t.tm_min = BCD2BIN(rtc(2));
	t.tm_hour = BCD2BIN2(rtc(4)) + 2;
	t.tm_mday = BCD2BIN(rtc(7));
	t.tm_mon = BCD2BIN(rtc(8)) - 1;
	t.tm_year = BCD2BIN(rtc(9)) + 100;
	t.tm_wday = 0;
	t.tm_yday = 0;
	t.tm_isdst = 0;
	
	return (uint64_t) mktime(&t);
}

uint32_t pit_getticks() {
	return pit_ticks;
}

EXPORT(irq_set);
EXPORT(irq_unset);