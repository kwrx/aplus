/* A+ Java Virtual Machine */
/* Load a Class File and return sum of two parameters from method */

#include <avm.h>
#include <stdlib.h>


/* Calc.java 							*/
/*								*/
/* public class Calc {						*/
/*	public static int Sum(int a, int b) {			*/
/*		return a + b;					*/
/*	}							*/
/* }								*/


int main(int argc, char** argv) {

	/* Initialize VM */
	avm_init();

	/* Open Calc.class */
	if(avm_open("Calc.class") == J_ERR)
		{ perror("Calc.class"); abort(); }


	/* Initialize context & resolve assemblies */
	avm_begin();


	j_value a, b;
	
	a.i32 = 10;
	b.i32 = 15;

	/* c = Calc.Sum(a, b); */
	j_value c = avm_call("Calc", "Sum", 2, a, b);

	/* Destroy context & all resources */
	avm_end();

}
