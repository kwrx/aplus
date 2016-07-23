/* A+ Java Virtual Machine */
/* Load a Class File and use a external user-defined native method. */

#include <avm.h>
#include <stdlib.h>
#include <stdio.h>


/* Native.java 							*/
/*								*/
/* public class Native {					*/
/*								*/
/*	// Declare external reference				*/
/*	static native void PrintNative(String S);		*/
/*								*/
/*	public static void PrintHello() {			*/
/*		PrintNative("Hello World\n");			*/
/*	}							*/
/* }								*/



// Declare Native function to use in java */
j_void PrintNative(j_pointer message) {
	printf("%s\n", (const char*) message);
}


int main(int argc, char** argv) {
	
	/* Initialize VM */
	avm_init();

	/* Open Native.class */
	if(avm_open("Native.class") == J_ERR)
		{ perror("Native.class"); abort(); }

	/* Register a native function: void Native.PrintNative(Object Message); */
	java_native_add("Native", "PrintNative", "L", T_VOID, PrintNative);

	/* Initialize context & resolve assemblies */
	avm_begin();

	/* Native.PrintHello(); */
	avm_call("Native", "PrintHello", 0);

	/* Destroy context & all resources */
	avm_end();

}
