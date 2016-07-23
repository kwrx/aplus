#ifndef _AVM_H
#define _AVM_H


#ifndef NULL
#define NULL			((void*) 0)
#endif

#define J_OK			(0)
#define J_ERR			(-1)


#ifdef __GNUC__
#ifndef __packed
#define __packed		__attribute__ ((packed))
#endif

#ifndef __weak
#define __weak			__attribute__ ((weak))
#endif

#define likely(x)		__builtin_expect(!!(x), 1)
#define unlikely(x)		__builtin_expect(!!(x), 0)
#else
#ifndef __packed
#define __packed
#endif

#ifndef __weak
#define __weak
#endif
#define likely(x)		(x)
#define unlikely(x)		(x)
#endif


#ifdef __GNUC__
#define __SWAP_16(x)	__builtin_bswap16((x))
#define __SWAP_32(x)	__builtin_bswap32((x))
#define __SWAP_64(x)	__builtin_bswap64((x))
#else
#error "Define your swap functions here"
#endif

#define SWAP(x, y)	\
	__SWAP_##y ((x))


typedef char j_byte;
typedef short j_short;
typedef int j_int;
typedef long long j_long;
typedef unsigned short j_char;
typedef float j_float;
typedef double j_double;
typedef int j_bool;
typedef void* j_pointer;
typedef void j_void;


typedef unsigned char u1;
typedef unsigned short u2;
typedef unsigned int u4;
typedef unsigned long long u8;


typedef union {
	j_byte i8;
	j_short i16;
	j_char u16;
	j_int i32;
	j_float f32;
	j_pointer ptr;
	j_long i64;
	j_double f64;
	j_bool b;
} j_value;


#define JVALUE_NULL		(j_value) 0LL






#define JAVA_MAGIC			0xCAFEBABE

#define ACC_PUBLIC			0x0001
#define ACC_PRIVATE			0x0002
#define ACC_PROTECTED		0x0004
#define ACC_STATIC			0x0008
#define ACC_FINAL			0x0010
#define ACC_SUPER			0x0020
#define ACC_SYNCHRONIZED	0x0020
#define ACC_VOLATILE		0x0040
#define ACC_BRIDGE			0x0040
#define ACC_TRANSIENT		0x0080
#define ACC_VARARGS			0x0080
#define ACC_NATIVE			0x0100
#define ACC_INTERFACE		0x0200
#define ACC_ABSTRACT		0x0400
#define ACC_STRICT			0x0800
#define ACC_SYNTHETIC		0x1000
#define ACC_ANNOTATION		0x2000
#define ACC_ENUM			0x4000


#define JAVACLASS_TAG_UTF8STRING 			1
#define JAVACLASS_TAG_UNKNOWN				2
#define JAVACLASS_TAG_INTEGER				3
#define JAVACLASS_TAG_FLOAT					4
#define JAVACLASS_TAG_LONG					5
#define JAVACLASS_TAG_DOUBLE				6
#define JAVACLASS_TAG_CLASS					7
#define JAVACLASS_TAG_STRING				8
#define JAVACLASS_TAG_FIELD					9
#define JAVACLASS_TAG_METHOD				10
#define JAVACLASS_TAG_INTERFACE				11
#define JAVACLASS_TAG_TYPENAME				12


#define T_BOOLEAN				0x0004
#define T_CHAR					0x0005
#define T_FLOAT					0x0006
#define T_DOUBLE				0x0007
#define T_BYTE					0x0008
#define T_SHORT					0x0009
#define T_INT					0x000A
#define T_LONG					0x000B
#define T_VOID					0x000C
#define T_REFERENCE				0X000D
#define T_ARRAY					0xF000
#define T_MASK					0x0FFF

#define JAVACTX_FLAG_CONTINUE		(0)
#define JAVACTX_FLAG_RETURN			(1)
#define JAVACTX_FLAG_EXCEPTION		(2)
#define JAVACTX_FLAG_SUCCESS		(4)


#define JAVA_ARRAY(x)			((java_array_t*) ((long) (x) - sizeof(java_array_t)))
#define JAVA_ARRAY_MAGIC		0xCAFE






typedef volatile long avm_spinlock_t;

#define AVM_MTX_KIND_DEFAULT			0
#define AVM_MTX_KIND_ERRORCHECK			1
#define AVM_MTX_KIND_RECURSIVE			2


typedef struct {
	avm_spinlock_t lock;
	long recursion;
	long kind;
	long owner;
} __packed avm_mutex_t;



typedef struct {
	u1 tag;
	
	union {
		struct {
			u2 name_index;
		} __packed class_info;
		
		struct {
			u2 name_and_type_index;
			u2 class_index;
		} __packed field_ref;
		
		struct {
			u2 name_and_type_index;
			u2 class_index;
		} __packed method_ref;
		
		struct {			
			u2 name_and_type_index;
			u2 class_index;
		} __packed interface_ref;
		
		struct {
			u2 string_index;
		} __packed string_info;
		
		struct {
			u4 bytes;
		} __packed int_info;
		
		struct {
			float bytes;
		} __packed float_info;
		
		struct {
			u8 bytes;
		} __packed long_info;
		
		struct {
			double bytes;
		} __packed double_info; 
		
		struct {
			u2 desc_index;
			u2 name_index;
		} name_and_type_info;
		
		struct {
			u2 length;
			u1* bytes;
		} utf8_info;
		
		struct {
			u1 reference_kind;
			u2 reference_index;
		} method_handle;
		
		struct {
			u2 desc_index;
		} method_type;
		
		struct {
			u2 name_and_type_index;
			u2 bootstrap_method_attr_index;
		} invoke_dynamic;
		
		u1 info[8];
	};
} __packed java_cp_info_t; 



typedef struct {
	u2 start_pc;
	u2 end_pc;
	u2 handler_pc;
	u2 catch_type;
} __packed java_exception_table_t;


typedef struct {
	u1 frame_type;
	u1 frame_data[0];
} __packed java_stackmap_frame_t;

typedef struct {
	u2 inner_class_info_index;
	u2 outer_class_info_index;
	u2 inner_name_index;
	u2 inner_class_access_flags;
} __packed java_inner_class_t;

typedef struct {
	u2 start_pc;
	u2 line_number;
} __packed java_line_number_table_t;

typedef struct {
	u2 start_pc;
	u2 length;
	u2 name_index;
	u2 desc_index;
	u2 index;
} __packed java_localvar_table_t;

typedef struct {
	u2 start_pc;
	u2 length;
	u2 name_index;
	u2 signature_index;
	u2 index;
} __packed java_localtype_table_t;

typedef struct java_attribute {
	u2 name_index;
	u4 length;
	
	union {
		struct {
			u2 const_value_index;
		} __packed const_value;
		
		struct {
			u2 max_stack;
			u2 max_locals;
			u4 code_length;
			u1* code;
			u2 exception_table_length;
			java_exception_table_t* exception_table;
			u2 attributes_count;
			struct java_attribute* attributes;
		} __packed code;
	
		struct {
			u2 number_of_entries;
			java_stackmap_frame_t* entries;
		} __packed stackmap_table;
	
		struct {
			u2 number_of_exceptions;
			u2* exception_index_table;
		} __packed exceptions;
		
		struct {
			u2 number_of_classes;
			java_inner_class_t* classes;
		} __packed inner_class;
		
		struct {
			u2 class_index;
			u2 method_index;
		} __packed enclosing_method;
		
		struct {
			/* Nothing. -.-' */
		} __packed synthetic;
		
		struct {
			u2 signature_index;
		} __packed signature;
		
		struct {
			u2 sourcefile_index;
		} __packed sourcefile;
		
		struct {
			u1* debug_extension;
		} __packed debug_extension;
		
		struct {
			u2 line_number_table_length;
			java_line_number_table_t* table;
		} __packed line_number_table;
		
		struct {
			u2 local_var_length;
			java_localvar_table_t* table;
		} __packed localvar_table;
		
		struct {
			u2 local_type_length;
			java_localtype_table_t* table;
		} __packed localtype_table;
		
		struct {
			/* Nothing. -.-' */
		} __packed deprecated;
	};
} __packed java_attribute_t;

typedef struct java_assembly java_assembly_t;
typedef struct {
	u2 flags;
	u2 name_index;
	u2 desc_index;
	u2 attr_count;
	java_attribute_t* attributes;

	/* Resolved Values */
	j_value value;
	java_assembly_t* assembly;

	char* name;
	char* desc;
} __packed java_field_t;



typedef struct {
	u2 flags;
	u2 name_index;
	u2 desc_index;
	u2 attr_count;
	java_attribute_t* attributes;

	/* Resolved Values */
	u2 nargs;
	u2 rettype;
	u1* signature;
	java_attribute_t* code;
	java_assembly_t* assembly;

	char* name;
	char* desc;
} __packed java_method_t;

typedef struct {
	u4 jc_magic;
	
	struct {
		u2 minor;
		u2 major;
	} __packed jc_version;
	
	/* Constant Pool */
	u2 jc_cp_count;
	java_cp_info_t* jc_cp;
	
	u2 jc_flags;
	u2 jc_this;
	u2 jc_super;
	
	u2 jc_interfaces_count;
	u2* jc_interfaces;
	
	u2 jc_fields_count;
	java_field_t* jc_fields;
	
	u2 jc_methods_count;
	java_method_t* jc_methods;
	
	u2 jc_attributes_count;
	java_attribute_t* jc_attributes;
} __packed java_class_t;

typedef struct java_assembly {
	char* path;
	char* name;

	java_class_t java_this;
	struct java_assembly* java_super;

	int resolved;
	void* buffer;
} java_assembly_t;


typedef struct java_context {
	struct java_context* parent;
	java_assembly_t* assembly;
		

	struct {
		struct {
			u4 pc;
			u4 pb;
			u4 sp;
		} regs;

		j_value* stack;
		u4 stack_size;

		j_value* locals;
		u4 locals_size;

		u1* code;
		java_method_t* method;

		j_value retval;
	} frame;

	struct {
		char* name;
		char* message;
		struct java_context* owner;
	} exception;

	u1 flags;
} java_context_t;


typedef struct {
	u2 magic;
	u2 type;
	u4 length;
	u1 data[0];
} java_array_t;


typedef struct java_object {
	java_assembly_t* assembly;

	avm_mutex_t lock;
	u4 refcount;
	u8 id;

	char* name;

	struct java_object* next;
} java_object_t;



typedef struct java_native {
	const char* classname;
	const char* name;
	const char* desc;
	u2 rettype;
	void* handler;

	struct java_native* next;
} java_native_t;

typedef struct java_library {
	const char* filename;
	void* fd;

	struct java_library* next;
} java_library_t;


#ifdef __cplusplus
extern "C" {
#endif

int java_assembly_find(java_assembly_t** assembly, const char* name);
int java_assembly_resolve(java_assembly_t* assembly);
int java_assembly_open(java_assembly_t** assembly, const char* filename);
int java_assembly_load(java_assembly_t** assembly, void* buffer, int size, const char* path);
int java_assembly_destroy(java_assembly_t* assembly, int recursive);
int java_assembly_base(java_assembly_t** base, java_assembly_t* A);


int jar_open(const char* filename);

int java_library_add(java_library_t** lib, const char* filename);
int java_library_load(java_library_t* lib, java_assembly_t**, const char* filename);


int java_attribute_load(java_assembly_t* assembly, void* buffer, java_attribute_t** attributes, u2 attr_count);
int java_attribute_find(java_attribute_t** attribute, java_assembly_t* assembly, java_attribute_t* A, int attr_count, const char* name);


int java_method_find(java_method_t** method, const char* classname, const char* methodname, const char* signature);
int java_method_find_reference(java_method_t** method, java_assembly_t* assembly, java_method_t* methods, u2 idx);
int java_method_resolve(java_method_t* method, java_assembly_t* assembly);
j_value java_method_invoke(java_context_t* j, java_assembly_t* assembly, java_method_t* method, j_value* params, int nargs);


int java_field_find(java_field_t** field, const char* classname, const char* fieldname, const char* signature);
int java_field_find_for_object(java_field_t** field, java_assembly_t* assembly, const char* classname, const char* fieldname, const char* signature);
int java_field_resolve(java_field_t* field, java_assembly_t* assembly);

void java_context_run(java_context_t* j);

void athrow(java_context_t* j, const char* exception, const char* message);
void rethrow(java_context_t* j, java_context_t* d);

int java_object_new(java_object_t** obj, const char* classname);
int java_object_new_from_idx(java_object_t** obj, java_assembly_t* assembly, u2 idx);

j_value java_native_invoke(java_method_t* method, java_native_t* func, j_value* params, int nargs);
int java_native_find(java_native_t** func, const char* classname, const char* name);
int java_native_add(const char* classname, const char* name, const char* desc, u2 rettype, void* handler);




/* A+ Virtual Machine - Main API */
void avm_init(void);
void avm_begin(void);
void avm_end(void);
int avm_initialized(void);
void avm_set_entrypoint(char* e);

int avm_load(void* buffer, int size, const char* name);
int avm_open(const char* filename);
int avm_open_library(const char* filename);
char* avm_make_signature(int rettype, ...);

j_value avm_call(const char* classname, const char* name, int nargs, ...);
j_value avm_main(int argc, char** argv);

void avm_config_path_add(const char* dir);
void avm_config_set_ops (
/*	void* (*calloc) (size_t, size_t),
	void (*free) (void*),
	int (*open) (const char*, int, ...),
	int (*close) (int),
	off_t (*lseek) (int, off_t, int),
	ssize_t (*read) (int, void*, size_t),
	int (*yield) (),
	pid_t (*getpid) (),
	int (*printf) (const char*, ...)
*/
	void*, void*, void*, void*, void*, void*, void*, void*, void*
);


int avm_spinlock_init(avm_spinlock_t* lock);
void avm_spinlock_lock(avm_spinlock_t* lock);
int avm_spinlock_trylock(avm_spinlock_t* lock);
void avm_spinlock_unlock(avm_spinlock_t* lock);

int avm_mutex_init(avm_mutex_t* mtx, long kind);
int avm_mutex_lock(avm_mutex_t* mtx);
int avm_mutex_trylock(avm_mutex_t* mtx);
int avm_mutex_unlock(avm_mutex_t* mtx);

#ifdef __cplusplus
}
#endif

#endif
