#include <avm.h>
#include "ops.h"


static u8 nextid = 0;
java_object_t* __jobj_queue = NULL;




int java_object_instance(java_assembly_t** instance, java_assembly_t* assembly) {
	if(unlikely(!assembly))
		return J_ERR;

	java_assembly_t* A = (java_assembly_t*) avm->calloc(sizeof(java_assembly_t), 1);
	ASSERT(A);

	memcpy(A, assembly, sizeof(java_assembly_t));

	java_assembly_t* tmp;
	for(tmp = A; tmp; tmp = tmp->java_super) {
		if(tmp->java_this.jc_fields_count == 0)
			continue;

		java_field_t* fields = tmp->java_this.jc_fields;
		tmp->java_this.jc_fields = (java_field_t*) avm->calloc(sizeof(java_field_t), tmp->java_this.jc_fields_count);
		ASSERT(tmp->java_this.jc_fields);
		
		memcpy(tmp->java_this.jc_fields, fields, sizeof(java_field_t) * tmp->java_this.jc_fields_count);
	}


	if(likely(instance))
		*instance = A;

	return J_OK;
}


int java_object_new(java_object_t** obj, const char* name) {
	java_assembly_t* objasm;
	if(java_assembly_find(&objasm, name) != J_OK) {
		LOGF("java_object_new() Assembly %s not found!", name);
		return J_ERR;
	}

	if(unlikely(!obj))
		return J_OK;


	java_object_t* o = (java_object_t*) avm->calloc(1, sizeof(java_object_t));
	ASSERT(o);
	
	
	o->refcount = 1;
	o->name = strdup(name);
	o->id = nextid++;

	avm_mutex_init(&o->lock, AVM_MTX_KIND_ERRORCHECK);
	
	if(java_object_instance(&o->assembly, objasm) != J_OK) {
		LOGF("java_object_new() Instance of %s failed!", objasm->name);
		
		avm->free(o->name);
		avm->free(o);
		return J_ERR;
	}

	o->next = __jobj_queue;
	__jobj_queue = o;

	if(likely(obj))
		*obj = o;

	return J_OK;
}

int java_object_new_from_idx(java_object_t** obj, java_assembly_t* assembly, u2 idx) {
	char* name = (char*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[idx].class_info.name_index].utf8_info.bytes;
	
	return java_object_new(obj, name);
}

void java_object_flush(void) {
	java_object_t* tmp, *tmp2;
	for(tmp = __jobj_queue; tmp;) {
		tmp2 = tmp;
		tmp = tmp->next;


		java_assembly_destroy(tmp2->assembly, 1);

		avm->free(tmp2->name);
		avm->free(tmp2);
	}

	__jobj_queue = NULL;
	nextid = 0LL;
}
