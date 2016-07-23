

class FibR {

	static int fib(int x) {
		return x > 1 ? fib(x - 1) * x : 1;
	}

	public static void main(String[] args) {
		fib(10);
		fib(10);
		fib(10);
		fib(10);
	}
}
