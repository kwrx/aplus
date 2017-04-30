
package java.lang;

public abstract class Number {
	public abstract int intValue();
	public abstract long longValue();
	public abstract float floatValue();
	public abstract double doubleValue();


	public byte byteValue() {
		return (byte) 0;
	}

	public short shortValue() {
		return (short) 0;
	}
}
