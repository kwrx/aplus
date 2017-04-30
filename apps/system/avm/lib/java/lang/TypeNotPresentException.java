
package java.lang;


public class TypeNotPresentException extends Exception {
	public TypeNotPresentException() {
		super();
	}

	public TypeNotPresentException(String Message) {
		super(Message);
	}

	public TypeNotPresentException(String Message, Throwable Cause) {
		super(Message, Cause);
	}
}
