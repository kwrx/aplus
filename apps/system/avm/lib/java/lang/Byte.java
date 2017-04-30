
package java.lang;

public final class Byte extends Number {
	public static final byte MAX_VALUE = 127;
	public static final byte MIN_VALUE = -127;
	public static final int SIZE = 8;
	public static final Object TYPE = null;


	protected byte Value;


	public Byte(byte value) {
		this.Value = value;
	}

	public Byte(String s) throws NumberFormatException {
		this.Value = (byte) AVM.VMConv.ToInt(s, -1);
	}

	public static String toString(byte b) {
		return AVM.VMConv.ToString((long) b, 10);
	}

	public static Byte valueOf(byte b) {
		return new Byte(b);
	}
	
	public byte byteValue() {
		return this.Value;
	}

	public static int compare(byte X, byte Y) {
		return (X == Y) ? 1 : 0;
	}

	public int compareTo(byte X) {
		return (this.Value == X) ? 1 : 0;
	}

	public static Byte decode(String s) {
		return (byte) AVM.VMConv.ToInt(s, -1);
	}

	public double doubleValue() {
		return (double) this.Value;
	}

	public boolean equals(Object obj) {
		if(obj instanceof Byte)
			return (this.Value == ((Byte) obj).byteValue());

		return false;
	}

	public float floatValue() {
		return (float) this.Value;
	}

	public int intValue() {
		return (int) this.Value;
	}

	public long longValue() {
		return (long) this.Value;
	}

	public static byte ParseByte(String s, int radix) throws NumberFormatException {
		int v = AVM.VMConv.ToInt(s, radix);
		if(v > Byte.MAX_VALUE || v < Byte.MIN_VALUE)
			throw new NumberFormatException();

		return (byte) v;
	}

	public static Byte ParseByte(String s) throws NumberFormatException {
		return Byte.ParseByte(s, -1);
	}

	
}
