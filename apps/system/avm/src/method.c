#include <avm.h>
#include "ops.h"


int java_method_find(java_method_t** method, const char* classname, const char* methodname, const char* signature) {
	java_assembly_t* A = NULL;
	if(java_assembly_find(&A, classname) != J_OK) {
		LOGF("java_method_find() Cannot found Assembly %s", classname);	
		return J_ERR;
	}

	int i;
	for(i = 0; i < A->java_this.jc_methods_count; i++) {
		char* name = (char*) A->java_this.jc_cp[A->java_this.jc_methods[i].name_index].utf8_info.bytes;

		if(strcmp(name, methodname) == 0) {
			if(likely(signature)) {
				name = (char*) A->java_this.jc_cp[A->java_this.jc_methods[i].desc_index].utf8_info.bytes;
			
				if(strcmp(name, signature) != 0)
					continue;
			}

			if(likely(method))
				*method = &A->java_this.jc_methods[i];

			return J_OK;
		}
	}

	return J_ERR;
}


int java_method_find_reference(java_method_t** method, java_assembly_t* assembly, java_method_t* methods, u2 idx) {
	u2 cidx = assembly->java_this.jc_cp[idx].method_ref.class_index;
	u2 nidx = assembly->java_this.jc_cp[idx].method_ref.name_and_type_index;

	char* name = (char*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[nidx].name_and_type_info.name_index].utf8_info.bytes;
	char* desc = (char*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[nidx].name_and_type_info.desc_index].utf8_info.bytes;
	const char* classname = (const char*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[cidx].class_info.name_index].utf8_info.bytes;
	
	if(cidx == assembly->java_this.jc_super)
		assembly = assembly->java_super;
	else if (cidx != assembly->java_this.jc_this) {
		if(java_assembly_find(&assembly, classname) != J_OK) {
			LOGF("java_method_find_reference() Assembly %s not found!", classname);
			return J_ERR;
		}
	}

	if(unlikely(!methods))
		methods = assembly->java_this.jc_methods;
	

	int i;
	for(i = 0; i < assembly->java_this.jc_methods_count; i++) {

		if(
			(strcmp(methods[i].name, name) == 0)	&&
			(strcmp(methods[i].desc, desc) == 0)
		) {
			if(likely(method))
				*method = &methods[i];			

			return J_OK;
		}
	}
	
	LOGF("java_method_find_reference() No such method %s.%s", classname, name);

	return J_ERR;
}


int java_method_resolve(java_method_t* method, java_assembly_t* assembly) {
	char* signature = (char*) assembly->java_this.jc_cp[method->desc_index].utf8_info.bytes;
	char* p = signature;
	int i = 0;
	
	if(*p != '(')
		goto noparams;
	p++;

	method->signature = (u1*) avm->calloc(1, strlen(signature));
	ASSERT(method->signature);

	while(*p && *p != ')') {
		switch(*p) {
			case '[':
				do {
					p++;
				} while(*p == '[');
				p--;
			case 'L':
				p++;
				switch(*p) {
					case 'B':
					case 'C':
					case 'D':
					case 'F':
					case 'I':
					case 'J':
					case 'S':
					case 'Z':
					case 'V':
						break;
					default:
						do {
							p++;
						} while(*p != ';');
						break;
				}
				method->signature[i++] = 'L';
				break;
			default:
				method->signature[i++] = *p;
				break;
		}
		
		method->nargs += 1;
		p++;
	}

	if(*p != ')')
		return J_ERR;
	p++;
	
noparams:

	switch(*p) {
		case 'B':
			method->rettype = T_BYTE;
			break;
		case 'C':
			method->rettype = T_CHAR;
			break;
		case 'D':
			method->rettype = T_DOUBLE;
			break;
		case 'F':
			method->rettype = T_FLOAT;
			break;
		case 'I':
			method->rettype = T_INT;
			break;
		case 'J':
			method->rettype = T_LONG;
			break;
		case 'L':
		case '[':
			method->rettype = T_REFERENCE;
			break;
		case 'S':
			method->rettype = T_SHORT;
			break;
		case 'Z':
			method->rettype = T_BOOLEAN;
			break;
		case 'V':
			method->rettype = T_VOID;
			break;
		default:
			LOGF("warning: return type %c is undefined\n", *p);
			method->rettype = T_VOID;
	}


	java_attribute_find(&method->code, assembly, method->attributes, method->attr_count, "Code");
	method->assembly = assembly;

	method->name = strdup((const char*) assembly->java_this.jc_cp[method->name_index].utf8_info.bytes);
	method->desc = strdup((const char*) assembly->java_this.jc_cp[method->desc_index].utf8_info.bytes);

	return J_OK;
}


j_value java_method_invoke(java_context_t* j, java_assembly_t* assembly, java_method_t* method, j_value* params, int nargs) {

	if(unlikely(method->flags & ACC_NATIVE)) {

		java_native_t* func;
	 	if(java_native_find(&func, method->assembly->name, method->name) != J_OK) {
			LOGF("java_method_invoke() Native Method %s.%s not found!", method->assembly->name, method->name);
			return JVALUE_NULL;
		}

		LOGF("> (native) %s.%s (%s)", func->classname, func->name, func->desc);
		j_value r = java_native_invoke(method, func, params, nargs);
		LOGF("---> %lld", r.i64);

		return r;
	}




	java_context_t* ctx = (java_context_t*) avm->calloc(sizeof(java_context_t), 1);
	ASSERT(ctx);

	ctx->parent = j;
	ctx->assembly = assembly;
	ctx->flags = JAVACTX_FLAG_CONTINUE;
	
	ctx->frame.stack = (j_value*) avm->calloc(sizeof(j_value), method->code->code.max_stack);
	ctx->frame.locals = (j_value*) avm->calloc(sizeof(j_value), method->code->code.max_locals);

	ASSERT(ctx->frame.stack);
	ASSERT(ctx->frame.locals);
	
	ctx->frame.stack_size = method->code->code.max_stack;
	ctx->frame.locals_size = method->code->code.max_stack;

	ctx->frame.code = method->code->code.code;
	ctx->frame.method = method;
	
	ctx->frame.regs.pc = 0;
	ctx->frame.regs.sp = 0;
	ctx->frame.regs.pb = 0;


	int i, p;
	for(i = 0, p = 0; i < nargs; i++, p++) {

		ctx->frame.locals[p] = params[i];

		switch(method->signature[i]) {
			case 'J':
			case 'D':
				p++;
				break;
		}	
	}


	LOGF("> %s.%s ()", assembly->name, assembly->java_this.jc_cp[method->name_index].utf8_info.bytes);
	java_context_run(ctx);
	LOGF("---> %lld",  ctx->frame.retval.i64);


	j_value r = ctx->frame.retval;

	if(likely(ctx->flags == JAVACTX_FLAG_SUCCESS)) {
		avm->free(ctx->frame.stack);
		avm->free(ctx->frame.locals);
		avm->free(ctx);
	}

	
	return r;
}

