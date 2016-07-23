

OPCODE(iload) {
	register int8_t idx = PC8;
	PC++;

	JPUSH_JV(j->frame.locals[idx]);
}

OPCODE(istore) {
	register int8_t idx = PC8;
	PC++;

	j->frame.locals[idx] = JPOP_JV();
}

#define j_op_lload j_op_iload
#define j_op_fload j_op_iload
#define j_op_dload j_op_iload
#define j_op_aload j_op_iload

#define j_op_lstore j_op_istore
#define j_op_fstore j_op_istore
#define j_op_dstore j_op_istore
#define j_op_astore j_op_istore



OPCODE(iload_0) {
	JPUSH_JV(j->frame.locals[0]);
}

OPCODE(iload_1) {
	JPUSH_JV(j->frame.locals[1]);
}

OPCODE(iload_2) {
	JPUSH_JV(j->frame.locals[2]);
}

OPCODE(iload_3) {
	JPUSH_JV(j->frame.locals[3]);
}

OPCODE(istore_0) {
	j->frame.locals[0] = JPOP_JV();
}

OPCODE(istore_1) {
	j->frame.locals[1] = JPOP_JV();
}

OPCODE(istore_2) {
	j->frame.locals[2] = JPOP_JV();
}

OPCODE(istore_3) {
	j->frame.locals[3] = JPOP_JV();
}


#define j_op_lload_0 j_op_iload_0
#define j_op_lload_1 j_op_iload_1
#define j_op_lload_2 j_op_iload_2
#define j_op_lload_3 j_op_iload_3

#define j_op_fload_0 j_op_iload_0
#define j_op_fload_1 j_op_iload_1
#define j_op_fload_2 j_op_iload_2
#define j_op_fload_3 j_op_iload_3

#define j_op_dload_0 j_op_iload_0
#define j_op_dload_1 j_op_iload_1
#define j_op_dload_2 j_op_iload_2
#define j_op_dload_3 j_op_iload_3

#define j_op_aload_0 j_op_iload_0
#define j_op_aload_1 j_op_iload_1
#define j_op_aload_2 j_op_iload_2
#define j_op_aload_3 j_op_iload_3



#define j_op_lstore_0 j_op_istore_0
#define j_op_lstore_1 j_op_istore_1
#define j_op_lstore_2 j_op_istore_2
#define j_op_lstore_3 j_op_istore_3

#define j_op_fstore_0 j_op_istore_0
#define j_op_fstore_1 j_op_istore_1
#define j_op_fstore_2 j_op_istore_2
#define j_op_fstore_3 j_op_istore_3

#define j_op_dstore_0 j_op_istore_0
#define j_op_dstore_1 j_op_istore_1
#define j_op_dstore_2 j_op_istore_2
#define j_op_dstore_3 j_op_istore_3

#define j_op_astore_0 j_op_istore_0
#define j_op_astore_1 j_op_istore_1
#define j_op_astore_2 j_op_istore_2
#define j_op_astore_3 j_op_istore_3
