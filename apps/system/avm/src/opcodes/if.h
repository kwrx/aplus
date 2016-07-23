

OPCODE(ifeq) {
	int16_t off = PC16;
	PC += 2;

	int32_t v = JPOP(i32);

	if(likely(!v))
		JGOTO(off);
}

OPCODE(ifne) {
	int16_t off = PC16;
	PC += 2;

	int32_t v = JPOP(i32);

	if(likely(v))
		JGOTO(off);
}

OPCODE(iflt) {
	int16_t off = PC16;
	PC += 2;

	int32_t v = JPOP(i32);

	if(likely(v < 0))
		JGOTO(off);
}

OPCODE(ifge) {
	int16_t off = PC16;
	PC += 2;

	int32_t v = JPOP(i32);

	if(likely(v >= 0))
		JGOTO(off);
}

OPCODE(ifgt) {
	int16_t off = PC16;
	PC += 2;

	int32_t v = JPOP(i32);

	if(likely(v > 0))
		JGOTO(off);
}

OPCODE(ifle) {
	int16_t off = PC16;
	PC += 2;

	int32_t v = JPOP(i32);

	if(likely(v <= 0))
		JGOTO(off);
}

OPCODE(if_icmpeq) {
	int16_t off = PC16;
	PC += 2;

	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(likely(a == b))
		JGOTO(off);
}

OPCODE(if_icmpne) {
	int16_t off = PC16;
	PC += 2;

	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(likely(a != b))
		JGOTO(off);
}

OPCODE(if_icmplt) {
	int16_t off = PC16;
	PC += 2;

	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(likely(a < b))
		JGOTO(off);
}

OPCODE(if_icmpge) {
	int16_t off = PC16;
	PC += 2;

	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(likely(a >= b))
		JGOTO(off);
}

OPCODE(if_icmpgt) {
	int16_t off = PC16;
	PC += 2;

	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(likely(a > b))
		JGOTO(off);
}

OPCODE(if_icmple) {
	int16_t off = PC16;
	PC += 2;

	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(likely(a <= b))
		JGOTO(off);
}

OPCODE(if_acmpeq) {
	int16_t off = PC16;
	PC += 2;

	void* b = JPOP(ptr);
	void* a = JPOP(ptr);

	if(likely(a == b))
		JGOTO(off);
}

OPCODE(if_acmpne) {
	int16_t off = PC16;
	PC += 2;

	void* b = JPOP(ptr);
	void* a = JPOP(ptr);

	if(likely(a != b))
		JGOTO(off);
}

OPCODE(ifnull) {
	int16_t off = PC16;
	PC += 2;

	void* a = JPOP(ptr);

	if(likely(!a))
		JGOTO(off);
}

OPCODE(ifnonnull) {
	int16_t off = PC16;
	PC += 2;

	void* a = JPOP(ptr);

	if(likely(a))
		JGOTO(off);
}
