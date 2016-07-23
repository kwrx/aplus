


public class TryCatch {
	public static void Test3() throws Exception {
		throw new Exception("Hello World");
	}

	public static void Test2() {
		try {
			Test3();	
		} catch(Exception e) {
			return;
		}
	}

	public static void Test1() {
		Test2();
	}

	public static void main(String[] args) {
		Test1();
	}
}
