#include <aplus.h>
#include <aplus/mmio.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <libc.h>
#include "arm.h"


spinlock_t serial_lock;


int serial_init() {

    spinlock_init(&serial_lock);

    mmio_w32(UART_CR(UART0), 0);
    mmio_w32(GPPUD, 0);
    delay(150);

    mmio_w32(GPPUDCLK0, (1 << 14) | (1 << 15));
    delay(150);

    mmio_w32(GPPUDCLK0, (0));
    mmio_w32(UART_ICR(UART0), 0x7FF);

    mmio_w32(UART_IBRD(UART0), 1);
    mmio_w32(UART_FBRD(UART0), 40);

    mmio_w32(UART_LCRH(UART0), (1 << 4) | (1 << 5) | (1 << 6));
    mmio_w32(UART_IMSC(UART0), (1 << 1) | (1 << 4) | (1 << 5)     |
                                (1 << 6) | (1 << 7) | (1 << 8)    |
                                (1 << 9) | (1 << 10));

    mmio_w32(UART_CR(UART0), (1 << 0) | (1 << 8) | (1 << 9));


    return 0;
}


void serial_send(uint8_t ch, uint8_t v) {
    (void) ch;

    spinlock_lock(&serial_lock);

    while(mmio_r32(UART_FR(UART0)) & (1 << 5))
        ;
    mmio_w32(UART_DR(UART0), v);

    spinlock_unlock(&serial_lock);
}

uint8_t serial_recv(uint8_t ch) {
    (void) ch;

    spinlock_lock(&serial_lock);

    while(mmio_r32(UART_FR(UART0)) & (1 << 4))
        ;
    register uint8_t t = (uint8_t) mmio_r32(UART_DR(UART0));

    spinlock_lock(&serial_lock);
    return t;
}



