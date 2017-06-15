#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <arch/i386/i386.h>
#include <libc.h>

#if DEBUG
#include <libdis.h>


spinlock_t lck_debug = SPINLOCK_UNLOCKED;


void debug_send(char value) {

	spinlock_lock(&lck_debug);
	static int initialized = 0;

	if(!initialized) {
		initialized = 1;

#if CONFIG_SERIAL_DEBUG
		outb(0x3F8 + 1, 0x00);
		outb(0x3F8 + 3, 0x80);
		outb(0x3F8 + 0, 0x03);
		outb(0x3F8 + 1, 0x00);
		outb(0x3F8 + 3, 0x03);
		outb(0x3F8 + 2, 0xC7);
		outb(0x3F8 + 4, 0x0B);
#endif	
	}


#if CONFIG_BOCHS
	outb(0xE9, value);
#endif


#if CONFIG_SERIAL_DEBUG
	int i;
	for(i = 0; i < 100000 && ((inb(0x3F8 + 5) & 0x20) == 0); i++)
		;
	outb(0x3F8, value);
#endif
	

	spinlock_unlock(&lck_debug);
}


void debug_dump(void* _context, char* errmsg, uintptr_t dump) {
	#define lookup(s, a)														\
		!(s = binfmt_lookup_symbol(current_task->image->symtab, a))				\
			? !(s = binfmt_lookup_symbol(kernel_task->image->symtab, a))		\
				? s = "<unknown>" : (void) 0 : (void) 0;


	i386_context_t* context = (i386_context_t*) _context;


	char* sym;
	lookup(sym, context->eip);
	
	kprintf(ERROR "%s\n"
				  "\t Task: %d (%s)\n"
				  "\t Address: %p\n"
				  "\t PC: %p (%s)\n"
				  "\t SP: %p\n",
				  errmsg, 
				  current_task->pid, current_task->name,
				  dump,
				  context->eip, sym, context->esp
	);

	if(!dump)
		return;

	lookup(sym, dump);
	kprintf("Dump: %s\n", sym);


	static char line[BUFSIZ];
	memset(line, 0, sizeof(line));


	x86_insn_t i;
	x86_init(opt_none, NULL, NULL);
	
	size_t s, p = 0;
	do {
		while((s = x86_disasm((void*) dump, 64, 0, p, &i)) > 0) {
			x86_format_insn(&i, line, BUFSIZ, intel_syntax);
			kprintf("\t%08x:\t\t%s\n", dump + p, line);

			p += s;
		}

		p++;
	} while(p < BUFSIZ);

	x86_cleanup();
}

#endif
