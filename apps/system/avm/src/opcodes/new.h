

OPCODE(new) {
	int16_t idx = PC16;
	PC += 2;

	java_object_t* obj;
	if(java_object_new_from_idx(&obj, j->assembly, idx) != J_OK)
		ATHROW("java/lang/OutOfMemoryError", "");

	JPUSH(ptr, (void*) obj);
}

OPCODE(newarray) {
	int8_t type = PC8;
	PC++;

	int32_t count = JPOP(i32);

	if(unlikely(count < 0))
		ATHROW("java/lang/NegativeArraySizeException", strfmt("%d", count));


	register int sz = 1;
	switch(type) {
		case T_BOOLEAN:
		case T_BYTE:
			sz = sizeof(int8_t);
			break;
		case T_SHORT:
		case T_CHAR:
			sz = sizeof(int16_t);
			break;
		case T_INT:
		case T_FLOAT:
		case T_REFERENCE:
			sz = sizeof(int32_t);
			break;
		case T_LONG:
		case T_DOUBLE:
			sz = sizeof(int64_t);
			break;
		default:
			ATHROW("java/lang/TypeNotPresentException", strfmt("%d", type));
	}

	
	java_array_t* arr = (java_array_t*) avm->calloc(1, (sz * count) + sizeof(java_array_t));
	ASSERT(arr);

	arr->magic = JAVA_ARRAY_MAGIC;
	arr->type = type;
	arr->length = count;
	
	JPUSH(ptr, (void*) arr->data);
}

OPCODE(anewarray) {
	(void) PC16;
	PC += 2;

	int32_t count = JPOP(i32);

	if(unlikely(count < 0))
		ATHROW("java/lang/NegativeArraySizeException", strfmt("%d", count));


	java_array_t* arr = (java_array_t*) avm->calloc(1, (sizeof(void*) * count) + sizeof(java_array_t));
	ASSERT(arr);

	arr->magic = JAVA_ARRAY_MAGIC;
	arr->type = T_REFERENCE;
	arr->length = count;
	
	JPUSH(ptr, (void*) arr->data);
}

OPCODE(multinewarray) {
	(void) PC16;
	PC += 2;

	int8_t rank = PC8;
	PC++;

	int32_t count = 0;
	while(rank--)
		count += JPOP(i32);

	if(unlikely(count < 0))
		ATHROW("java/lang/NegativeArraySizeException", strfmt("%d", count));

	java_array_t* arr = (java_array_t*) avm->calloc(1, (sizeof(void*) * count) + sizeof(java_array_t));
	ASSERT(arr);

	arr->magic = JAVA_ARRAY_MAGIC;
	arr->type = T_REFERENCE;
	arr->length = count;
	
	JPUSH(ptr, (void*) arr->data);
}
