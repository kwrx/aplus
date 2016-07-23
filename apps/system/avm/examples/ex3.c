/* A+ Java Virtual Machine */
/* Load a Class File and use a external library-defined native method. */

#include <avm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


/* Native.java 							*/
/*								*/
/* public class Native {					*/
/*								*/
/*	// Declare external reference				*/
/*	static native String getenv(String env);		*/
/*								*/
/*	public static String GetPath() {			*/
/*		String EnvPath = getenv("PATH");		*/
/*								*/
/*		return EnvPath;					*/
/*	}							*/
/* }								*/




int main(int argc, char** argv) {

	/* Initialize VM */
	avm_init();

	/* Open Native.class */
	if(avm_open("Native.class") == J_ERR)
		{ perror("Native.class"); abort(); }

	/* Register a native function from "unistd.h" : Object Native.getenv(Object Env); */
	java_native_add("Native", "getenv", "L", T_REFERENCE, getenv);

	/* Initialize context & resolve assemblies */
	avm_begin();

	/* Native.PrintHello(); */
	const char* path = (const char*) avm_call("Native", "GetPath", 0).ptr;

	/* Destroy context & all resources */
	avm_end();

}
