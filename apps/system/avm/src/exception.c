#include <avm.h>
#include "ops.h"


#include "opcodes/opcode.h"


static void unhandled_exception(java_context_t* j, const char* exception, const char* message) {
	if(likely(j)) {
		PRINTF("(Unhandled Exception) %s: %s\n", j->exception.name, j->exception.message);

		j = j->exception.owner;


		int ln = -1;
		java_attribute_t* a;
		if(java_attribute_find(&a, j->assembly, j->frame.method->code->code.attributes, j->frame.method->code->code.attributes_count, "LineNumberTable") == J_OK) {
			ln++;
			while(PC > a->line_number_table.table[ln].start_pc && ln < a->line_number_table.line_number_table_length)
				ln++;

			ln = a->line_number_table.table[ln - 1].line_number;			
		}

	
		char* ss = "";
		if(java_attribute_find(&a, j->assembly, j->assembly->java_this.jc_attributes, j->assembly->java_this.jc_attributes_count, "SourceFile") == J_OK)
			ss = (char*) j->assembly->java_this.jc_cp[a->sourcefile.sourcefile_index].utf8_info.bytes;


		PRINTF("\tat %s.%s(%s:%d)\n", j->assembly->name, j->assembly->java_this.jc_cp[j->frame.method->name_index].utf8_info.bytes, ss, ln);
		PRINTF("\tat bytecode: [%d+%d] %s\n", PB, PC - PB, j_opcodes[j->frame.code[PB]].name);

	} else
		PRINTF("(Unhandled Exception) %s: %s\n", exception, message);

	
	exit(1);
}

void athrow(java_context_t* j, const char* exception, const char* message) {
	if(likely(j)) {
		j->flags = JAVACTX_FLAG_EXCEPTION;
		j->exception.name = strdup(exception);
		j->exception.message = strdup(message);
		j->exception.owner = j;
	
		return;
	}

	unhandled_exception(NULL, exception, message);
}


void rethrow(java_context_t* j, java_context_t* d) {
	if(likely(d)) {
		d->flags = JAVACTX_FLAG_EXCEPTION;
		d->exception.name = j->exception.name;
		d->exception.message = j->exception.message;
		d->exception.owner = j->exception.owner;
	
		return;
	}

	unhandled_exception(j, j->exception.name, j->exception.message);
}
