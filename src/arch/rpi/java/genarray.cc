

#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv) {
	if(argc < 2)
		return 1;

	FILE* fp = fopen(argv[1], "rb");
	
	char buf[8];
	int r, i;


	printf("char data[] = {\n");

	while((r = fread(buf, 1, 8, fp))) {
		printf("\t");
		for(i = 0; i < r; i++)
			printf("0x%x, ", buf[i] & 0xFF);

		printf("\n");
	}

	printf("};\n");
}
