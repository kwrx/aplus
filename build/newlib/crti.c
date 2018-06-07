/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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


#if defined(__i386__)
__asm__ (
	".section .init			\n"
	".global _init			\n"
	".type _init, @function \n"
	"_init:					\n"
	"	push %ebp			\n"
	"	movl %esp, %ebp		\n"
	
	".section .fini			\n"
	".global _fini			\n"
	".type _fini, @function	\n"
	"_fini:					\n"
	"	push %ebp			\n"
	"	movl %esp, %ebp		\n"
);
#elif defined(__x86_64__)
__asm__ (
	".section .init			\n"
	".global _init			\n"
	".type _init, @function \n"
	"_init:					\n"
	"	push %rbp			\n"
	"	movl %rsp, %rbp		\n"
	
	".section .fini			\n"
	".global _fini			\n"
	".type _fini, @function	\n"
	"_fini:					\n"
	"	push %rbp			\n"
	"	movl %rsp, %rbp		\n"
);
#endif

