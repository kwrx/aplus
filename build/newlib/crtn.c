#if defined(__i386__)
__asm__ (
	".section .init			\n"
	"	popl %ebp			\n"
	"	ret					\n"
	
	".section .fini			\n"
	"	popl %ebp			\n"
	"	ret					\n"
);
#elif defined(__x86_64__)
__asm__ (
	".section .init			\n"
	"	popl %rbp			\n"
	"	ret					\n"
	
	".section .fini			\n"
	"	popl %rbp			\n"
	"	ret					\n"
);
#endif
