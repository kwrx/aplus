#include <avm.h>
#include "ops.h"
#include "vector.h"



extern vector_t* asm_vector;
static vector_t* path_vector = NULL;

static int __avm_initialized = 0;
static char* __entrypoint = NULL;

void avm_config_path_add(const char* dir) {
	vector_add(&path_vector, strdup(dir));
}

void avm_config_set_ops (
	void* calloc,
	void* free,
	void* open,
	void* close,
	void* lseek,
	void* read,
	void* yield,
	void* getpid,
	void* printf
) {
	#define C(x)			\
		if((x))			\
			avm->x = x

	C(calloc);
	C(free);
	C(open);
	C(close);
	C(lseek);
	C(read);
	C(yield);
	C(getpid);
	C(printf);
						
}


void avm_set_entrypoint(char* e) {
	__entrypoint = strdup(e);
}

int avm_initialized(void) {
	return __avm_initialized;
}


void avm_init(void) {
#if HAVE_GC_H
	GC_INIT();
#endif
}

void avm_begin(void) {
	java_native_init();
	java_assembly_init();

	__avm_initialized = 1;
}

void avm_end(void) {
	vector_t* v;
	for(v = path_vector; v; v = v->next)
		avm->free(v->value);

	vector_destroy(&path_vector);


	
	java_object_flush();
	java_native_flush();
	java_library_flush();
	java_assembly_flush();

	if(__entrypoint)
		avm->free(__entrypoint);

	__entrypoint = NULL;
	__avm_initialized = 0;
}


static int __java_assembly_open_wrapper(const char* filename) { return java_assembly_open(NULL, filename); }

int avm_open(const char* filename) {
	int (*open_wrapper) (const char*) = __java_assembly_open_wrapper;

	if(strcmp(&filename[strlen(filename) - 4], ".jar") == 0)
		open_wrapper = jar_open;


	if(open_wrapper(filename) == J_OK)
		return J_OK;


	char buf[1024];
	vector_t* v;
	for(v = path_vector; v; v = v->next) {
		const char* path = (const char*) v->value;
		memset(buf, 0, sizeof(buf));


		strcat(buf, path);
		if(path[strlen(path) - 1] != '/')
			strcat(buf, "/");
		strcat(buf, filename);

		if(open_wrapper(buf) == J_OK)
			return J_OK;
	}

	return J_ERR;
}

int avm_open_library(const char* filename) {

	char buf[1024];
	vector_t* v;
	for(v = path_vector; v; v = v->next) {
		const char* path = (const char*) v->value;
		memset(buf, 0, sizeof(buf));


		strcat(buf, path);
		if(path[strlen(path) - 1] != '/')
			strcat(buf, "/");
		strcat(buf, filename);

		if(java_library_add(NULL, buf) == J_OK)
			return J_OK;
	}

	return J_ERR;
}

int avm_load(void* buffer, int size, const char* name) {
	return java_assembly_load(NULL, buffer, size, name);
}



char* avm_make_signature(int rettype, ...) {
	va_list ll;
	va_start(ll, rettype);

	char _buf[4096];
	int i;
	for(i = 0; i < sizeof(_buf); i++)
		_buf[i] = 0;

	
	char* buf = (char*) _buf;
	char* retref = NULL;

	if((rettype & T_MASK) == T_REFERENCE)
		retref = va_arg(ll, char*);



	*buf++ = '(';

	int ch = 0;
	do {
		ch = va_arg(ll, int);

		if(ch & T_ARRAY)
			*buf++ = '[';

		switch(ch & T_MASK) {
			case T_BYTE:
				*buf++ = 'B';
				break;
			case T_CHAR:
				*buf++ = 'C';
				break;
			case T_DOUBLE:
				*buf++ = 'D';
				break;
			case T_FLOAT:
				*buf++ = 'F';
				break;
			case T_INT:
				*buf++ = 'I';
				break;
			case T_LONG:
				*buf++ = 'J';
				break;
			case T_SHORT:
				*buf++ = 'S';
				break;
			case T_BOOLEAN:
				*buf++ = 'Z';
				break;
			case T_VOID:
				*buf++ = 'V';
				break;
			case T_REFERENCE: {
				*buf++ = 'L';
				
				char* s = va_arg(ll, char*);
				while(*s)
					*buf++ = *s++;

				*buf++ = ';';
			} break;

			case 0:
				break;
			default:
				return NULL;
		}
	} while(ch != 0);

	*buf++ = ')';


	if(rettype & T_ARRAY)
		*buf++ = '[';

	
	switch(rettype & T_MASK) {
		case T_BYTE:
			*buf++ = 'B';
			break;
		case T_CHAR:
			*buf++ = 'C';
			break;
		case T_DOUBLE:
			*buf++ = 'D';
			break;
		case T_FLOAT:
			*buf++ = 'F';
			break;
		case T_INT:
			*buf++ = 'I';
			break;
		case T_LONG:
			*buf++ = 'J';
			break;
		case T_SHORT:
			*buf++ = 'S';
			break;
		case T_BOOLEAN:
			*buf++ = 'Z';
			break;
		case T_VOID:
			*buf++ = 'V';
			break;
		case T_REFERENCE: {
			*buf++ = 'L';
			
			char* s = retref;
			while(*s)
				*buf++ = *s++;

			*buf++ = ';';
		} break;

		default:
			return NULL;
	}


	va_end(ll);
	return strdup(_buf);
}

j_value avm_call(const char* classname, const char* name, int nargs, ...) {
	va_list ll;
	va_start(ll, nargs);


	j_value r = JVALUE_NULL;
	j_value* params = NULL;


	java_method_t* method;
	java_assembly_t* assembly;

	if(
		(java_assembly_find(&assembly, classname) == J_OK) && 
		(java_method_find(&method, classname, name, NULL) == J_OK)
	) {
		if(nargs > 0) {
			params = avm->calloc(sizeof(j_value), nargs);
			ASSERT(params);
			ASSERT(nargs == strlen(method->signature));

			int i;
			for(i = 0; i < nargs; i++)
				params[i] = va_arg(ll, j_value);
		}

		r = java_method_invoke(NULL, assembly, method, params, nargs);

		if(params)
			avm->free(params);
	}

	return r;
}

j_value avm_main(int argc, char** argv) {
	java_method_t* method;
	java_assembly_t* assembly;


	vector_t* v;
	for(v = asm_vector; v; v = v->next) {
		assembly = (java_assembly_t*) v->value;

		if(__entrypoint)
			if(strcmp(__entrypoint, assembly->name) != 0)
				continue;

		if(java_method_find(&method, assembly->name, "main", "([Ljava/lang/String;)V") == J_OK) {
			LOGF("EntryPoint: %s.%s ()", assembly->name, "main");
			
			java_array_t* arr = (java_array_t*) avm->calloc(1, (sizeof(void*) * argc) + sizeof(java_array_t));
			ASSERT(arr);

			arr->magic = JAVA_ARRAY_MAGIC;
			arr->type = T_REFERENCE;
			arr->length = argc;

			memcpy(arr->data, argv, sizeof(char*) * argc);


			j_value param;
			param.ptr = (void*) arr;
			return java_method_invoke(NULL, assembly, method, &param, 1);
		}
	}

	athrow(NULL, "java/lang/NoSuchMethodError", "\"main\" not found");
	return JVALUE_NULL;
}

