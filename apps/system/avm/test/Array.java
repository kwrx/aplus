public class Array {


	static Object CreateArray(int size) {
		return (Object) new byte[size];
	}

	public static void main(String[] args) {
		byte[] arr = (byte[]) CreateArray(10);

		for(int i = 0; i < 10; i++)
			arr[i] = (byte) i;
		

		if(arr[5] != (byte)5)
			arr[50] = (byte) 100;
		arr[12] = (byte)10;
	}
}
