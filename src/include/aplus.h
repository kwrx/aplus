
//
//  aplus.h
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

#ifndef _APLUS_H
#define _APLUS_H


#include <stdint.h>
#include <stdarg.h>
#include <string.h>


#define read_reg(n)												\
	static inline uint32_t read_##n() {							\
		uint32_t ret;											\
		__asm__ __volatile__ ("mov %0, " #n : "=r"(ret));		\
		return ret;												\
	}
	
	read_reg(eax)
	read_reg(ebx)
	read_reg(ecx)
	read_reg(edx)
	read_reg(esi)
	read_reg(edi)
	read_reg(ebp)
	read_reg(esp)
	read_reg(cr0)
	read_reg(cr1)
	read_reg(cr2)
	read_reg(cr3)
	read_reg(cr4)
	
	
#define write_reg(n)											\
	static inline void write_##n(uint32_t val) {				\
		__asm__ __volatile__ ("mov " #n ", %0" : : "r"(val));	\
	}


	write_reg(eax)
	write_reg(ebx)
	write_reg(ecx)
	write_reg(edx)
	write_reg(esi)
	write_reg(edi)
	write_reg(ebp)
	write_reg(esp)
	write_reg(cr0)
	write_reg(cr1)
	write_reg(cr2)
	write_reg(cr3)
	write_reg(cr4)
	

#define outx(n, t, reg)												\
	static inline void out##n(uint16_t p, t v) {					\
		__asm__ __volatile__ ("out dx, " #reg : : "a"(v), "d"(p));	\
	}
	
	outx(b, uint8_t, al)
	outx(w, uint16_t, ax)
	outx(l, uint32_t, eax)
	
	
#define inx(n, t, reg)												\
	static inline t in##n(uint16_t p) {								\
		t r;														\
		__asm__ __volatile__ ("in " #reg ", dx" : "=a"(r) : "d"(p));\
		return r;													\
	}
	
	inx(b, uint8_t, al)
	inx(w, uint16_t, ax)
	inx(l, uint32_t, eax)
	
	
typedef struct regs {
	uint32_t gs, fs, es, ds, edi, esi, ebp, esp, ebx, edx, ecx, eax, int_no, err_code, eip, cs, eflags, useresp;
} __attribute__((packed)) regs_t;




#ifdef DEBUG
static inline void kprintf(char* fmt, ...) {
	static char __kprintf_buf[1024];
	memset(__kprintf_buf, 0, 1024);
	
	va_list lst;
	va_start(lst, fmt);
	vsprintf(__kprintf_buf, fmt, lst);
	va_end(lst);
	
	video_puts(__kprintf_buf);
}

#else
#define kprintf
#endif


extern void __panic(char*, char*, char*, int);
#define panic(msg) __panic(msg, __FILE__, __func__, __LINE__)




struct kernel_symbol {
	char* name;
	void* handler;
} __attribute__((packed));


#define EXPORT(f)																				\
	struct kernel_symbol kernel_symbol__##f __attribute__((section(".kernel_symbols"))) = {		\
		#f, (void*) f																			\
	}


#endif