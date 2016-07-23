#include <avm.h>
#include "ops.h"
#include "vector.h"


#if CONFIG_JNI
static java_native_t* __jni_queue = NULL;
#endif



j_value java_native_invoke(java_method_t* method, java_native_t* func, j_value* params, int nargs) {
#if CONFIG_JNI
	ffi_cif cif;
	ffi_type* ffi_rettype;
	ffi_type* ffi_args[nargs];
	void* ffi_vals[nargs];



	switch(func->rettype) {
		#define _C(t1, t2)						\
		case t1:								\
			ffi_rettype = &ffi_type_##t2;		\
			break

		_C(T_LONG, sint64);
		_C(T_DOUBLE, double);
		_C(T_INT, sint32);
		_C(T_FLOAT, float);
		_C(T_REFERENCE, pointer);
		_C(T_BYTE, sint8);
		_C(T_CHAR, uint16);
		_C(T_BOOLEAN, sint32);
		_C(T_SHORT, sint16);
		_C(T_VOID, void);

		default:
			LOGF("java_native_invoke() Invalid Return Type: %d", func->rettype);
			return JVALUE_NULL;

		#undef _C
	}


	register const char* s = func->desc;
	register int i = 0, p = 0;
	int np = nargs;

	while(*s && np--) {
		switch(*s) {
			case 'B':
				ffi_args[i] = &ffi_type_sint8;
				break;
			case 'C':
				ffi_args[i] = &ffi_type_uint16;
				break;
			case 'F':
				ffi_args[i] = &ffi_type_float;
				break;
			case 'I':
				ffi_args[i] = &ffi_type_sint32;
				break;
			case 'L':
				ffi_args[i] = &ffi_type_pointer;
				break;
			case 'S':
				ffi_args[i] = &ffi_type_sint16;
				break;
			case 'Z':
				ffi_args[i] = &ffi_type_sint32;
				break;
			case 'D':
				ffi_args[i] = &ffi_type_double;
				break;
			case 'J':
				ffi_args[i] = &ffi_type_sint64;
				break;
			case '[': {
					java_array_t* a = JAVA_ARRAY(params[p++].ptr);
					
					int j;
					for(j = 0; j < a->length; j++) {
						switch(a->type) {
						#define _C(t1, t2)							\
							case t1:								\
								ffi_args[i] = &ffi_type_##t2;		\
								ffi_vals[i] = &params[p];			\
								break

							_C(T_LONG, sint64);
							_C(T_DOUBLE, double);
							_C(T_INT, sint32);
							_C(T_FLOAT, float);
							_C(T_REFERENCE, pointer);
							_C(T_BYTE, sint8);
							_C(T_CHAR, uint16);
							_C(T_BOOLEAN, sint32);
							_C(T_SHORT, sint16);

							default:
								LOGF("java_native_invoke() Invalid Array Type: %d", a->type);
								return JVALUE_NULL;
						}

						#undef _C

						i++; p++;
					}
				}
				continue;
			default:
				LOGF("java_native_invoke() Invalid Param Type: %d", *s);
				return JVALUE_NULL;
		}

		ffi_vals[i++] = &params[p++];
		s++;
	}


	if(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, nargs, ffi_rettype, ffi_args) != FFI_OK) {
		LOG("ffi_prep_cif() != FFI_OK");
		return JVALUE_NULL;
	}


	j_value retval;
	ffi_call(&cif, func->handler, &retval, ffi_vals);
	
	return retval;


#else
	LOG("WARNING: Java Native Interface disabled");
	return JVALUE_NULL;
#endif
}


int java_native_find(java_native_t** func, const char* classname, const char* name) {
#if CONFIG_JNI
	java_native_t* tmp;
	for(tmp = __jni_queue; tmp; tmp = tmp->next) {
		if(
			(strcmp(classname, tmp->classname) == 0) &&
			(strcmp(name, tmp->name) == 0)
		) break;
	}

	if(unlikely(!tmp))
		return J_ERR;

	if(likely(func))
		*func = tmp;

	return J_OK;
#else
	LOG("WARNING: Java Native Interface disabled");
	return J_ERR;
#endif
}

int java_native_add(const char* classname, const char* name, const char* desc, u2 rettype, void* handler) {
#if CONFIG_JNI
	java_native_t* func = (java_native_t*) avm->calloc(sizeof(java_native_t), 1);
	ASSERT(func);

	func->classname = strdup(classname);
	func->name = strdup(name);
	func->desc = strdup(desc);
	func->rettype = rettype;
	func->handler = handler;

	func->next = __jni_queue;
	__jni_queue = func;

	return J_OK;
#else
	LOG("WARNING: Java Native Interface disabled");
	return J_ERR;
#endif
}

void java_native_flush(void) {
#if CONFIG_JNI
	java_native_t* tmp, *tmp2;
	for(tmp = __jni_queue; tmp;) {
		tmp2 = tmp;
		tmp = tmp->next;

		avm->free((void*) tmp2->classname);
		avm->free((void*) tmp2->name);
		avm->free((void*) tmp2->desc);
		avm->free((void*) tmp2);
	}

	__jni_queue = NULL;
#endif
}




#if CONFIG_JNI

#define _THIS	java_object_t* __this
#define THIS	__this

#define API(x)	JNI_##x


#include "api/VMClass.h"
#include "api/VMDebug.h"
#endif


void java_native_init(void) {
#if CONFIG_JNI

#define JNI(c, f, r, s)	\
	java_native_add(c, #f, s, r, JNI_##f)

	
	JNI("AVM/VMClass", hashCode, T_INT, "L");
	JNI("AVM/VMClass", getName, T_REFERENCE, "L");

	JNI("AVM/VMDebug", Print, T_VOID, "L");
#endif
}

