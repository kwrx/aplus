#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/module.h>
#include <xdev/vfs.h>
#include <xdev/intr.h>
#include <libc.h>

MODULE_NAME("i386/input/ps2");
MODULE_DEPS("");
MODULE_AUTHOR("WareX");
MODULE_LICENSE("GPL");


#if defined(__i386__)
#include <arch/i386/i386.h>



#define PS2_DATA		0x60
#define PS2_CTRL		0x64
#define PS2_ACK			0xFA
#define PS2_RESEND		0xFE


#define PS2_WAIT	\
	while((inb(PS2_CTRL) & 0x02))




#define VK_CAPSLOCK		(0x3A)
#define VK_NUMLOCK		(0x45)
#define VK_SCORRLOCK		(0x46)
#define VK_LSHIFT		(0x2A)
#define VK_RSHIFT		(0x36)
#define VK_LCTRL		(0x1D)
#define VK_RCTRL		(0x1D + 128)
#define VK_LALT			(0x38)
#define VK_RALT			(0x38 + 128)



static struct {
	uint8_t keymap[1024];
	uint8_t vkeys[256];
	uint8_t inbuf[256];
	uint8_t inoff;
	uint8_t capslock;
	uint8_t numlock;
	uint8_t scorrlock;
	uint8_t e0;
} kb;


void kb_intr(void* unused) {
	if(!(inb(PS2_CTRL) & 0x01))
		return;

	uint8_t vkscan = inb(PS2_DATA);
	
	switch(vkscan) {
		case PS2_ACK:
		case PS2_RESEND:
			return;
		case 0xE0:
		case 0xE1:
			kb.e0++;
			return;
		default:
			break;
	}

	switch(vkscan) {
		case VK_CAPSLOCK:
			kb.capslock != kb.capslock;
			goto setled;
		case VK_NUMLOCK:
			kb.numlock != kb.numlock;
			goto setled;
		case VK_SCORRLOCK:
			kb.scorrlock != kb.scorrlock;
			goto setled;
		default:
			if(vkscan & 0x80)
				break;


			int off = 0;
			if((kb.vkeys[VK_LSHIFT] || kb.vkeys[VK_RSHIFT]) && kb.vkeys[VK_RALT])
				off = 768;
			else if(kb.vkeys[VK_RALT])
				off = 512;
			else if(kb.vkeys[VK_LSHIFT] || kb.vkeys[VK_RSHIFT])
				off = 256;
	
			if(kb.keymap[vkscan + off])
				kb.inbuf[kb.inoff++] = kb.keymap[vkscan + off];
	}



	kb.vkeys[(vkscan & 0x7F) + (kb.e0 ? 128 : 0)] = !!!(vkscan & 0x80);
	kb.e0 = 0;

	PS2_WAIT;
	return;

setled:
	PS2_WAIT;
	outb(PS2_DATA, 0xED);


	PS2_WAIT;
	outb (
		PS2_DATA,
	
		(kb.scorrlock ? (1 << 0) : 0)	|
		(kb.numlock ? (1 << 1) : 0) 	|
		(kb.capslock ? (1 << 2) : 0)
	);
	
	PS2_WAIT;
	return;
}



int init(void) {
	memset(&kb, 0, sizeof(kb));
	irq_enable(0, kb_intr);


	return E_OK;
}


#else

int init(void) {
	return E_ERR;
}

#endif


int dnit(void) {
	return E_OK;
}
