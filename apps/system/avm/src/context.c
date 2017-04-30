#include <avm.h>
#include "ops.h"

#define _WITH_OPCODES
#include "opcodes/opcode.h"



static int __check_exception(const char* exname, const char* aname) {
	

	java_assembly_t* A;
	if(java_assembly_find(&A, aname) != J_OK)
		return J_ERR;

	for(; A; A = A->java_super)
		if(strcmp(A->name, aname) == 0)
			return J_OK;

	return J_ERR;
}


void java_context_run(java_context_t* j) {
	do {
		register u1 opcode = PC8;


		LOGF("[%d] (%2X) %s", PC, (int) opcode, j_opcodes[opcode].name);


		PB = PC;
		PC += 1;
		j_opcodes[opcode].handler (j);

	} while(j->flags == JAVACTX_FLAG_CONTINUE);


	if(unlikely(j->flags == JAVACTX_FLAG_EXCEPTION)) {

		register long i;
		for(i = 0; i < j->frame.method->code->code.exception_table_length; i++) {

			int spc = j->frame.method->code->code.exception_table[i].start_pc;
			int epc = j->frame.method->code->code.exception_table[i].end_pc;
			const char* exname = (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[j->frame.method->code->code.exception_table[i].catch_type].class_info.name_index].utf8_info.bytes;


			if(j->frame.regs.pb >= spc && j->frame.regs.pb <= epc) {
				if((strcmp(j->exception.name, exname) == 0) || (__check_exception(j->exception.name, exname) == J_OK)) {

					java_object_t* exobj = NULL;
					java_object_new(&exobj, j->exception.name);

					JPUSH(ptr, (void*) exobj);



					PC = PB = j->frame.method->code->code.exception_table[i].handler_pc;

					j->exception.name = NULL;
					j->exception.message = NULL;
					j->exception.owner = NULL;
					j->flags = JAVACTX_FLAG_CONTINUE;

					
					return java_context_run(j);
				}
			}
		}

		rethrow(j, j->parent);
		return;
	}

#if HAVE_GC_H
	//GC_gcollect();
#endif

	j->flags = JAVACTX_FLAG_SUCCESS;
}
