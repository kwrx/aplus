#ifndef _RPI_H
#define _RPI_H

#include <aplus.h>
#include <aplus/mmio.h>

#define MAILBOX_BASE		0x2000B880

#define GPIO_BASE			0x20200000
#define GPPUD				(GPIO_BASE + 0x94)
#define GPPUDCLK0			(GPIO_BASE + 0x98)


#define LFBIO_BASE			0x40040000
#define LFBIO_BOX			1


#define UART0				0x20201000
#define UART1				0x20202000


#define UART_DR(x)			(x + 0x00)
#define UART_RSRECR(x)		(x + 0x04)
#define UART_FR(x)			(x + 0x18)
#define UART_ILPR(x)		(x + 0x20)
#define UART_IBRD(x)		(x + 0x24)
#define UART_FBRD(x)		(x + 0x28)
#define UART_LCRH(x)		(x + 0x2C)
#define UART_CR(x)			(x + 0x30)
#define UART_IFLS(x)		(x + 0x34)
#define UART_IMSC(x)		(x + 0x38)
#define UART_RIS(x)			(x + 0x3C)
#define UART_MIS(x)			(x + 0x40)
#define UART_ICR(x)			(x + 0x44)
#define UART_DMACR(x)		(x + 0x48)
#define UART_ITCR(x)		(x + 0x80)
#define UART_ITIP(x)		(x + 0x84)
#define UART_ITOP(x)		(x + 0x88)
#define UART_TDR(x)			(x + 0x8C)



#define LFB_WIDTH			800
#define LFB_HEIGHT			600
#define LFB_DEPTH			32


#define TIMER_FREQ			1000000
#define TIMER_BASE			0x20003000
#define TIMER_CTNL			0
#define TIMER_TICK			4

#define INTR_BASE				0x2000B200
#define INTR_TIMER				(1 << 0)
#define INTR_MAILBOX			(1 << 1)
#define INTR_DOORBELL_0			(1 << 2)
#define INTR_DOORBELL_1			(1 << 3)
#define INTR_GPU0_HALT			(1 << 4)
#define INTR_GPU1_HALT			(1 << 5)
#define INTR_ACCESS_ERR_1		(1 << 6)
#define INTR_ACCESS_ERR_0		(1 << 7)

#define INTR_TIMER_BASE					0x2000B400
#define INTR_TIMER_CTRL_23BIT			(1 << 1)
#define INTR_TIMER_CTRL_PRESCALE_1		(0 << 2)
#define INTR_TIMER_CTRL_PRESCALE_16		(1 << 2)
#define INTR_TIMER_CTRL_PRESCALE_256	(2 << 2)
#define INTR_TIMER_CTRL_INT_ENABLE		(1 << 5)
#define INTR_TIMER_CTRL_INT_DISABLE		(0 << 5)
#define INTR_TIMER_CTRL_ENABLE			(1 << 7)
#define INTR_TIMER_CTRL_DISABLE			(0 << 7)


static inline void delay(uint32_t count) {
	__asm__ __volatile__ (
		"__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
		 : : [count]"r"(count) : "cc"
	);
}

static inline int mail_send(uint32_t base, uint32_t box) {
	while((mmio_r32(MAILBOX_BASE + 0x18) & 0x80000000) != 0);

	mmio_w32(MAILBOX_BASE + 0x20, base + box);
	return 0;
}


static inline uint32_t mail_recv(uint32_t box) {
	uint32_t ret;	

	do {
		while(((ret = mmio_r32(MAILBOX_BASE + 0x18)) & 0x40000000) != 0);
	} while(((ret = mmio_r32(MAILBOX_BASE)) & 0x0F) != box);

	return ret;
}


static inline uint32_t cpsr_get() {
	uint32_t r;
	__asm__ __volatile__ (
		"mrs %[ps], cpsr" : [ps]"=r" (r)
	);

	return r;
}

static inline void cpsr_set(uint32_t r) {
	__asm__ __volatile__ (
		"msr cpsr, %[ps]" : : [ps]"r"(r)
	);
}



typedef struct regs {
	uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;
} regs_t;

#endif
