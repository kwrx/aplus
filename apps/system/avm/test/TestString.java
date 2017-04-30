

public class TestString {
	public static void main(String args[]) {
		String S = "Hello World";
		AVM.VMDebug.Print(S.intern());
	}
}
