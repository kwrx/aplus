

OPCODE(getstatic) {
	int16_t idx = PC16;
	PC += 2;

	u2 cidx = j->assembly->java_this.jc_cp[idx].field_ref.class_index;
	u2 nidx = j->assembly->java_this.jc_cp[idx].field_ref.name_and_type_index;


	char* name = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.name_index].utf8_info.bytes;
	char* desc = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.desc_index].utf8_info.bytes;
	const char* classname = (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[cidx].class_info.name_index].utf8_info.bytes;


	java_field_t* field = NULL;
	java_field_find(&field, classname, name, desc);

	JPUSH_JV(field->value);
}

OPCODE(putstatic) {
	int16_t idx = PC16;
	PC += 2;

	u2 cidx = j->assembly->java_this.jc_cp[idx].field_ref.class_index;
	u2 nidx = j->assembly->java_this.jc_cp[idx].field_ref.name_and_type_index;


	char* name = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.name_index].utf8_info.bytes;
	char* desc = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.desc_index].utf8_info.bytes;
	const char* classname = (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[cidx].class_info.name_index].utf8_info.bytes;


	java_field_t* field = NULL;
	java_field_find(&field, classname, name, desc);


	field->value = JPOP_JV();
}


OPCODE(getfield) {
	int16_t idx = PC16;
	PC += 2;

	java_object_t* obj = (java_object_t*) JPOP(ptr);

	if(unlikely(!obj))
		ATHROW("java/lang/NullPointerException", "Object cannot be null");

	
	u2 cidx = j->assembly->java_this.jc_cp[idx].field_ref.class_index;
	u2 nidx = j->assembly->java_this.jc_cp[idx].field_ref.name_and_type_index;


	char* name = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.name_index].utf8_info.bytes;
	char* desc = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.desc_index].utf8_info.bytes;
	const char* classname = (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[cidx].class_info.name_index].utf8_info.bytes;


	java_field_t* field = NULL;
	java_field_find_for_object(&field, obj->assembly, classname, name, desc);

	JPUSH_JV(field->value);
}

OPCODE(putfield) {
	int16_t idx = PC16;
	PC += 2;

	j_value jval = JPOP_JV();
	java_object_t* obj = (java_object_t*) JPOP(ptr);


	if(unlikely(!obj))
		ATHROW("java/lang/NullPointerException", "Object cannot be null");

	u2 cidx = j->assembly->java_this.jc_cp[idx].field_ref.class_index;
	u2 nidx = j->assembly->java_this.jc_cp[idx].field_ref.name_and_type_index;


	char* name = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.name_index].utf8_info.bytes;
	char* desc = (char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[nidx].name_and_type_info.desc_index].utf8_info.bytes;
	const char* classname = (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[cidx].class_info.name_index].utf8_info.bytes;


	java_field_t* field = NULL;
	java_field_find_for_object(&field, obj->assembly, classname, name, desc);

	
	field->value = jval;
}
