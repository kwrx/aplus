

static int __check_super(java_assembly_t* child, java_assembly_t* parent) {

	if(child->java_super == NULL)
		return -1;

	java_assembly_t* tmp = child->java_super;
	while(tmp) {
		if(strcmp(tmp->name, parent->name) == 0)		
			return 0;		
		
		tmp = tmp->java_super;
	}

	return -1;
}

OPCODE(checkcast) {
	int16_t idx = PC16;
	PC += 2;

	java_object_t* obj = (java_object_t*) JPOP(ptr);
	if(unlikely(!obj)) {
		JPUSH(ptr, NULL);
		return;
	}


#if 0
	java_assembly_t* a;
	if(java_assembly_find(&a, (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[idx].class_info.name_index].utf8_info.bytes) != J_OK)
		ATHROW("java/lang/UnsatisfiedLinkError");
	
	if(!((strcmp(a->name, obj->assembly->name) == 0) || (__check_super(obj->assembly, a) == 0)))
		ATHROW("java/lang/ClassCastException");
#else
	(void) idx;
#endif

	JPUSH(ptr, (void*) obj);
}

OPCODE(instanceof) {
	int16_t idx = PC16;
	PC += 2;

	java_object_t* obj = (java_object_t*) JPOP(ptr);
	if(unlikely(!obj)) {
		JPUSH(i32, 0);
		return;
	}

	const char* aname = (const char*) j->assembly->java_this.jc_cp[j->assembly->java_this.jc_cp[idx].class_info.name_index].utf8_info.bytes;
	java_assembly_t* a;
	if(java_assembly_find(&a, aname) != J_OK)
		ATHROW("java/lang/UnsatisfiedLinkError", aname);

	register int r = ((strcmp(a->name, obj->assembly->name) == 0) || (__check_super(obj->assembly, a) == 0));
	JPUSH(i32, r);
}
