/* A+ Java Virtual Machine */
/* Load a Class File and call PrintHello method */

#include <avm.h>
#include <stdlib.h>


/* HelloWorld.java 							*/
/*									*/
/* public class HelloWorld {						*/
/*	public static void PrintHello()	{				*/
/*		System.out.println("Hello World");			*/
/*	}								*/
/* }									*/


int main(int argc, char** argv) {

	/* Initialize VM */
	avm_init();

	/* Open HelloWorld.class */
	if(avm_open("HelloWorld.class") == J_ERR)
		{ perror("HelloWorld.class"); abort(); }


	/* Initialize context & resolve assemblies */
	avm_begin();

	/* HelloWorld.PrintHello(); */
	avm_call("HelloWorld", "PrintHello", 0);

	/* Destroy context & all resources */
	avm_end();

}
