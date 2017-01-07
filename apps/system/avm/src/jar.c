#include <avm.h>
#include "ops.h"


int jar_open(const char* filename) {
#if CONFIG_JAR

	int e;
	struct zip* zip = zip_open(filename, 0, &e);

	
	if(!zip) {
		char ebuf[128];
		memset(ebuf, 0, sizeof(ebuf));

		zip_error_to_str(ebuf, sizeof(ebuf), e, errno);

		LOGF("jar_open() %s: %s", filename, ebuf);	
		return J_ERR;
	}
	

	int i, j = zip_get_num_files(zip);
	for(i = 0; i < j; i++) {

		struct zip_stat sb;
		zip_stat_init(&sb);
		zip_stat_index(zip, i, 0, &sb);
		
		if(sb.name[strlen(sb.name) - 1] == '/')
			continue;

		if(strcmp(sb.name, "META-INF/MANIFEST.MF") == 0)
			continue;


		struct zip_file* zf = zip_fopen_index(zip, i, 0);

		if(unlikely(!zf)) {
			LOGF("jar_open() %s: %s", filename, zip_strerror(zip));
			return J_ERR;
		}

		void* buffer = (void*) avm->calloc(1, sb.size);
		zip_fread(zf, buffer, sb.size);
		zip_fclose(zf);

		if(java_assembly_load(NULL, buffer, sb.size, sb.name) != J_OK)
			LOGF("Cannot load %s from JAR Archive", sb.name);
	}


	zip_close(zip);
	return J_OK;
#else
	LOG("WARNING: Java Archive Interface disabled");
	return J_ERR;
#endif
}





