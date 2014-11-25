#include <stdio.h>

__attribute__((dllexport)) int shared_func() {
	printf("Hello World");
}
