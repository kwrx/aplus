

OPCODE(monitorenter) {
	java_object_t* obj = (java_object_t*) JPOP(ptr);

	if(unlikely(!obj))
		ATHROW("java/lang/NullPointerException", "Object cannot be null");


	/* FIXME */
	//if(unlikely(
		avm_mutex_lock(&obj->lock)
	// != J_OK))
	//	ATHROW("java/lang/IllegalMonitorStateException", "DeadLock")
	;
}

OPCODE(monitorexit) {
	java_object_t* obj = (java_object_t*) JPOP(ptr);

	if(unlikely(!obj))
		ATHROW("java/lang/NullPointerException", "Object cannot be null");
	

	/* FIXME */
	// if(unlikely(
	avm_mutex_unlock(&obj->lock)
	// != J_OK))
	//	ATHROW("java/lang/IllegalMonitorStateException", "Mutex cannot be unlocked")
	;
}
