

OPCODE(pop) {
	(void) JPOP_JV();
}

OPCODE(pop2) {
	(void) JPOP_JV();
	(void) JPOP_JV();
}

OPCODE(dup) {
	j_value jv = JPOP_JV();
	
	JPUSH_JV(jv);
	JPUSH_JV(jv);
}

OPCODE(dup_x1) {
	j_value jv1 = JPOP_JV();
	j_value jv2 = JPOP_JV();

	JPUSH_JV(jv1);
	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
}

OPCODE(dup_x2) {
	j_value jv1 = JPOP_JV();
	j_value jv2 = JPOP_JV();
	j_value jv3 = JPOP_JV();
	

	JPUSH_JV(jv1);
	JPUSH_JV(jv3);
	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
}

OPCODE(dup2) {
	j_value jv1 = JPOP_JV();
	j_value jv2 = JPOP_JV();

	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
}

OPCODE(dup2_x1) {
	j_value jv1 = JPOP_JV();
	j_value jv2 = JPOP_JV();
	j_value jv3 = JPOP_JV();

	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
	JPUSH_JV(jv3);
	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
}

OPCODE(dup2_x2) {
	j_value jv1 = JPOP_JV();
	j_value jv2 = JPOP_JV();
	j_value jv3 = JPOP_JV();
	j_value jv4 = JPOP_JV();

	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
	JPUSH_JV(jv4);
	JPUSH_JV(jv3);
	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
}

OPCODE(swap) {
	j_value jv1 = JPOP_JV();
	j_value jv2 = JPOP_JV();

	JPUSH_JV(jv2);
	JPUSH_JV(jv1);
}
