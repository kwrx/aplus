__attribute__((noreturn))
extern void __exit(int);

__attribute__((noreturn))
void _exit(int e) {
    __exit(e & 0377);
}