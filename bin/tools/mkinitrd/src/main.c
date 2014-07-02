#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define INITFS_MAGIC		0x2BADC0DE

typedef struct node {
	uint32_t magic;
	char name[256];
	uint32_t size;
	uint32_t offset;
} node_t;

int main(int argc, char** argv) {
	if(argc < 2)
		return -1;

	char* pwd = argv[1];
	argv++;
	argc--;

	chdir(pwd);
		
	node_t* n = malloc(sizeof(node_t) * argc);
	unsigned int offset = sizeof(node_t) * argc + 0x20;
	
	for(int i = 0; i < argc - 1; i++) {
	
		
		FILE* f = fopen(argv[i + 1], "rb");
		if(!f)
			exit(printf("1: File not found \"%s\"\n", argv[i + 1]));
			
		fseek(f, 0, SEEK_END);
		n[i].size = ftell(f);
		fclose(f);
			
		strcpy(n[i].name, argv[i + 1]);
		n[i].offset = offset;
		n[i].magic = INITFS_MAGIC;
		
		offset += n[i].size + 0x20;
	}
	
	FILE* o = fopen("initrd.img", "wb");
	fwrite(n, sizeof(node_t), argc, o);
	fseek(o, sizeof(node_t) * argc + 0x20, SEEK_SET);
	
	
	for(int i = 0; i < argc - 1; i++) {
		
		FILE* f = fopen(argv[i + 1], "rb");
		if(!f)
			exit(printf("2: File not found \"%s\"\n", argv[i + 1]));

			
		void* buf = (void*)malloc(n[i].size);
		fread(buf, n[i].size, 1, f);
		fclose(f);
		
		fseek(o, n[i].offset, SEEK_SET);
		fwrite(buf, n[i].size, 1, o);
	}
	
	fclose(o);
	return 0;
}