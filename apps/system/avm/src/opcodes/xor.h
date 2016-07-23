

OPCODE(ixor) {
	int32_t b = JPOP(i32);
	int32_t a = JPOP(i32);

	JPUSH(i32, a ^ b);
}

OPCODE(lxor) {
	int64_t b = JPOP(i64);
	int64_t a = JPOP(i64);

	JPUSH(i64, a ^ b);
}
