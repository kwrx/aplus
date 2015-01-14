# Posix Thread library
POSIX Threads, usually referred to as Pthreads, is a POSIX standard for threads. The standard, POSIX.1c, Threads extensions (IEEE Std 1003.1c-1995), defines an API for creating and manipulating threads.


#### Required function
* Create a Thread;
* Terminate a Thread;
* Get Thread ID.

```
 tid_t __os_thread_create(void* entrypoint, void* params, int priority);
 int __os_thread_kill(int killid, int exitcode);
 tid_t __os_gettid();
 
 See "/platform/win32/hooks.h" as example.
```






#### Current status
- [X] Thread functions
- [X] Thread attribute functions
- [X] Scheduling functions
- [X] Thread specific data functions
- [X] Mutex attribute functions
- [X] Mutex functions
- [X] Condition variable attribute functions
- [X] Condition variable functions
- [X] Barrier attribute functions
- [X] Barrier functions
- [X] Read-write lock attribute functions
- [X] Read-write lock functions
- [X] Spinlock functions
