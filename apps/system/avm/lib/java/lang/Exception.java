
package java.lang;


public class Exception extends Throwable {
	public Exception() {
		super();
	}

	public Exception(String Message) {
		super(Message);
	}

	public Exception(String Message, Throwable Cause) {
		super(Message, Cause);
	}
}
