#include <avm.h>
#include "ops.h"
#include "vector.h"




void parse_constval(java_assembly_t* assembly, void* buffer, java_attribute_t* attribute, u4 length) {
	R16(attribute->const_value.const_value_index);
}

void parse_code(java_assembly_t* assembly, void* buffer, java_attribute_t* attribute, u4 length) {
	R16(attribute->code.max_stack);
	R16(attribute->code.max_locals);
	R32(attribute->code.code_length);
	
	attribute->code.code = (u1*) avm->calloc(sizeof(u1), attribute->code.code_length);
	ASSERT(attribute->code.code);

	RXX(attribute->code.code, attribute->code.code_length);

	R16(attribute->code.exception_table_length);
	attribute->code.exception_table = (java_exception_table_t*) avm->calloc(sizeof(java_exception_table_t), attribute->code.exception_table_length);
	ASSERT(attribute->code.exception_table);
	
	int i;
	for(i = 0; i < attribute->code.exception_table_length; i++) {
		R16(attribute->code.exception_table[i].start_pc);
		R16(attribute->code.exception_table[i].end_pc);
		R16(attribute->code.exception_table[i].handler_pc);
		R16(attribute->code.exception_table[i].catch_type);
	}

	R16(attribute->code.attributes_count);
	java_attribute_load(assembly, buffer, &attribute->code.attributes, attribute->code.attributes_count);
}

void parse_exceptions(java_assembly_t* assembly, void* buffer, java_attribute_t* attribute, u4 length) {
	R16(attribute->exceptions.number_of_exceptions);
	attribute->exceptions.exception_index_table = (u2*) avm->calloc(sizeof(u2), attribute->exceptions.number_of_exceptions);
	ASSERT(attribute->exceptions.exception_index_table);


	int i;
	for(i = 0; i < attribute->exceptions.number_of_exceptions; i++)
		R16(attribute->exceptions.exception_index_table[i]);
}

void parse_sourcefile(java_assembly_t* assembly, void* buffer, java_attribute_t* attribute, u4 length) {
	R16(attribute->sourcefile.sourcefile_index);
}

void parse_lntable(java_assembly_t* assembly, void* buffer, java_attribute_t* attribute, u4 length) {
	R16(attribute->line_number_table.line_number_table_length);
	attribute->line_number_table.table = (java_line_number_table_t*) avm->calloc(sizeof(java_line_number_table_t), attribute->line_number_table.line_number_table_length);
	ASSERT(attribute->line_number_table.table);
	
	int i;
	for(i = 0; i < attribute->line_number_table.line_number_table_length; i++) {
		R16(attribute->line_number_table.table[i].start_pc);
		R16(attribute->line_number_table.table[i].line_number);
	}
}



struct {
	char* name;
	void (*parse) (java_assembly_t*, void*, java_attribute_t*, u4);
} __attr_info[] = {
	{ "ConstantValue", parse_constval },
	{ "Code", parse_code },
	{ "StackMapTable", NULL },
	{ "Exceptions", parse_exceptions },
	{ "InnerClasses", NULL },
	{ "EnclosingMethod", NULL },
	{ "Synthetic", NULL },
	{ "Signature", NULL },
	{ "SourceFile", parse_sourcefile },
	{ "SourceDebugExtension", NULL },
	{ "LineNumberTable", parse_lntable },
	{ "LocalVariableTable", NULL },
	{ "LocalVariableTypeTable", NULL },
	{ "Deprecated", NULL },
	{ "RuntimeVisibleAnnotations", NULL },
	{ "RuntimeInvisibleAnnotations", NULL },
	{ "RuntimeVisibileParameterAnnotations", NULL },
	{ "RuntimeInvisibleParameterAnnotations", NULL },
	{ "AnnotationDefault", NULL },
	{ "BootstapMethods", NULL },
	{ NULL, NULL }
};


int java_attribute_load(java_assembly_t* assembly, void* buffer, java_attribute_t** attributes, u2 attr_count) {
	long buffer_s = (long) buffer;	
	*attributes = (java_attribute_t*) avm->calloc(sizeof(java_attribute_t), attr_count);
	ASSERT(*attributes);

	#define A (*attributes)


	int i;
	for(i = 0; i < attr_count; i++) {
		R16(A[i].name_index);
		R32(A[i].length);

		char* name = (char*) assembly->java_this.jc_cp[A[i].name_index].utf8_info.bytes;
		
		int j = 0;
		while(__attr_info[j].name) {
			if(strcmp(__attr_info[j].name, name) == 0) {
				if(__attr_info[j].parse)
					__attr_info[j].parse(assembly, buffer, &A[i], A[i].length);
				
				RXX(NULL, A[i].length);
			}

			j++;
		}
	}

	#undef A

	return (int) ((long) buffer - buffer_s);
}


int java_attribute_find(java_attribute_t** attribute, java_assembly_t* assembly, java_attribute_t* A, int attr_count, const char* name) {
	int i;
	for(i = 0; i < attr_count; i++) {
		char* Aname = (char*) assembly->java_this.jc_cp[A[i].name_index].utf8_info.bytes;
	
		if(strcmp(Aname, name) == 0) {
			if(likely(attribute))
				*attribute = &A[i];			

			return J_OK;
		} 
	}

	return J_ERR;
}
