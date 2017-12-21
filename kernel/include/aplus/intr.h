#ifndef _INTR_H
#define _INTR_H

void intr_disable(void);
void intr_enable(void);


#define INTR_OFF        intr_disable()
#define INTR_ON         intr_enable()

#endif
