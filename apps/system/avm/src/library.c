#include <avm.h>
#include "ops.h"


#if !FREESTANDING && CONFIG_JAR
static java_library_t* lb_queue = NULL;
#endif


void java_library_flush(void) {
#if CONFIG_JAR
	java_library_t* lb, *lx;
	for(lb = lb_queue; lb;) {
		lx = lb->next;

		zip_close(lb->fd);

		avm->free((void*) lb->filename);
		avm->free(lb);

		lb = lx;
	}
	lb_queue = NULL;
#endif
}


int java_library_add(java_library_t** lib, const char* filename) {
#if CONFIG_JAR
	int e;
	struct zip* zip = zip_open(filename, 0, &e);
	
	if(unlikely(!zip)) {
		char ebuf[128];
		memset(ebuf, 0, sizeof(ebuf));

		zip_error_to_str(ebuf, sizeof(ebuf), e, errno);

		LOGF("java_library_add() %s: %s", filename, ebuf);	
		return J_ERR;
	}

	java_library_t* lb = avm->calloc(1, sizeof(java_library_t));
	lb->filename = strdup(filename);
	lb->fd = zip;
	lb->next = lb_queue;
	lb_queue = lb;

	if(likely(lib))
		*lib = lb;

	
	LOGF("Loaded library %s", filename);
	return J_OK;
#else
	LOG("WARNING: Java Library Interface disabled");
	return J_ERR;
#endif
}

int java_library_load(java_library_t* lib, java_assembly_t** assembly, const char* filename) {
#if CONFIG_JAR
	if(unlikely(!lib)) {
		java_library_t* lb;
		for(lb = lb_queue; lb; lb = lb->next) {
			if(java_library_load(lb, assembly, filename) == J_OK)
				return J_OK;
		}

		return J_ERR;
	}

	char clname[strlen(filename) + 7];
	memset(clname, 0, sizeof(clname));

	strcat(clname, filename);
	strcat(clname, ".class");

	int idx = zip_name_locate(lib->fd, clname, 0);
	if(idx == -1)
		return J_ERR;



	struct zip_stat sb;
	zip_stat_init(&sb);
	zip_stat_index(lib->fd, idx, 0, &sb);


	struct zip_file* zf = zip_fopen_index(lib->fd, idx, 0);

	if(unlikely(!zf)) {
		LOGF("jar_library_load() %s: %s", filename, zip_strerror(lib->fd));
		return J_ERR;
	}

	void* buffer = (void*) avm->calloc(1, sb.size);
	zip_fread(zf, buffer, sb.size);
	zip_fclose(zf);

	if(java_assembly_load(assembly, buffer, sb.size, sb.name) != J_OK)
		LOGF("Cannot load %s from JAR Library", sb.name);


	return J_OK;
#else
	LOG("WARNING: Java Library Interface disabled");
	return J_ERR;
#endif
}
