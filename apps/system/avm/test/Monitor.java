

public class Monitor {
	public static void main(String[] args) {
		Object O = new Object();

		AVM.VMDebug.Print("## Enter ##");
		synchronized (O) {
			synchronized (O) {
				AVM.VMDebug.Print("## Critical Region ##");
			}
		}
		AVM.VMDebug.Print("## Exit ##");
	}
}
