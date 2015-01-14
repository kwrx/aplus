#include "stdio.h"

char* tmpnam(char* buf) {
	static char __intern_buf[L_tmpnam];
	if(unlikely(!buf))
		buf = __intern_buf;

	int i = 0;

	char* p = getenv("TMPDIR");
	if(likely(p))
		strcpy(buf, p);
	else
		strcpy(buf, P_tmpdir);

	i += strlen(buf);
	buf[i++] = PATH_SEPARATOR;
	
	while(i < L_tmpnam - 1)
		buf[i++] = 'a' + (rand() % ('z' - 'a'));

	buf[i] = '\0';
	return buf;
}
