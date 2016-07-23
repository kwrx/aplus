

#define ALOAD(t0, t1, t2)																\
	OPCODE(t0##aload) {																	\
		int32_t idx = JPOP(i32);														\
		t1* a = (t1*) JPOP(ptr);														\
																						\
		if(unlikely(!a))																\
			ATHROW("java/lang/NullPointerException", "Object cannot be null");			\
																						\
		if(unlikely(idx > JAVA_ARRAY(a)->length))										\
			ATHROW("java/lang/ArrayIndexOutOfBoundsException", strfmt("%d", idx));		\
																						\
		JPUSH(t2, a[idx]);																\
	}


ALOAD(i, int32_t, i32)
ALOAD(l, int64_t, i64)
ALOAD(f, float, f32)
ALOAD(d, double, f64)
ALOAD(a, void*, ptr)
ALOAD(b, int8_t, i8)
ALOAD(c, uint16_t, u16)
ALOAD(s, int16_t, i16)


#define ASTORE(t0, t1, t2)																\
	OPCODE(t0##astore) {																\
		t1 v = JPOP(t2);																\
		int32_t idx = JPOP(i32);														\
		t1* a = (t1*) JPOP(ptr);														\
																						\
		if(unlikely(!a))																\
			ATHROW("java/lang/NullPointerException", "Object cannot be null");			\
																						\
		if(unlikely(idx > JAVA_ARRAY(a)->length))										\
			ATHROW("java/lang/ArrayIndexOutOfBoundsException", strfmt("%d", idx));		\
																						\
		a[idx] = v;																		\
	}


ASTORE(i, int32_t, i32)
ASTORE(l, int64_t, i64)
ASTORE(f, float, f32)
ASTORE(d, double, f64)
ASTORE(a, void*, ptr)
ASTORE(b, int8_t, i8)
ASTORE(c, uint16_t, u16)
ASTORE(s, int16_t, i16)





OPCODE(arraylength) {
	void* aref = (void*) JPOP(ptr);

	if(unlikely(!aref))
		ATHROW("java/lang/NullPointerException", "Object cannot be null");

	JPUSH(i32, JAVA_ARRAY(aref)->length);
}

