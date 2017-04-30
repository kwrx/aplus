
package java.lang;


public class OutOfMemoryError extends Error {
	public OutOfMemoryError() {
		super();
	}

	public OutOfMemoryError(String Message) {
		super(Message);
	}

	public OutOfMemoryError(String Message, Throwable Cause) {
		super(Message, Cause);
	}
}
