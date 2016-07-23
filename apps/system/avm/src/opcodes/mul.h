
OPCODE(imul) {
	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	JPUSH(i32, a * b);
}

OPCODE(lmul) {
	int64_t b = JPOP(i64);
	int64_t a = JPOP(i64);

	JPUSH(i64, a * b);
}

OPCODE(fmul) {
	float b = JPOP(f32);
	float a = JPOP(f32);

	JPUSH(f32, a * b);
}

OPCODE(dmul) {
	double b = JPOP(f64);
	double a = JPOP(f64);

	JPUSH(f64, a * b);
}
