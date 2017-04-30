#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/intr.h>
#include <aplus/input.h>
#include <libc.h>

MODULE_NAME("pc/input/ps2");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#	if defined(__i386__)
#		include <arch/i386/i386.h>
#	elif defined(__x86_64__)
#		include <arch/x86_64/x86_64.h>
#	endif


#define PS2_DATA		0x60
#define PS2_CTRL		0x64
#define PS2_ACK			0xFA
#define PS2_RESEND		0xFE



#define PS2_WAIT									\
	{												\
		int t = 100000;								\
		while((inb(PS2_CTRL) & 0x02) && t > 0)		\
			t--;									\
	}

#define PS2_WAIT_0									\
	{												\
		int t = 100000;								\
		while(!(inb(PS2_CTRL) & 0x01) && t > 0)		\
			t--;									\
	}





static char vkrel = VK_RELEASED;
static mouse_t mouse;
static struct {
	uint8_t vkeymap[256];
	uint8_t vkeymap_e0[256];
	uint8_t vkeys[256];
	uint8_t capslock;
	uint8_t numlock;
	uint8_t scrolllock;
	uint8_t e0;
} kb;


static void __fifo_send(const char* dev, void* ptr, size_t size) {
	int fd = sys_open(dev, O_WRONLY, 0);
	if(fd < 0)
		return;
	
	sys_write(fd, ptr, size);
	sys_close(fd);
}

static void kb_setled() {
	PS2_WAIT;
	outb(PS2_DATA, 0xED);


	PS2_WAIT;
	outb (
		PS2_DATA,
	
		(kb.scrolllock ? (1 << 0) : 0)	|
		(kb.numlock ? (1 << 1) : 0) 	|
		(kb.capslock ? (1 << 2) : 0)
	);
	
	PS2_WAIT;
}

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
	}


	uint8_t vkey = VK_NULL;
	if(kb.e0)
		vkey = kb.vkeymap_e0[vkscan & 0x7F];
	else
		vkey = kb.vkeymap[vkscan & 0x7F];

	if(vkey == VK_NULL)
		return;
	kb.e0 = 0;


	
	switch(vkey) {
		case VK_CAPITAL:
			kb.capslock != kb.capslock;
			kb_setled();
			break;
		case VK_NUMLOCK:
			kb.numlock != kb.numlock;
			kb_setled();
			break;
		case VK_SCROLL:
			kb.scrolllock != kb.scrolllock;
			kb_setled();
	}


	if(vkscan & 0x80)
		__fifo_send(PATH_KBDEV, &vkrel, 1);

	__fifo_send(PATH_KBDEV, &vkey, 1);



	kb.vkeys[vkey] = !!!(vkscan & 0x80);
	PS2_WAIT;
}


void mouse_intr(void* unused) {
	int s, j;
	s = inb(PS2_CTRL);

	while((s & 0x01)) {
		j = inb(PS2_DATA);
		
		if((s & 0x20)) {
	
			mouse.pack[mouse.cycle] = j;

			switch(mouse.cycle) {
				case 0:
					if(!(j & 0x08))
						return;

					mouse.cycle++;
					break;
				case 1:
				//case 2:
					mouse.cycle++;
					break;
				case 2:
					if(
						(mouse.pack[0] & 0x80) ||
						(mouse.pack[0] & 0x40)
					) break;
				
					mouse.dx = (mouse.pack[1] - ((mouse.pack[0] & 0x10) ? 256 : 0)) * mouse.speed;
					mouse.dy = (mouse.pack[2] - ((mouse.pack[0] & 0x20) ? 256 : 0)) * mouse.speed;

					//mouse.dz = mouse.pack[3];

					mouse.x += mouse.dx;
					mouse.y -= mouse.dy;
					
					/* TODO: Add clipping */
					
	
					mouse.buttons[0] = (mouse.pack[0] & 0x01);
					mouse.buttons[1] = (mouse.pack[0] & 0x02);
					mouse.buttons[2] = (mouse.pack[0] & 0x04);
					
					
					__fifo_send(PATH_MOUSEDEV, &mouse, sizeof(mouse));
					
					mouse.cycle = 0;
					break;
			}
		}
	
		s = inb(PS2_CTRL);
	}
}



int init(void) {
	memset(&kb, 0, sizeof(kb));
	memset(&mouse, 0, sizeof(mouse));


	#define _k(x)	\
		kb.vkeymap[x]

	_k(0x1E) = VK_A;
	_k(0x30) = VK_B;
	_k(0x2E) = VK_C;
	_k(0x20) = VK_D;
	_k(0x12) = VK_E;
	_k(0x21) = VK_F;
	_k(0x22) = VK_G;
	_k(0x23) = VK_H;
	_k(0x17) = VK_I;
	_k(0x24) = VK_J;
	_k(0x25) = VK_K;
	_k(0x26) = VK_L;
	_k(0x32) = VK_M;
	_k(0x31) = VK_N;
	_k(0x18) = VK_O;
	_k(0x19) = VK_P;
	_k(0x10) = VK_Q;
	_k(0x13) = VK_R;
	_k(0x1F) = VK_S;
	_k(0x14) = VK_T;
	_k(0x16) = VK_U;
	_k(0x2F) = VK_V;
	_k(0x11) = VK_W;
	_k(0x2D) = VK_X;
	_k(0x15) = VK_Y;
	_k(0x2C) = VK_Z;
	_k(0x0B) = VK_0;
	_k(0x02) = VK_1;
	_k(0x03) = VK_2;
	_k(0x04) = VK_3;
	_k(0x05) = VK_4;
	_k(0x06) = VK_5;
	_k(0x07) = VK_6;
	_k(0x08) = VK_7;
	_k(0x09) = VK_8;
	_k(0x0A) = VK_9;
	_k(0x29) = VK_OEM_3;
	_k(0x0C) = VK_OEM_MINUS;
	_k(0x0D) = VK_OEM_PLUS;
	_k(0x2B) = VK_OEM_5;
	_k(0x0E) = VK_BACK;
	_k(0x39) = VK_SPACE;
	_k(0x0F) = VK_TAB;
	_k(0x3A) = VK_CAPITAL;
	_k(0x2A) = VK_LSHIFT;
	_k(0x1D) = VK_LCONTROL;
	_k(0x38) = VK_MENU;
	_k(0x36) = VK_RSHIFT;
	_k(0x1C) = VK_RETURN;
	_k(0x01) = VK_ESCAPE;
	_k(0x3B) = VK_F1;
	_k(0x3C) = VK_F2;
	_k(0x3D) = VK_F3;
	_k(0x3E) = VK_F4;
	_k(0x3F) = VK_F5;
	_k(0x40) = VK_F6;
	_k(0x41) = VK_F7;
	_k(0x42) = VK_F8;
	_k(0x43) = VK_F9;
	_k(0x44) = VK_F10;
	_k(0x57) = VK_F11;
	_k(0x58) = VK_F12;
	_k(0x46) = VK_SCROLL;
	_k(0x1A) = VK_OEM_4;
	_k(0x45) = VK_NUMLOCK;
	_k(0x37) = VK_MULTIPLY;
	_k(0x4A) = VK_SUBTRACT;
	_k(0x4E) = VK_ADD;
	_k(0x53) = VK_DECIMAL;
	_k(0x52) = VK_NUMPAD0;
	_k(0x4F) = VK_NUMPAD1;
	_k(0x50) = VK_NUMPAD2;
	_k(0x51) = VK_NUMPAD3;
	_k(0x4B) = VK_NUMPAD4;
	_k(0x4C) = VK_NUMPAD5;
	_k(0x4D) = VK_NUMPAD6;
	_k(0x47) = VK_NUMPAD7;
	_k(0x48) = VK_NUMPAD8;
	_k(0x49) = VK_NUMPAD9;
	_k(0x1B) = VK_OEM_6;
	_k(0x27) = VK_OEM_1;
	_k(0x28) = VK_OEM_7;
	_k(0x33) = VK_OEM_COMMA;
	_k(0x34) = VK_OEM_PERIOD;
	_k(0x35) = VK_OEM_2;

	#undef _k
	#define _k(x)	\
		kb.vkeymap_e0[x]

	_k(0x5B) = VK_LWIN;
	_k(0x1D) = VK_RCONTROL;
	_k(0x5C) = VK_RWIN;
	_k(0x38) = VK_RMENU;
	_k(0x5D) = VK_APPS;
	_k(0x2A) = VK_NULL;
	_k(0x37) = VK_PRINT;
	_k(0x52) = VK_INSERT;
	_k(0x47) = VK_HOME;
	_k(0x49) = VK_PAGE_UP;
	_k(0x53) = VK_DELETE;
	_k(0x4F) = VK_END;
	_k(0x51) = VK_PAGE_DOWN;
	_k(0x48) = VK_UP;
	_k(0x4B) = VK_LEFT;
	_k(0x50) = VK_DOWN;
	_k(0x4D) = VK_RIGHT;
	_k(0x35) = VK_DIVIDE;
	_k(0x1C) = VK_NUMLOCK;

	#undef _k



	#define MOUSE_WRITE(x)		\
		PS2_WAIT;				\
		outb(PS2_CTRL, 0xD4);	\
		PS2_WAIT;				\
		outb(PS2_DATA, x)

	#define MOUSE_READ(x)		\
		PS2_WAIT_0;				\
		x = inb(PS2_DATA)


	PS2_WAIT;
	outb(PS2_CTRL, 0xA8);
	PS2_WAIT;
	outb(PS2_CTRL, 0x20);
	PS2_WAIT_0;
	
	int s = inb(PS2_DATA) | 2;
	PS2_WAIT;

	outb(PS2_CTRL, 0x60);
	PS2_WAIT;
	outb(PS2_DATA, s);



	MOUSE_WRITE(0xF6);
	MOUSE_READ(s);

#if 0
	MOUSE_WRITE(0xF3);
	MOUSE_READ(s);
	MOUSE_WRITE(200);
	MOUSE_READ(s);
	MOUSE_WRITE(0xF3);
	MOUSE_READ(s);
	MOUSE_WRITE(100);
	MOUSE_READ(s);
	MOUSE_WRITE(0xF3);
	MOUSE_READ(s);
	MOUSE_WRITE(80);
	MOUSE_READ(s);
	MOUSE_WRITE(0xF2);
	MOUSE_READ(s);
#endif

	MOUSE_WRITE(0xF4);
	MOUSE_READ(s);
	


	mouse.speed = 1;
	mouse.clip.left = 0;
	mouse.clip.top = 0;
	mouse.clip.right = 0xFFFF;
	mouse.clip.bottom = 0xFFFF;


	if(sys_mkfifo(PATH_KBDEV, 0666) != 0)
		kprintf(ERROR "%s: cannot create FIFO device!\n", PATH_KBDEV);
		
	if(sys_mkfifo(PATH_MOUSEDEV, 0666) != 0)
		kprintf(ERROR "%s: cannot create FIFO device!\n", PATH_MOUSEDEV);
		

	irq_enable(1, kb_intr);
	irq_enable(12, mouse_intr);


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
