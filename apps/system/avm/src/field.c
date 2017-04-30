#include <avm.h>
#include "ops.h"


int java_field_find(java_field_t** field, const char* classname, const char* fieldname, const char* signature) {
	java_assembly_t* A = NULL;
	if(java_assembly_find(&A, classname) != J_OK) {
		LOGF("java_field_find() Cannot found Assembly %s", classname);	
		return J_ERR;
	}

	int i;
	for(i = 0; i < A->java_this.jc_fields_count; i++) {
		char* name = (char*) A->java_this.jc_cp[A->java_this.jc_fields[i].name_index].utf8_info.bytes;

		if(strcmp(name, fieldname) == 0) {
			if(likely(signature)) {
				name = (char*) A->java_this.jc_cp[A->java_this.jc_fields[i].desc_index].utf8_info.bytes;
			
				if(strcmp(name, signature) != 0)
					continue;
			}

			if(likely(field))
				*field = &A->java_this.jc_fields[i];

			return J_OK;
		}
	}

	return J_ERR;
}


int java_field_find_for_object(java_field_t** field, java_assembly_t* assembly, const char* classname, const char* fieldname, const char* signature) {
	java_assembly_t* A = NULL;
	for(A = assembly; A; A = A->java_super) {
		if(strcmp(classname, A->name) == 0)
			break;
	}

	if(unlikely(!A)) {
		LOGF("java_field_find() Cannot found Assembly %s", classname);	
		return J_ERR;
	}

	int i;
	for(i = 0; i < A->java_this.jc_fields_count; i++) {
		char* name = (char*) A->java_this.jc_cp[A->java_this.jc_fields[i].name_index].utf8_info.bytes;

		if(strcmp(name, fieldname) == 0) {
			if(likely(signature)) {
				name = (char*) A->java_this.jc_cp[A->java_this.jc_fields[i].desc_index].utf8_info.bytes;
			
				if(strcmp(name, signature) != 0)
					continue;
			}

			if(likely(field))
				*field = &A->java_this.jc_fields[i];

			return J_OK;
		}
	}

	return J_ERR;
}


int java_field_resolve(java_field_t* field, java_assembly_t* assembly) {
	/* Resolve Constant Values*/
	java_attribute_t* a;
	if(java_attribute_find(&a, assembly, field->attributes, field->attr_count, "ConstantValue") == J_OK) {
		u1 tag = assembly->java_this.jc_cp[a->const_value.const_value_index].tag;
	
		switch(tag) {
			case JAVACLASS_TAG_INTEGER:
				field->value.i32 = assembly->java_this.jc_cp[a->const_value.const_value_index].int_info.bytes;
				break;
			case JAVACLASS_TAG_FLOAT:
				field->value.f32 = assembly->java_this.jc_cp[a->const_value.const_value_index].float_info.bytes;
				break;
			case JAVACLASS_TAG_LONG:
				field->value.i64 = assembly->java_this.jc_cp[a->const_value.const_value_index].long_info.bytes;
				break;
			case JAVACLASS_TAG_DOUBLE:
				field->value.f64 = assembly->java_this.jc_cp[a->const_value.const_value_index].double_info.bytes;
				break;
			case JAVACLASS_TAG_STRING:
				field->value.ptr = (void*) assembly->java_this.jc_cp[assembly->java_this.jc_cp[a->const_value.const_value_index].string_info.string_index].utf8_info.bytes;
				break;
			default:
				LOGF("java_field_resolve_value() Invalid TAG! %d", tag);
				return J_ERR;
		}
	}

	field->assembly = assembly;
	field->name = strdup((const char*) assembly->java_this.jc_cp[field->name_index].utf8_info.bytes);
	field->desc = strdup((const char*) assembly->java_this.jc_cp[field->desc_index].utf8_info.bytes);

	return J_OK;
}

