#ifndef _INTR_H
#define _INTR_H

typedef void (irq_handler_t) (void*);


void irq_enable(int number, irq_handler_t handler);
void irq_disable(int number);

void* irq_set_data(int number, void* data);
void* irq_get_data(int number);

void irq_ack(int irq_no);

void intr_disable(void);
void intr_enable(void);


extern int current_irq;

#define INTR_OFF	intr_disable()
#define INTR_ON		intr_enable()

#endif
