

OPCODE(ldc) {
	uint8_t idx = PC8;
	PC++;

	java_cp_info_t* cp = &j->assembly->java_this.jc_cp[idx];

	switch(cp->tag) {
		case JAVACLASS_TAG_STRING:
			JPUSH(ptr, j->assembly->java_this.jc_cp[cp->string_info.string_index].utf8_info.bytes);
			break;
		case JAVACLASS_TAG_UTF8STRING:
			JPUSH(ptr, cp->utf8_info.bytes);
			break;
		default:
			JPUSH(i32, cp->int_info.bytes);
	}

}

OPCODE(ldc_w) {
	uint16_t idx = PC16;
	PC += 2;

	java_cp_info_t* cp = &j->assembly->java_this.jc_cp[idx];

	switch(cp->tag) {
		case JAVACLASS_TAG_STRING:
			JPUSH(ptr, j->assembly->java_this.jc_cp[cp->string_info.string_index].utf8_info.bytes);
			break;
		case JAVACLASS_TAG_UTF8STRING:
			JPUSH(ptr, cp->utf8_info.bytes);
			break;
		default:
			JPUSH(i32, cp->int_info.bytes);
	}
}

OPCODE(ldc2_w) {
	uint16_t idx = PC16;
	PC += 2;

	JPUSH(i64, j->assembly->java_this.jc_cp[idx].long_info.bytes);
}
