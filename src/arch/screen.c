//
//  screen.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the kfree Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <string.h>
#include <stdint.h>
#include <time.h>
#include <aplus.h>
#include <aplus/int86.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>
#include <aplus/ioctl.h>
#include <fcntl.h>


#define GFX_ADDR         0x3CE
#define GFX_DATA         0x3CF
#define GFX_I_RESET      0x00
#define GFX_I_ENABLE      0x01
#define GFX_I_COLORCMP   0x02
#define GFX_I_ROTATE      0x03
#define GFX_I_READMAP   0x04
#define GFX_I_MODE      0x05
#define GFX_I_MISC      0x06
#define GFX_I_CNOCARE   0x07
#define GFX_I_BITMASK   0x08

#define SEQ_ADDR          0x3C4
#define SEQ_DATA          0x3C5
#define SEQ_I_RESET       0x00
#define SEQ_I_CLOCK       0x01
#define SEQ_I_MAPMASK    0x02
#define SEQ_I_CHARMAP   0x03
#define SEQ_I_MEMMODE   0x04

#define ATR_ADDR_DATA   0x3C0
#define ATR_DATA_READ   0x3C1

#define CRT_ADDR               0x3D4
#define CRT_DATA               0x3D5
#define CRT_I_H_TOTAL         0x00
#define CRT_I_END_H_DIS       0x01
#define CRT_I_START_H_BLANK    0x02
#define CRT_I_END_H_BALNK       0x03
#define CRT_I_START_H_RET       0x04
#define CRT_I_END_H_RET       0x05
#define CRT_I_V_TOTAL          0x06
#define CRT_I_OVERFLOW          0x07
#define CRT_I_PRE_ROW_SCN       0x08
#define CRT_I_MAX_SCN_LINE    0x09
#define CRT_I_START_CUR       0x0A
#define CRT_I_END_CUR          0x0B
#define CRT_I_START_ADDR_H    0x0C
#define CRT_I_START_ADDR_L    0x0D
#define CRT_I_CUR_LOC_H       0x0E
#define CRT_I_CUR_LOC_L       0x0F
#define CRT_I_START_V_RET       0x10
#define CRT_I_END_V_RET       0x11
#define CRT_I_END_V_DIS       0x12
#define CRT_I_OFFSET          0x13
#define CRT_I_UND_LOC          0x14
#define CRT_I_START_V_BLANK    0x15
#define CRT_I_END_V_BLANK       0x16
#define CRT_I_MODE             0x17
#define CRT_I_LINE_CMP          0x18

#define DAC_ADDR_W      0x3C8
#define DAC_ADDR_R      0x3C7
#define DAC_DATA         0x3C9
#define DAC_STATE         0x3C7

#define VGA_MISC_W      0x3C2
#define VGA_MISC_R      0x3CC
#define VGA_FEATURE_W   0x3DA
#define VGA_FEATURE_R   0x3CA
#define VGA_STATUS_0      0x3C2
#define VGA_STATUS_1      0x3DA




	

uint32_t videopos = 0;
uint32_t videoattr = 0x07;

static uint8_t video_width = 80;
static uint8_t video_height = 25;


int video_putc(char ch) {

	__lock

#ifdef DEBUG
#ifdef BOCHS_DEBUG	
	outb(0xE9, ch);
#endif
#endif

	switch(ch) {
		case '\n':
			videopos += video_width - (videopos % video_width);
			break;
			
		case '\b':
			videopos -= 1;
			*(uint16_t*)(0xb8000 + (videopos * 2)) = ((videoattr & 0xFF) << 8) | (0x20 & 0xFF);
			break;
			
		case '\t':
			for(int i = 0, x = 8 - ((videopos % video_width) % 8); i < x; i++)
				videopos += 1;
			break;
			
		case '\v':
			videopos += video_width;
			break;
			
		case '\r':
			videopos -= (videopos % video_width);
			break;
			
		case '\0':
			break;
			
		default:
			*(uint16_t*)(0xb8000 + (videopos * 2)) = ((videoattr & 0xFF) << 8) | (ch & 0xFF);
			videopos += 1;
			break;
	}
	
	
	if(videopos > (video_width * video_height - 1)) {
		videopos -= video_width;
		memcpy(0xb8000, 0xb8000 + (video_width * 2), video_width * video_height * 2);
		memset(0xb8000 + (video_width * video_height * 2 - video_width * 2), 0, video_width * 2);
	}
	
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0xD);
	outb(0x3D4, 0x0B);
	outb(0x3D5, 0xE);

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (videopos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((videopos >> 8) & 0xFF));
			
	__unlock
}

int video_puts(char* s) {

	__lock
	
	while(*s)
		video_putc(*s++);
		
	__unlock
}


int video_clear() {

	__lock

	videopos = 0;
	for(int i = 0; i < video_width * video_height; i++)
		*(uint16_t*)(0xb8000 + (i * 2)) = ((videoattr & 0xFF) << 8) | 0x20;
		
	video_putc(0);
		
	__unlock
}

int video_init() {


	struct int86_regs regs;
	regs.ax = 0x1112;
	regs.bx = 0;
	
	int86(0x10, &regs);

	
	
	outb(SEQ_ADDR, SEQ_I_RESET);
	outb(SEQ_DATA, 0x01);

    outb(VGA_MISC_W, 0xE7);

	outb(SEQ_ADDR, SEQ_I_RESET);
	outb(SEQ_DATA, 0x03);

	outb(SEQ_ADDR, SEQ_I_RESET);
	outb(SEQ_DATA, 0x02);

    outb(SEQ_ADDR, SEQ_I_CLOCK);
    outb(SEQ_DATA, 0x01);

	outb(SEQ_ADDR, SEQ_I_RESET);
	outb(SEQ_DATA, 0x03);

	outb(CRT_ADDR, CRT_I_END_V_RET);
	outb(CRT_DATA, 0x0E);

	outb(CRT_ADDR, CRT_I_H_TOTAL);
	outb(CRT_DATA, 0x6B);

	outb(CRT_ADDR, CRT_I_END_H_DIS);
	outb(CRT_DATA, 0x59);

	outb(CRT_ADDR, CRT_I_START_H_BLANK);
	outb(CRT_DATA, 0x5A);

	outb(CRT_ADDR, CRT_I_END_H_BALNK);
	outb(CRT_DATA, 0x82);

	outb(CRT_ADDR, CRT_I_START_H_RET);
	outb(CRT_DATA, 0x60);

	outb(CRT_ADDR, CRT_I_END_H_RET);
	outb(CRT_DATA, 0x8D);

	outb(CRT_ADDR, CRT_I_V_TOTAL);
	outb(CRT_DATA, 0x0B);

	outb(CRT_ADDR, CRT_I_OVERFLOW);
	outb(CRT_DATA, 0x3E);

	outb(CRT_ADDR, CRT_I_PRE_ROW_SCN);
	outb(CRT_DATA, 0x00);

	outb(CRT_ADDR, CRT_I_MAX_SCN_LINE);
	outb(CRT_DATA, 0x47);

	outb(CRT_ADDR, CRT_I_START_V_RET);
	outb(CRT_DATA, 0xEA);

	outb(CRT_ADDR, CRT_I_END_V_RET);
	outb(CRT_DATA, 0x0C);

	outb(CRT_ADDR, CRT_I_END_V_DIS);
	outb(CRT_DATA, 0xDF);

	outb(CRT_ADDR, CRT_I_OFFSET);
	outb(CRT_DATA, 0x2D);

	outb(CRT_ADDR, CRT_I_UND_LOC);
	outb(CRT_DATA, 0x08);

	outb(CRT_ADDR, CRT_I_START_V_BLANK);
	outb(CRT_DATA, 0xE8);

	outb(CRT_ADDR, CRT_I_END_V_BLANK);
	outb(CRT_DATA, 0x05);

	outb(CRT_ADDR, CRT_I_MODE);
	outb(CRT_DATA, 0xA3);

	outb(CRT_ADDR, CRT_I_LINE_CMP);
	outb(CRT_DATA, 0xFF);

	
	video_width = 90;
	video_height = 60;
	
	video_clear();
}

