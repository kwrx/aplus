#include <stdio.h>

int shared_func() {
	int x = 100;
	x += 100;
	return x;
}

int main(int argc, char** argv) {
	printf("Hello World");
	return shared_func();
}
