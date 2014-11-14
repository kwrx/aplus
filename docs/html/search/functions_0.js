var searchData=
[
  ['_5f_5falphablend',['__alphablend',['../clrconv_8h.html#ab7d4dfdb7e512b070be9ba847e71547f',1,'clrconv.h']]],
  ['_5f_5fasm_5f_5f',['__asm__',['../desc_8c.html#ad365b1ba0a341f9a84551327fe0dac7b',1,'__asm__(&quot;.section .text		\n\
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
						\n&quot;):&#160;desc.c'],['../task_8c.html#a30c5de54c48d098388d2048073a393a7',1,'__asm__(&quot;.global task_context_switch		\n&quot;&quot;task_context_switch:				\n&quot;&quot;	push ebp						\n&quot;&quot;	mov ebp, esp					\n&quot;&quot;	pushf							\n&quot;&quot;	push edx						\n&quot;&quot;	push eax						\n&quot;&quot;	push ecx						\n&quot;&quot;	push ebx						\n&quot;&quot;	push esi						\n&quot;&quot;	push edi						\n&quot;&quot;	mov eax, [ebp + 8]				\n&quot;&quot;	mov edx, [ebp + 12]				\n&quot;&quot;	mov [eax], esp					\n&quot;&quot;	mov esp, [edx]					\n&quot;&quot;	pop edi							\n&quot;&quot;	pop esi							\n&quot;&quot;	pop ebx							\n&quot;&quot;	pop ecx							\n&quot;&quot;	pop eax							\n&quot;&quot;	pop edx							\n&quot;&quot;	popf							\n&quot;&quot;	pop ebp							\n&quot;&quot;	sti								\n&quot;&quot;ret								\n&quot;):&#160;task.c']]],
  ['_5f_5fattribute_5f_5f',['__attribute__',['../desc_8c.html#ae21df847b32e33e64ba0912dd628e610',1,'__attribute__((packed)):&#160;desc.c'],['../iso9660_8h.html#a11b1fe8d934cf9dd11e83e3a093b68db',1,'__attribute__((packed)) iso9660_volume_descriptor_t:&#160;desc.c'],['../attribute_8h.html#a4b7f6d2e9be3c83ead055e037a05f9d0',1,'__attribute__((aligned(0x100))) attribute_t:&#160;attribute.h'],['../arp_8h.html#aa746629dfa56980dbf09cc6edf68c467',1,'__attribute__((packed)) arp_header_t:&#160;desc.c'],['../eth_8h.html#a5af061aaa0da1dfd1022f738cc6686cc',1,'__attribute__((packed)) eth_header_t:&#160;desc.c'],['../ipv4_8h.html#aedc8b3a56af6d6fa02c12255c4176f30',1,'__attribute__((packed)) ipv4_header_t:&#160;desc.c'],['../ipv6_8h.html#abbba6b4344c05a2ad89d85e5b128365f',1,'__attribute__((packed)) ipv6_header_t:&#160;desc.c'],['../udp_8h.html#acca43b175052e15f7552d7d46900564b',1,'__attribute__((packed)) udp_header_t:&#160;desc.c'],['../task_8h.html#a82ac5a61b16740abc5cceaaff721c5c8',1,'__attribute__((packed)) task_env_t:&#160;desc.c'],['../aplus_8h.html#a828ca35a80f14c0f9432022c5f3823a4',1,'__attribute__((packed)) regs_t:&#160;desc.c'],['../grub_8h.html#afa73cc1cb823e7947162fcc569b64e5a',1,'__attribute__((packed)) vesaModeInfo:&#160;desc.c'],['../mm_8c.html#a2cdc9850cc059eb848b6843d5327e37a',1,'__attribute__((packed)):&#160;mm.c'],['../crt0_8c.html#a1b6824bfd4ec0404a4f875ed8db0274e',1,'__attribute__((noreturn)):&#160;crt0.c'],['../chkfail_8c.html#a1b6824bfd4ec0404a4f875ed8db0274e',1,'__attribute__((noreturn)):&#160;chkfail.c']]],
  ['_5f_5fdefault_5fsighandler_5f_5f',['__default_sighandler__',['../crt0_8c.html#aa7ab5703ee773f395bff8dc7abf214af',1,'crt0.c']]],
  ['_5f_5fdefault_5ftty_5foutput',['__default_tty_output',['../tty_8c.html#a4cb6812ea931081c175160c7832bd5cb',1,'tty.c']]],
  ['_5f_5ffastlock_5fwaiton',['__fastlock_waiton',['../spinlock_8h.html#a2bf717efcda7e5189e3192e09e49c249',1,'__fastlock_waiton():&#160;spinlock.c'],['../spinlock_8c.html#a2bf717efcda7e5189e3192e09e49c249',1,'__fastlock_waiton():&#160;spinlock.c']]],
  ['_5f_5ffork_5fchild',['__fork_child',['../task_8c.html#ae020ae98e6a6fc90f836f38cc405290c',1,'task.c']]],
  ['_5f_5finit_5ftraps',['__init_traps',['../crt0_8c.html#a54dbf06bfcd3b86af2609c8d594b7907',1,'crt0.c']]],
  ['_5f_5flocked_5fmtx',['__locked_mtx',['../pthread__mutex_8c.html#a09a80d595723a7cb36229098892878f0',1,'pthread_mutex.c']]],
  ['_5f_5fpthread_5fhandler_5f_5f',['__pthread_handler__',['../pthread__create_8c.html#a0177c641d305d417a7960701e701fc07',1,'pthread_create.c']]],
  ['_5f_5fpthread_5finit_5fqueue',['__pthread_init_queue',['../pthread__create_8c.html#acbbda48de9905dc1d40c073a89b66e62',1,'__pthread_init_queue(void):&#160;pthread_queue.c'],['../pthread__queue_8c.html#a524e53db3985af45990760ed9068c0a9',1,'__pthread_init_queue():&#160;pthread_queue.c']]],
  ['_5f_5fsigtramp',['__sigtramp',['../crt0_8c.html#a50595d26a35dfb272a9f822789821724',1,'crt0.c']]],
  ['_5f_5fspinlock_5fwaiton',['__spinlock_waiton',['../spinlock_8h.html#aa40e1417e8410d92c2ffc313e57560e8',1,'__spinlock_waiton():&#160;spinlock.c'],['../spinlock_8c.html#aa40e1417e8410d92c2ffc313e57560e8',1,'__spinlock_waiton():&#160;spinlock.c']]],
  ['_5f_5ftest_5fpthread_5f1',['__test_pthread_1',['../usr_2src_2libpthread_2test_2main_8c.html#abc9e9db807928dccd01360c262a265ae',1,'main.c']]],
  ['_5f_5ftest_5fsignals_5f_5f',['__test_signals__',['../usr_2src_2libatk_2test_2main_8c.html#afd4f7f3e83f5ccc17a12c629a407c900',1,'__test_signals__(int sig):&#160;main.c'],['../usr_2src_2libcrt0_2test_2main_8c.html#afd4f7f3e83f5ccc17a12c629a407c900',1,'__test_signals__(int sig):&#160;main.c']]],
  ['_5fdefun',['_DEFUN',['../execl_8c.html#aaed3b82a6a2931a5ea07cf041fcc5fba',1,'_DEFUN(execl,(path, arg0, va_alist), _CONST char *path _AND _CONST char *arg0 _AND va_dcl):&#160;execl.c'],['../execle_8c.html#a299626bec19ca939a02c5e9941e8d071',1,'_DEFUN(execle,(path, arg0, va_alist), _CONST char *path _AND _CONST char *arg0 _AND va_dcl):&#160;execle.c'],['../execlp_8c.html#aab09774ec2290c965f64151463f38eee',1,'_DEFUN(execlp,(path, arg0, va_alist), _CONST char *path _AND _CONST char *arg0 _AND va_dcl):&#160;execlp.c'],['../execv_8c.html#af785e3a983b2ce8aa8cdfb77e3b5b903',1,'_DEFUN(execv,(path, argv), const char *path _AND char *const argv[]):&#160;execv.c'],['../execvp_8c.html#a10efa145fdda589520d161cf247a568f',1,'_DEFUN(strccpy,(s1, s2, c), char *s1 _AND char *s2 _AND char c):&#160;execvp.c'],['../execvp_8c.html#abd01b9e4dcf0b408353235fe59e17675',1,'_DEFUN(execvp,(file, argv), _CONST char *file _AND char *_CONST argv[]):&#160;execvp.c']]],
  ['_5fexecve',['_execve',['../__execve_8c.html#accd0d84615f4b84367796ea088af9d72',1,'_execve(const char *filename, char *const args[], char *const env[]):&#160;_execve.c'],['../execl_8c.html#accd0d84615f4b84367796ea088af9d72',1,'_execve(const char *filename, char *const args[], char *const env[]):&#160;_execve.c'],['../execle_8c.html#accd0d84615f4b84367796ea088af9d72',1,'_execve(const char *filename, char *const args[], char *const env[]):&#160;_execve.c'],['../execlp_8c.html#accd0d84615f4b84367796ea088af9d72',1,'_execve(const char *filename, char *const args[], char *const env[]):&#160;_execve.c'],['../execv_8c.html#accd0d84615f4b84367796ea088af9d72',1,'_execve(const char *filename, char *const args[], char *const env[]):&#160;_execve.c']]]
];
