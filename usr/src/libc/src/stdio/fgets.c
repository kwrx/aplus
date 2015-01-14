#include "stdio.h"

char* fgets(char* buf, int num, FILE* fp) {
	int i;
	for(i = 0; i < num; i++) {
		int ch = fgetc(fp);
		switch(ch) {
			case EOF:
				break;
			case '\n':
				buf[i++] = (char) ch;
				break;
		}

		buf[i] = (char) ch;
	}

	buf[i] = '\0';
	return buf;
}
