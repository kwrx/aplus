

package java.lang;

public class Error extends Throwable {
	public Error() {
		super();
	}

	public Error(String Message) {
		super(Message);
	}

	public Error(String Message, Throwable Cause) {
		super(Message, Cause);
	}
}
