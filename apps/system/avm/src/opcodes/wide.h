

OPCODE(wide) {
	uint8_t op = PC8;
	PC++;

	switch(op) {
		case 0x15 ... 0x19: {		/* iload, ... */
				int16_t idx = PC16;
				PC += 2;

				JPUSH_JV(j->frame.locals[idx]);
			} 
			break;
		
		case 0x36 ... 0x3A: {		/* istore, ... */
				int16_t idx = PC16;
				PC += 2;

				j->frame.locals[idx] = JPOP_JV();
			}
			break;
		case 0xA9: {				/* ret */
				int16_t idx = PC16;
				PC += 2;

				PC = j->frame.locals[idx].i32;
			}
			break;
		case 0x84: {				/* iinc */
				int16_t idx = PC16;
				PC += 2;
		
				int16_t inc = PC16;
				PC += 2;

				j->frame.locals[idx].i32 += inc;
			}
			break;
	}
}
