

OPCODE(i2l) {
	int32_t i = JPOP(i32);
	JPUSH(i64, (int64_t) i);
}

OPCODE(i2f) {
	int32_t i = JPOP(i32);
	JPUSH(f32, (float) i);
}

OPCODE(i2d) {
	int32_t i = JPOP(i32);
	JPUSH(f64, (double) i);
}

OPCODE(l2i) {
	int64_t i = JPOP(i64);
	JPUSH(i32, (int32_t) (i & 0xFFFFFFFF));
}

OPCODE(l2f) {
	int64_t i = JPOP(i64);
	JPUSH(f32, (float) i);
}

OPCODE(l2d) {
	int64_t i = JPOP(i64);
	JPUSH(f64, (double) i);
}

OPCODE(f2i) {
	float i = JPOP(f32);
	JPUSH(i32, (int32_t) i);
}

OPCODE(f2l) {
	float i = JPOP(f32);
	JPUSH(i64, (int64_t) i);
}

OPCODE(f2d) {
	float i = JPOP(f32);
	JPUSH(f64, (double) i);
}

OPCODE(d2i) {
	double i = JPOP(f64);
	JPUSH(i32, (int32_t) i);
}

OPCODE(d2l) {
	double i = JPOP(f64);
	JPUSH(i64, (int64_t) i);
}

OPCODE(d2f) {
	double i = JPOP(f64);
	JPUSH(f32, (float) i);
}

OPCODE(i2b) {
	int32_t i = JPOP(i32);
	JPUSH(i8, (uint8_t) (i & 0xFF));
}

OPCODE(i2c) {
	int32_t i = JPOP(i32);
	JPUSH(u16, (uint16_t) (i & 0xFFFF));
}

OPCODE(i2s) {
	int32_t i = JPOP(i32);
	JPUSH(i16, (int16_t) (i & 0xFFFF));
}

