
OPCODE(ineg) {
	int32_t a = JPOP(i32);

	JPUSH(i32, -a);
}

OPCODE(lneg) {
	int64_t a = JPOP(i64);

	JPUSH(i64, -a);
}

OPCODE(fneg) {
	float a = JPOP(f32);

	JPUSH(f32, -a);
}

OPCODE(dneg) {
	double a = JPOP(f64);

	JPUSH(f64, -a);
}
