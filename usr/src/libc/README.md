# C Standard Library
The C standard library is the standard library for the C programming language, as specified in the ANSI C standard.
The C standard library provides macros, type definitions, and functions for tasks like string handling, mathematical computations, input/output processing, memory allocation and several other operating system services.


#### Required syscalls

```
 void sys_exit(int status);
 int sys_open(const char* file, int flags, ...);
 int sys_close(int fd);
 int sys_read(int fd, void* buf, int size);
 int sys_write(int fd, void* buf, int size);
 int sys_lseek(int fd, int off, int dir);
 int sys_link(const char* oldname, const char* newname);
 int sys_unlink(const char* filename);
 
 See "src/config.h".
```

##### Work in progress...
