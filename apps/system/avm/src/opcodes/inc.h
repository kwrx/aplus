

OPCODE(iinc) {	
	register int8_t idx = PC8;
	PC++;
		
	register int8_t inc = PC8;
	PC++;


	j->frame.locals[idx].i32 += inc;
}
