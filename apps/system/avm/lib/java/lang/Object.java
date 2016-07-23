

package java.lang;

public class Object {

	public int hashCode() {
		return AVM.VMClass.hashCode(this);
	}

	public boolean equals(Object o) {
		return (this == o);
	}

	public String toString() {
		return AVM.VMClass.getName(this);
	}


	
}
