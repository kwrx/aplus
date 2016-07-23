

OPCODE(aconst_null) {
	JPUSH(ptr, NULL);
}

OPCODE(iconst_m1) {
	JPUSH(i32, -1);
}

OPCODE(iconst_0) {
	JPUSH(i32, 0);
}

OPCODE(iconst_1) {
	JPUSH(i32, 1);
}

OPCODE(iconst_2) {
	JPUSH(i32, 2);
}

OPCODE(iconst_3) {
	JPUSH(i32, 3);
}

OPCODE(iconst_4) {
	JPUSH(i32, 4);
}

OPCODE(iconst_5) {
	JPUSH(i32, 5);
}

OPCODE(lconst_0) {
	JPUSH(i64, 0L);
}

OPCODE(lconst_1) {
	JPUSH(i64, 1L);
}

OPCODE(fconst_0) {
	JPUSH(f32, 0.0f);
}

OPCODE(fconst_1) {
	JPUSH(f32, 1.0f);
}

OPCODE(fconst_2) {
	JPUSH(f32, 2.0f);
}

OPCODE(dconst_0) {
	JPUSH(f64, 0.0);
}

OPCODE(dconst_1) {
	JPUSH(f64, 1.0);
}
