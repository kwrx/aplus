#include <avm.h>
#include "ops.h"
#include "vector.h"


vector_t* asm_vector = NULL;

void java_assembly_init(void) {
	vector_t* v;
	int R;
	do {
		R = 0;
		for(v = asm_vector; v; v = v->next) {
			java_assembly_t* A = (java_assembly_t*) v->value;

			if(!A->resolved) {
				if(java_assembly_resolve(A) != J_OK)
					athrow(NULL, "java/lang/InternalError", strfmt("Cannot resolve %s", A->name));
				

				R++;
				LOGF("Loaded %s", A->name);	
			}
		
		}
	} while(R);
}

void java_assembly_flush(void) {
	vector_t* v;
	for(v = asm_vector; v; v = v->next) {
		java_assembly_t* A = (java_assembly_t*) v->value;

		if(java_assembly_destroy(A, 0) != J_OK)
			LOGF("Cannot destroy %s", A->name);
	}

	vector_destroy(&asm_vector);
}

int java_assembly_find(java_assembly_t** assembly, const char* name) {
	if(unlikely(!name))
		return J_ERR;

	vector_t* v;
	for(v = asm_vector; v; v = v->next) {
		java_assembly_t* A = (java_assembly_t*) v->value;

		if((strcmp(A->name, name) == 0) || (strcmp(A->path, name) == 0)) {
			if(likely(assembly))
				*assembly = A;

			return J_OK;
		}
	}


	return java_library_load(NULL, assembly, name);
}


int java_assembly_load(java_assembly_t** assembly, void* buffer, int size, const char* path) {
	if(java_assembly_find(assembly, path) == J_OK)
		return J_OK;


	java_assembly_t* A = (java_assembly_t*) avm->calloc(1, sizeof(java_assembly_t));
	ASSERT(A);

	A->path = path ? strdup(path) : "Memory";
	A->name = NULL;
	A->java_super = NULL;
	A->resolved = 0;
	A->buffer = buffer;
	
	if(unlikely(java_class_load(A, A->buffer, size) != J_OK)) {
		LOG("java_class_load() failed!");
		return J_ERR;
	}

	if(likely(assembly))
		*assembly = A;

	vector_add(&asm_vector, A);


	if(avm_initialized())
		java_assembly_init();

	return J_OK;
}


int java_assembly_open(java_assembly_t** assembly, const char* filename) {
	if(java_assembly_find(assembly, filename) == J_OK)
		return J_OK;


	int fd = avm->open(filename, O_RDONLY, 0644);
	if(unlikely(fd < 0)) {
		LOGF("Assembly \"%s\" not found!", filename);
		return J_ERR;
	}

	int size = avm->lseek(fd, 0, SEEK_END);
	avm->lseek(fd, 0, SEEK_SET);
	
	void* buffer = (void*) avm->calloc(1, size);
	ASSERT(buffer);

	if(unlikely(avm->read(fd, buffer, size) != size)) {
		LOGF("I/O Read Error in \"%s\"", filename);

		avm->free(buffer);
		return J_ERR;
	}

	avm->close(fd);

	if(unlikely(java_assembly_load(assembly, buffer, size, filename) != J_OK)) {
		LOG("java_assembly_load() failed!");
		
		avm->free(buffer);
		return J_ERR;
	}

	return J_OK;
}

int java_assembly_destroy(java_assembly_t* assembly, int recursive) {
	if(recursive)	
		if(assembly->java_super)
			java_assembly_destroy(assembly->java_super, 1);

	assembly->java_this.jc_cp ? avm->free(assembly->java_this.jc_cp) : (void) 0;
	assembly->java_this.jc_interfaces ? avm->free(assembly->java_this.jc_interfaces) : (void) 0;
	assembly->java_this.jc_fields ? avm->free(assembly->java_this.jc_fields) : (void) 0;
	assembly->java_this.jc_methods ? avm->free(assembly->java_this.jc_methods) : (void) 0;
	assembly->java_this.jc_attributes ? avm->free(assembly->java_this.jc_attributes) : (void) 0;
	assembly->buffer ? avm->free(assembly->buffer) : (void) 0;
	avm->free(assembly);

	return 0;
}


int java_assembly_resolve(java_assembly_t* assembly) {
	if(assembly->resolved)
		return J_OK;	

	int i;
	for(i = 1; i < assembly->java_this.jc_cp_count; i++) {
		if(assembly->java_this.jc_cp[i].tag == JAVACLASS_TAG_CLASS) {
			char* name = (char*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[i].class_info.name_index].utf8_info.bytes;
		
			if(strcmp(assembly->name, name) == 0)
				continue;

			if(name[0] == '[')
				continue;

			if(java_assembly_open(NULL, name) != J_OK) {
				LOGF("Cannot load dependency \"%s\" of %s", name, assembly->name);
				continue;
			}
		}
	}


	if(assembly->java_this.jc_super) {
		char* name = (char*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[assembly->java_this.jc_super].class_info.name_index].utf8_info.bytes;
		if(java_assembly_open(&assembly->java_super, name) != J_OK) {
			LOGF("Cannot load super class \"%s\" of %s", name, assembly->name);
			return J_ERR;
		}
	}

	assembly->resolved = 1;
	return J_OK;
}


int java_assembly_base(java_assembly_t** base, java_assembly_t* A) {
	for(; A->java_super; A = A->java_super)
		if(strcmp(A->java_super->name, "java/lang/Object") == 0)
			break;
	
	if(base)
		*base = A;

	return J_OK;
}

