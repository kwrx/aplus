

OPCODE(bipush) {
	JPUSH(i8, PC8);
	PC++;
}

OPCODE(sipush) {
	JPUSH(i16, PC16);
	PC += 2;
}

