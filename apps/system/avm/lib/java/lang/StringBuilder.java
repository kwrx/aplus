

package java.lang;

public class StringBuilder {

	private char chars[];
	private int position;
	private int size;

	public StringBuilder() {
		chars = new char[1];
		position = 0;
		size = 1;
	}

	public StringBuilder(int capacity) {
		chars = new char[capacity];
		position = 0;
		size = capacity;
	}

	/*public StringBuilder(String S) {
		StringBuilder(S.length);
		appendString(S);
	}


	public StringBuilder append(boolean b) {
		if(b)
			return appendString("true");
		
		return appendString("false");
	}

	public StringBuilder append(char c) {
		
	}
	*/
}
