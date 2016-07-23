#include <avm.h>
#include "ops.h"
#include "vector.h"


	
#define CP_EMPTY	(java_cp_info_t*) avm->calloc(1, sizeof(java_cp_info_t))


static struct {
	int tag;
	int length;
	char* name;
} __cp_info[] = {
	{ 0, 0, NULL },
	{ 1, 2, "UTF-8 String" },
	{ 2, 0, "Unknown" },
	{ 3, 4, "Integer" },
	{ 4, 4, "Float" },
	{ 5, 8, "Long" },
	{ 6, 8, "Double" },
	{ 7, 2, "Class Reference" },
	{ 8, 2, "String Reference" },
	{ 9, 4, "Field Reference" },
	{ 10, 4, "Method Reference" },
	{ 11, 4, "Interface Method Reference" },
	{ 12, 4, "TypeName Descriptor" }
};




static float __decode_float(u4 bytes) {
	float r = 0.0f;
	switch(bytes) {
		case 0x7F800000:
			r = INFINITY;
			break;
		case 0xFF800000:
			r = -INFINITY;
			break;
		case 0x7F800001 ... 0x7FFFFFFF:
		case 0xFF800001 ... 0xFFFFFFFF:
			r = NAN;
			break;
		default: {
			int s = ((bytes >> 31) == 0) ? 1 : -1;
			int e = ((bytes >> 23) & 0xFF);
			int m = (e == 0)
						? (bytes & 0x7FFFFF) << 1
						: (bytes & 0x7FFFFF) | 0x800000;

			r = s * m * 2e-150L;
		}
	}

	return r;
}

static double __decode_double(u8 bytes) {
	double r = 0.0;
	switch(bytes) {
		case 0x7FF0000000000000L:
			r = INFINITY;
			break;
		case 0xFFF0000000000000L:
			r = -INFINITY;
			break;
		case 0x7FF0000000000001L ... 0x7FFFFFFFFFFFFFFFL:
		case 0xFFF0000000000001L ... 0xFFFFFFFFFFFFFFFFL:
			r = NAN;
			break;
		default: {
			int s = ((bytes >> 63) == 0) ? 1 : -1;
			int e = ((bytes >> 52) & 0x7FFL);
			int m = (e == 0)
						? (bytes & 0xFFFFFFFFFFFFFL) << 1
						: (bytes & 0xFFFFFFFFFFFFFL) | 0x10000000000000L;

			r = s * m * 2e-1075L;
		}
	}

	return r;
}

	
int java_class_load(java_assembly_t* assembly, void* buffer, int size) {
	//LOGF("%s: java_class_parse(%p, %d)", assembly->path, buffer, size);
	
	vector_t* vector = NULL;
	java_class_t* jc = &assembly->java_this;
	
	R32(jc->jc_magic);
	
	if(unlikely(jc->jc_magic != JAVA_MAGIC)) {
		LOGF("%s: Invalid magic number! %X", assembly->path, jc->jc_magic);
		return J_ERR;
	}
	
	R16(jc->jc_version.minor);
	R16(jc->jc_version.major);
	R16(jc->jc_cp_count);
	
	//LOGF("%s: Version %d.%d", assembly->path, (int) jc->jc_version.major, (int) jc->jc_version.minor);
	//LOGF("%s: Constant Pool entries %d", assembly->path, (int) jc->jc_cp_count);



	vector_add(&vector, CP_EMPTY);
	
	int i, s;
	for(i = 0; i < jc->jc_cp_count - 1; i++) {
		java_cp_info_t* cp = CP_EMPTY;
		ASSERT(cp);
		
		R8(cp->tag);
		switch(__cp_info[cp->tag].length) {
			case 0:
				break;
			case 1:
				R8(*(u1*) cp->info);
				break;
			case 2:
				R16(*(u2*) cp->info);
				break;
			case 4:
				R32(*(u4*) cp->info);
				break;
			case 8:
				R64(*(u8*) cp->info);
				break;
			default:
				LOGF("%s: Invalid Constant Pool size! %d", assembly->path, __cp_info[cp->tag].length);
				ASSERT(0);
		}
		
		


		switch(cp->tag) {
			case JAVACLASS_TAG_FLOAT:
				cp->float_info.bytes = __decode_float(*(u4*) cp->info);
				break;
			case JAVACLASS_TAG_DOUBLE:
				cp->double_info.bytes = __decode_double(*(u8*) cp->info);
				break;
			case JAVACLASS_TAG_UTF8STRING:
				cp->utf8_info.bytes = (u1*) avm->calloc(1, cp->utf8_info.length + 1);
				ASSERT(cp->utf8_info.bytes);

				RXX(cp->utf8_info.bytes, cp->utf8_info.length);
				break;
		}

		vector_add(&vector, cp);
		


		switch(cp->tag) {
			case JAVACLASS_TAG_LONG:
			case JAVACLASS_TAG_DOUBLE:
				vector_add(&vector, CP_EMPTY);
				i++;
				break;
			default:
				continue;
		}
	}
	
	jc->jc_cp = (java_cp_info_t*) vector_to_array(&vector, jc->jc_cp_count, sizeof(java_cp_info_t));
	


	R16(jc->jc_flags);
	R16(jc->jc_this);
	R16(jc->jc_super);

	assembly->name = strdup((char*) jc->jc_cp[jc->jc_cp[jc->jc_this].class_info.name_index].utf8_info.bytes);
	//LOGF("%s: Resolved name of this(%d) \"%s\"", assembly->path, jc->jc_this, assembly->name);	


	R16(jc->jc_interfaces_count);
	//LOGF("%s: Interfaces entries %d", assembly->path, (int) jc->jc_interfaces_count);
	
	if(likely(jc->jc_interfaces_count)) {
		jc->jc_interfaces = (u2*) avm->calloc(sizeof(u2), jc->jc_interfaces_count);
		ASSERT(jc->jc_interfaces);

		for(i = 0; i < jc->jc_interfaces_count; i++)
			R16(jc->jc_interfaces[i]);
	}


	R16(jc->jc_fields_count);
	//LOGF("%s: Fields entries %d", assembly->path, (int) jc->jc_fields_count);

	if(likely(jc->jc_fields_count)) {
		jc->jc_fields = (java_field_t*) avm->calloc(sizeof(java_field_t), jc->jc_fields_count);
		ASSERT(jc->jc_fields);

		for(i = 0; i < jc->jc_fields_count; i++) {
			R16(jc->jc_fields[i].flags);
			R16(jc->jc_fields[i].name_index);
			R16(jc->jc_fields[i].desc_index);
			R16(jc->jc_fields[i].attr_count);


			s = java_attribute_load(assembly, buffer, &jc->jc_fields[i].attributes, jc->jc_fields[i].attr_count);
			RXX(NULL, s);

			
			java_field_resolve(&jc->jc_fields[i], assembly);
		}
	}

	R16(jc->jc_methods_count);
	//LOGF("%s: Methods entries %d", assembly->path, (int) jc->jc_methods_count);

	if(likely(jc->jc_methods_count)) {
		jc->jc_methods = (java_method_t*) avm->calloc(sizeof(java_method_t), jc->jc_methods_count);
		ASSERT(jc->jc_methods);

		for(i = 0; i < jc->jc_methods_count; i++) {
			R16(jc->jc_methods[i].flags);
			R16(jc->jc_methods[i].name_index);
			R16(jc->jc_methods[i].desc_index);
			R16(jc->jc_methods[i].attr_count);

			s = java_attribute_load(assembly, buffer, &jc->jc_methods[i].attributes, jc->jc_methods[i].attr_count);
			RXX(NULL, s);

			/* Resolve */
			java_method_resolve(&jc->jc_methods[i], assembly);
		}	
	}

	R16(jc->jc_attributes_count);
	//LOGF("%s: Attributes entries %d", assembly->path, (int) jc->jc_attributes_count);

	s = java_attribute_load(assembly, buffer, &jc->jc_attributes, jc->jc_attributes_count);
	RXX(NULL, s);


	return J_OK;
}


