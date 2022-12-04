/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_FB_H
#define _APLUS_FB_H

#ifndef __ASSEMBLY__

#include <sys/ioctl.h>
#include <stdint.h>

#define FB_MAX			                                32

#define FBIOGET_VSCREENINFO	                            0x4600
#define FBIOPUT_VSCREENINFO	                            0x4601
#define FBIOGET_FSCREENINFO	                            0x4602
#define FBIOGETCMAP		                                0x4604
#define FBIOPUTCMAP		                                0x4605
#define FBIOPAN_DISPLAY		                            0x4606
#define FBIO_CURSOR                                     _IOWR('F', 0x08, struct fb_cursor)
#define FBIOGET_CON2FBMAP	                            0x460F
#define FBIOPUT_CON2FBMAP	                            0x4610
#define FBIOBLANK		                                0x4611
#define FBIOGET_VBLANK		                            _IOR('F', 0x12, struct fb_vblank)
#define FBIO_ALLOC                                      0x4613
#define FBIO_FREE                                       0x4614
#define FBIOGET_GLYPH                                   0x4615
#define FBIOGET_HWCINFO                                 0x4616
#define FBIOPUT_MODEINFO                                0x4617
#define FBIOGET_DISPINFO                                0x4618
#define FBIO_WAITFORVSYNC	                            _IOW('F', 0x20, uint32_t)

#define FB_TYPE_PACKED_PIXELS		                    0	/* Packed Pixels	*/
#define FB_TYPE_PLANES			                        1	/* Non interleaved planes */
#define FB_TYPE_INTERLEAVED_PLANES	                    2	/* Interleaved planes	*/
#define FB_TYPE_TEXT			                        3	/* Text/attributes	*/
#define FB_TYPE_VGA_PLANES		                        4	/* EGA/VGA planes	*/
#define FB_TYPE_FOURCC			                        5	/* Type identified by a V4L2 FOURCC */

#define FB_AUX_TEXT_MDA		                            0	/* Monochrome text */
#define FB_AUX_TEXT_CGA		                            1	/* CGA/EGA/VGA Color text */
#define FB_AUX_TEXT_S3_MMIO	                            2	/* S3 MMIO fasttext */
#define FB_AUX_TEXT_MGA_STEP16	                        3	/* MGA Millenium I: text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_MGA_STEP8	                        4	/* other MGAs:      text, attr,  6 reserved bytes */
#define FB_AUX_TEXT_SVGA_GROUP	                        8	/* 8-15: SVGA tileblit compatible modes */
#define FB_AUX_TEXT_SVGA_MASK	                        7	/* lower three bits says step */
#define FB_AUX_TEXT_SVGA_STEP2	                        8	/* SVGA text mode:  text, attr */
#define FB_AUX_TEXT_SVGA_STEP4	                        9	/* SVGA text mode:  text, attr,  2 reserved bytes */
#define FB_AUX_TEXT_SVGA_STEP8	                        10	/* SVGA text mode:  text, attr,  6 reserved bytes */
#define FB_AUX_TEXT_SVGA_STEP16	                        11	/* SVGA text mode:  text, attr, 14 reserved bytes */
#define FB_AUX_TEXT_SVGA_LAST	                        15	/* reserved up to 15 */

#define FB_AUX_VGA_PLANES_VGA4		                    0	/* 16 color planes (EGA/VGA) */
#define FB_AUX_VGA_PLANES_CFB4		                    1	/* CFB4 in planes (VGA) */
#define FB_AUX_VGA_PLANES_CFB8		                    2	/* CFB8 in planes (VGA) */

#define FB_VISUAL_MONO01		                        0	/* Monochr. 1=Black 0=White */
#define FB_VISUAL_MONO10		                        1	/* Monochr. 1=White 0=Black */
#define FB_VISUAL_TRUECOLOR		                        2	/* True color	*/
#define FB_VISUAL_PSEUDOCOLOR		                    3	/* Pseudo color (like atari) */
#define FB_VISUAL_DIRECTCOLOR		                    4	/* Direct color */
#define FB_VISUAL_STATIC_PSEUDOCOLOR	                5	/* Pseudo color readonly */
#define FB_VISUAL_FOURCC		                        6	/* Visual identified by a V4L2 FOURCC */

#define FB_ACCEL_NONE		                            0	/* no hardware accelerator	*/
#define FB_ACCEL_CIRRUS_ALPINE                          53	/* Cirrus Logic 543x/544x/5480	*/

#define FB_CAP_FOURCC		                            1	/* Device supports FOURCC-based formats */



struct fb_fix_screeninfo {
	char id[16];			                    /* identification string eg "TT Builtin"    */
	unsigned long smem_start;	                /* Start of frame buffer mem                */
					                            /* (physical address)                       */
	uint32_t smem_len;			                /* Length of frame buffer mem               */
	uint32_t type;			                    /* see FB_TYPE_*		                    */
	uint32_t type_aux;			                /* Interleave for interleaved Planes        */
	uint32_t visual;			                /* see FB_VISUAL_*		                    */ 
	uint16_t xpanstep;			                /* zero if no hardware panning              */
	uint16_t ypanstep;			                /* zero if no hardware panning              */
	uint16_t ywrapstep;		                    /* zero if no hardware ywrap                */
	uint32_t line_length;		                /* length of a line in bytes                */
	unsigned long mmio_start;	                /* Start of Memory Mapped I/O               */
					                            /* (physical address)                       */
	uint32_t mmio_len;			                /* Length of Memory Mapped I/O              */
	uint32_t accel;			                    /* Indicate to driver which	                */
					                            /*  specific chip/card we have	            */
	uint16_t capabilities;		                /* see FB_CAP_*			                    */
	uint16_t reserved[2];		                /* Reserved for future compatibility        */
};



struct fb_bitfield {
	uint32_t offset;			/* beginning of bitfield	            */
	uint32_t length;			/* length of bitfield		            */
	uint32_t msb_right;		    /* != 0 : Most significant bit is       */ 
					            /* right                                */ 
};

#define FB_NONSTD_HAM		                            1	/* Hold-And-Modify (HAM)        */
#define FB_NONSTD_REV_PIX_IN_B	                        2	/* order of pixels in each byte is reversed */

#define FB_ACTIVATE_NOW		                            0	/* set values immediately (or vbl)*/
#define FB_ACTIVATE_NXTOPEN	                            1	/* activate on next open	*/
#define FB_ACTIVATE_TEST	                            2	/* don't set, round up impossible */
#define FB_ACTIVATE_MASK                                15
                    

#define FB_ACTIVATE_VBL	                                16	/* activate values on next vbl  */
#define FB_CHANGE_CMAP_VBL                              32	/* change colormap on vbl	*/
#define FB_ACTIVATE_ALL	                                64	/* change all VCs on this fb	*/
#define FB_ACTIVATE_FORCE                               128	/* force apply even when no change*/
#define FB_ACTIVATE_INV_MODE                            256 /* invalidate videomode */

#define FB_ACCELF_TEXT		                            1	/* (OBSOLETE) see fb_info.flags and vc_mode */

#define FB_SYNC_HOR_HIGH_ACT	                        1	/* horizontal sync high active	*/
#define FB_SYNC_VERT_HIGH_ACT	                        2	/* vertical sync high active	*/
#define FB_SYNC_EXT		                                4	/* external sync		*/
#define FB_SYNC_COMP_HIGH_ACT	                        8	/* composite sync high active   */
#define FB_SYNC_BROADCAST	                            16	/* broadcast video timings      */
                    

#define FB_SYNC_ON_GREEN	                            32	/* sync on green */

#define FB_VMODE_NONINTERLACED                          0	/* non interlaced */
#define FB_VMODE_INTERLACED	                            1	/* interlaced	*/
#define FB_VMODE_DOUBLE		                            2	/* double scan */
#define FB_VMODE_ODD_FLD_FIRST	                        4	/* interlaced: top line first */
#define FB_VMODE_MASK		                            255

#define FB_VMODE_YWRAP		                            256	/* ywrap instead of panning     */
#define FB_VMODE_SMOOTH_XPAN	                        512	/* smooth xpan possible (internally used) */
#define FB_VMODE_CONUPDATE	                            512	/* don't update x/yoffset	*/


#define FB_ROTATE_UR      0
#define FB_ROTATE_CW      1
#define FB_ROTATE_UD      2
#define FB_ROTATE_CCW     3

#define PICOS2KHZ(a) (1000000000UL/(a))
#define KHZ2PICOS(a) (1000000000UL/(a))

struct fb_var_screeninfo {
	uint32_t xres;			            /* visible resolution		            */
	uint32_t yres;
	uint32_t xres_virtual;		        /* virtual resolution		            */
	uint32_t yres_virtual;
	uint32_t xoffset;			        /* offset from virtual to visible       */
	uint32_t yoffset;			        /* resolution			                */

	uint32_t bits_per_pixel;		    /* guess what			                */
	uint32_t grayscale;		            /* 0 = color, 1 = grayscale,	        */
					                    /* >1 = FOURCC			                */
	struct fb_bitfield red;		        /* bitfield in fb mem if true color,    */
	struct fb_bitfield green;	        /* else only length is significant      */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	        /* transparency			                */	

	uint32_t nonstd;			        /* != 0 Non standard pixel format       */

	uint32_t activate;			        /* see FB_ACTIVATE_*		            */

	uint32_t height;			        /* height of picture in mm              */
	uint32_t width;			            /* width of picture in mm               */

	uint32_t accel_flags;		        /* (OBSOLETE) see fb_info.flags         */


    uint32_t pixclock;			        /* pixel clock in ps (pico seconds)     */
	uint32_t left_margin;		        /* time from sync to picture	        */
	uint32_t right_margin;		        /* time from picture to sync	        */
	uint32_t upper_margin;		        /* time from sync to picture	        */
	uint32_t lower_margin;
	uint32_t hsync_len;		            /* length of horizontal sync	        */
	uint32_t vsync_len;		            /* length of vertical sync	            */
	uint32_t sync;			            /* see FB_SYNC_*		                */
	uint32_t vmode;			            /* see FB_VMODE_*		                */
	uint32_t rotate;			        /* angle we rotate counter clockwise    */
	uint32_t colorspace;		        /* colorspace for FOURCC-based modes    */
	uint32_t reserved[4];		        /* Reserved for future compatibility    */
};

struct fb_cmap {
	uint32_t start;			            /* First entry	                        */
	uint32_t len;			            /* Number of entries                    */
	uint16_t *red;			            /* Red values	                        */
	uint16_t *green;
	uint16_t *blue;
	uint16_t *transp;			        /* transparency, can be NULL            */
};

struct fb_con2fbmap {
	uint32_t console;
	uint32_t framebuffer;
};


#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3


enum {
	/* screen: unblanked, hsync: on,  vsync: on */
	FB_BLANK_UNBLANK       = VESA_NO_BLANKING,

	/* screen: blanked,   hsync: on,  vsync: on */
	FB_BLANK_NORMAL        = VESA_NO_BLANKING + 1,

	/* screen: blanked,   hsync: on,  vsync: off */
	FB_BLANK_VSYNC_SUSPEND = VESA_VSYNC_SUSPEND + 1,

	/* screen: blanked,   hsync: off, vsync: on */
	FB_BLANK_HSYNC_SUSPEND = VESA_HSYNC_SUSPEND + 1,

	/* screen: blanked,   hsync: off, vsync: off */
	FB_BLANK_POWERDOWN     = VESA_POWERDOWN + 1
};

#define FB_VBLANK_VBLANKING	                0x001	/* currently in a vertical blank */
#define FB_VBLANK_HBLANKING	                0x002	/* currently in a horizontal blank */
#define FB_VBLANK_HAVE_VBLANK	            0x004	/* vertical blanks can be detected */
#define FB_VBLANK_HAVE_HBLANK	            0x008	/* horizontal blanks can be detected */
#define FB_VBLANK_HAVE_COUNT	            0x010	/* global retrace counter is available */
#define FB_VBLANK_HAVE_VCOUNT	            0x020	/* the vcount field is valid */
#define FB_VBLANK_HAVE_HCOUNT	            0x040	/* the hcount field is valid */
#define FB_VBLANK_VSYNCING	                0x080	/* currently in a vsync */
#define FB_VBLANK_HAVE_VSYNC	            0x100	/* verical syncs can be detected */

struct fb_vblank {
	uint32_t flags;			    /* FB_VBLANK flags */
	uint32_t count;			    /* counter of retraces since boot */
	uint32_t vcount;			/* current scanline position */
	uint32_t hcount;			/* current scandot position */
	uint32_t reserved[4];		/* reserved for future compatibility */
};

/* Internal HW accel */
#define ROP_COPY 0
#define ROP_XOR  1

struct fb_copyarea {
	uint32_t dx;
	uint32_t dy;
	uint32_t width;
	uint32_t height;
	uint32_t sx;
	uint32_t sy;
};

struct fb_fillrect {
	uint32_t dx;	            /* screen-relative */
	uint32_t dy;
	uint32_t width;
	uint32_t height;
	uint32_t color;
	uint32_t rop;
};

struct fb_image {
	uint32_t dx;		        /* Where to place image */
	uint32_t dy;
	uint32_t width;		        /* Size of image */
	uint32_t height;
	uint32_t fg_color;		    /* Only used when a mono bitmap */
	uint32_t bg_color;
	uint8_t  depth;		        /* Depth of the image */
	const char *data;	        /* Pointer to image data */
	struct fb_cmap cmap;	    /* color map info */
};



#define FB_CUR_SETIMAGE 0x01
#define FB_CUR_SETPOS   0x02
#define FB_CUR_SETHOT   0x04
#define FB_CUR_SETCMAP  0x08
#define FB_CUR_SETSHAPE 0x10
#define FB_CUR_SETSIZE	0x20
#define FB_CUR_SETALL   0xFF

struct fbcurpos {
	uint16_t x, y;
};

struct fb_cursor {
	uint16_t set;		        /* what to set */
	uint16_t enable;		    /* cursor on/off */
	uint16_t rop;		        /* bitop operation */
	const char *mask;	        /* cursor mask bits */
	struct fbcurpos hot;	    /* cursor hot spot */
	struct fb_image	image;	    /* Cursor image */
};

#ifdef CONFIG_FB_BACKLIGHT
#define FB_BACKLIGHT_LEVELS	128
#define FB_BACKLIGHT_MAX	0xFF
#endif


#endif
#endif