
public class Fib {

	static int fib(int x) {
		int y = 1;
		do {
			y *= x--;
		} while(x > 0);
	
		return y;
	}

	public static void main(String[] args) {
		fib(10);
		fib(10);
		fib(10);
		fib(10);
	}
}


/*
	Result:
		jvm: 0.001151s		(x20 faster)
		gij: 0.024xxxs
*/
