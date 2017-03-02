#include <aplus.h>
#include <aplus/mm.h>
#include <libc.h>

char** args_dup(char** args) {
	int len = 0;
	char** ret = NULL;
	
	if(unlikely(!args)) {
		ret = (char**) kmalloc(sizeof(char**), GFP_USER);
		ret[0] = NULL;
		
		return ret;	
	}
	
	while(args[len])
		len++;

	ret = (char**) kmalloc(sizeof(char**) * (len + 1), GFP_USER);
	
	int i;
	for(i = 0; i < len; i++)
		ret[i] = strdup(args[i]);
		
	ret[len] = NULL;
	return ret;
}