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

