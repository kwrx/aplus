

package java.lang;

public class Throwable {

	public String Message;
	public Throwable InnerException;
	
	public Throwable() {
	
	}

	public Throwable(String Message) {
		this.Message = Message;
	}

	public Throwable(String Message, Throwable Cause) {
		this.Message = Message;
		this.InnerException = Cause;
	}

}
