# aPlus - Hardware Abstraction Layer


## Timer


    `typedef TYPE ktime_t`
    
    `ktime_t timer_gettimestamp()`
    `ktime_t timer_getticks()`
    `ktime_t timer_getms()`
    `ktime_t timer_getus()`
    `ktime_t timer_getfreq()`
    `void timer_delay(ktime_t milliseconds)`
---

## Task
    `typedef STRUCT ARCH_task_context`

    `int task_set_thread_area(volatile task_t* tk, struct __user_desc* uinfo)`
    `volatile task_t* task_clone(int (*fn) (void*), void* stack, int flags, void* arg)`
    `volatile task_t* task_fork(void)`
    `void task_yield(void)`
    `void task_switch(volatile* task_t* prev_task, volatile task_t* new_task)`
    `void task_release(volatile* task_t* task)`
---

## Memory
    `typedef POINTER physaddr_t`
    `typedef POINTER virtaddr_t`

    `physaddr_t __V2P(virtaddr_t virtaddr)`
    `void map_page(virtaddr_t virtaddr, physaddr_t physaddr, int user)`
    `void unmap_page(virtaddr_t virtaddr)`
    `void enable_page(virtaddr_t virtaddr)`
    `void disable_page(virtaddr_t virtaddr)`
    `virtaddr_t get_free_pages(int count, int maked, int user)`
---

## Interrupts
    `void intr_enable(void)`
    `void intr_disable(void)`
---

## ELF
    `int elf_check_machine(Elf_Ehdr* elf)`
---

## Debug
    `void debug_send(char value)`
    `void debug_lookup_symbol(symbol_t* symtab, uintptr_t address)`
    `void debug_dump(void* context, char* errmsg, uintptr_t dump, uintptr_t errcode)`
    `void debug_stacktrace(int frames)`
---