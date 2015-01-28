#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

#include <jvm/jvm.h>


#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "java/j_object_class.h"
#include "java/j_runtime_class.h"
#include "java/j_program_class.h"

int color = 0;
void screenclr() {
	memset(mbd->lfb.base, color++, mbd->lfb.size);
}

int __rpi_java_exec() {
	jinit();
	
	jnative_register_function("jvm/Runtime", "screenclr", "", T_VOID, screenclr);

	jassembly_t* j_object;
	jassembly_load_memory(&j_object, "java/lang/Object", j_object_class, sizeof(j_object_class));

	jassembly_t* j_runtime;
	jassembly_load_memory(&j_runtime, "jvm/Runtime", j_runtime_class, sizeof(j_runtime_class));
	
	jassembly_t* j_program;
	jassembly_load_memory(&j_program, "Program", j_program_class, sizeof(j_program_class));


	jvalue_t ret = jcode_function_call(j_program, "main", NULL, 0);
	jerr_check_exceptions(NULL);

	return 0;
}

#endif
