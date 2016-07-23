

OPCODE(goto) {
	int16_t off = PC16;
	PC += 2;

	JGOTO(off);
}

OPCODE(goto_w) {
	int32_t off = PC32;
	PC += 4;

	JGOTO(off);
}

OPCODE(jsr) {
	int16_t off = PC16;
	PC += 2;

	JPUSH(i32, PC);
	JGOTO(off);
}

OPCODE(jsr_w) {
	int32_t off = PC32;
	PC += 4;

	JPUSH(i32, PC);
	JGOTO(off);
}

OPCODE(ret) {
	int8_t idx = PC8;
	PC++;

	PC = j->frame.locals[idx].i32;
}

OPCODE(tableswitch) {
	PC += 3;	/* Padding */
	
	int32_t def = PC32;
	PC += 4;

	int32_t low = PC32;
	PC += 4;

	int32_t high = PC32;
	PC += 4;

	int32_t idx = JPOP(i32);

	if(idx < low || idx > high) {
		JGOTO(def);
	} else {
		PC += ((idx - low) * 4);
		JGOTO(PC32);
	}
}


OPCODE(lookupswitch) {
	PC += 3;	/* Padding */
	
	int32_t def = PC32;
	PC += 4;

	int32_t npairs = PC32;
	PC += 4;

	int32_t R0 = JPOP(i32);

	int32_t i = 0;
	for(i = 0; i < npairs; i++) {
		int32_t R1 = PC32;
		PC += 4;

		if(R0 == R1) {
			JGOTO(PC32);
			return;
		}
		
		PC += 4;
	}

	JGOTO(def);
}


OPCODE(ireturn) {
	j->frame.retval.i32 = JPOP(i32);
	JRETURN;
}

OPCODE(lreturn) {
	j->frame.retval.i64 = JPOP(i64);
	JRETURN;
}

OPCODE(freturn) {
	j->frame.retval.f32 = JPOP(f32);
	JRETURN;
}

OPCODE(dreturn) {
	j->frame.retval.f64 = JPOP(f64);
	JRETURN;
}

OPCODE(areturn) {
	j->frame.retval.ptr = JPOP(ptr);
	JRETURN;
}

OPCODE(return) {
	JRETURN;
}




