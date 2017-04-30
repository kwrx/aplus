

public class Runtime {
	public static void main(String[] args) {
		String Name = new Runtime().toString();
		AVM.VMDebug.Print(Name);

		try {
			int i = 1 / 0;
		} catch (Exception ex) {
			AVM.VMDebug.Print(ex.toString());
		}
	}
}
