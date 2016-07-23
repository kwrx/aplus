
OPCODE(idiv) {
	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	if(unlikely(b == 0))
		ATHROW("java/lang/ArithmeticException", "Division by zero");

	JPUSH(i32, a / b);
}

OPCODE(ldiv) {
	int64_t b = JPOP(i64);
	int64_t a = JPOP(i64);

	if(unlikely(b == 0))
		ATHROW("java/lang/ArithmeticException", "Division by zero");

	JPUSH(i64, a / b);
}

OPCODE(fdiv) {
	float b = JPOP(f32);
	float a = JPOP(f32);

	JPUSH(f32, a / b);
}

OPCODE(ddiv) {
	double b = JPOP(f64);
	double a = JPOP(f64);

	JPUSH(f64, a / b);
}
