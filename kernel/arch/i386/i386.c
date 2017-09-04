#include <aplus.h>

extern int intr_init();
extern int timer_init();
extern int task_init();
extern int pci_init();



void i386_init() {
    __builtin_cpu_init();

    uintptr_t p;
    for(p = 0x000F0000; p < 0x000FFFFF; p += 16) {
        if(strncmp((char*) p, "_SM_", 4) != 0)
            continue;

        uintptr_t sta = *(uintptr_t*) (p + 0x18);
        uint16_t len = *(uint16_t*) (p + 0x1C);

        int i;
        for(i = 0; i < len; i++) {
            if(*(uint8_t*) (sta) != 4) {
                sta += *(uint8_t*) (sta + 0x01);

                while(*(uint16_t*) sta != 0)
                    sta++;

                sta += 2;
                continue;
            }

            mbd->cpu.family = "Unknown"; //x86_cpu_family[*(uint8_t*) (sta + 0x06)];
            mbd->cpu.speed = *(uint16_t*) (sta + 0x16);
            mbd->cpu.cores = *(uint8_t*) (sta + 0x23);
            mbd->cpu.threads = *(uint8_t*) (sta + 0x24);

            break;
        }
    }

    (void) intr_init();
    (void) pci_init();
    (void) timer_init();
    (void) task_init();
}
