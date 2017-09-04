#include <aplus.h>
#include <aplus/base.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/intr.h>
#include <aplus/input.h>
#include <aplus/event.h>
#include <libc.h>

MODULE_NAME("pc/input/ps2");
MODULE_DEPS("arch/x86,sys/event");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#    if defined(__i386__)
#        include <arch/i386/i386.h>
#    elif defined(__x86_64__)
#        include <arch/x86_64/x86_64.h>
#    endif


#define PS2_DATA        0x60
#define PS2_CTRL        0x64
#define PS2_ACK            0xFA
#define PS2_RESEND        0xFE



#define PS2_WAIT                                        \
    {                                                   \
        int t = 100000;                                 \
        while((inb(PS2_CTRL) & 0x02) && t > 0)          \
            t--;                                        \
    }

#define PS2_WAIT_0                                      \
    {                                                   \
        int t = 100000;                                 \
        while(!(inb(PS2_CTRL) & 0x01) && t > 0)         \
            t--;                                        \
    }




static uint16_t ps2_codemap[] = {
  [0x1] = 0x1, /* xtkbd:1 -> linux:1 (KEY_ESC) -> linux:1 (KEY_ESC) */
  [0x2] = 0x2, /* xtkbd:2 -> linux:2 (KEY_1) -> linux:2 (KEY_1) */
  [0x3] = 0x3, /* xtkbd:3 -> linux:3 (KEY_2) -> linux:3 (KEY_2) */
  [0x4] = 0x4, /* xtkbd:4 -> linux:4 (KEY_3) -> linux:4 (KEY_3) */
  [0x5] = 0x5, /* xtkbd:5 -> linux:5 (KEY_4) -> linux:5 (KEY_4) */
  [0x6] = 0x6, /* xtkbd:6 -> linux:6 (KEY_5) -> linux:6 (KEY_5) */
  [0x7] = 0x7, /* xtkbd:7 -> linux:7 (KEY_6) -> linux:7 (KEY_6) */
  [0x8] = 0x8, /* xtkbd:8 -> linux:8 (KEY_7) -> linux:8 (KEY_7) */
  [0x9] = 0x9, /* xtkbd:9 -> linux:9 (KEY_8) -> linux:9 (KEY_8) */
  [0xa] = 0xa, /* xtkbd:10 -> linux:10 (KEY_9) -> linux:10 (KEY_9) */
  [0xb] = 0xb, /* xtkbd:11 -> linux:11 (KEY_0) -> linux:11 (KEY_0) */
  [0xc] = 0xc, /* xtkbd:12 -> linux:12 (KEY_MINUS) -> linux:12 (KEY_MINUS) */
  [0xd] = 0xd, /* xtkbd:13 -> linux:13 (KEY_EQUAL) -> linux:13 (KEY_EQUAL) */
  [0xe] = 0xe, /* xtkbd:14 -> linux:14 (KEY_BACKSPACE) -> linux:14 (KEY_BACKSPACE) */
  [0xf] = 0xf, /* xtkbd:15 -> linux:15 (KEY_TAB) -> linux:15 (KEY_TAB) */
  [0x10] = 0x10, /* xtkbd:16 -> linux:16 (KEY_Q) -> linux:16 (KEY_Q) */
  [0x11] = 0x11, /* xtkbd:17 -> linux:17 (KEY_W) -> linux:17 (KEY_W) */
  [0x12] = 0x12, /* xtkbd:18 -> linux:18 (KEY_E) -> linux:18 (KEY_E) */
  [0x13] = 0x13, /* xtkbd:19 -> linux:19 (KEY_R) -> linux:19 (KEY_R) */
  [0x14] = 0x14, /* xtkbd:20 -> linux:20 (KEY_T) -> linux:20 (KEY_T) */
  [0x15] = 0x15, /* xtkbd:21 -> linux:21 (KEY_Y) -> linux:21 (KEY_Y) */
  [0x16] = 0x16, /* xtkbd:22 -> linux:22 (KEY_U) -> linux:22 (KEY_U) */
  [0x17] = 0x17, /* xtkbd:23 -> linux:23 (KEY_I) -> linux:23 (KEY_I) */
  [0x18] = 0x18, /* xtkbd:24 -> linux:24 (KEY_O) -> linux:24 (KEY_O) */
  [0x19] = 0x19, /* xtkbd:25 -> linux:25 (KEY_P) -> linux:25 (KEY_P) */
  [0x1a] = 0x1a, /* xtkbd:26 -> linux:26 (KEY_LEFTBRACE) -> linux:26 (KEY_LEFTBRACE) */
  [0x1b] = 0x1b, /* xtkbd:27 -> linux:27 (KEY_RIGHTBRACE) -> linux:27 (KEY_RIGHTBRACE) */
  [0x1c] = 0x1c, /* xtkbd:28 -> linux:28 (KEY_ENTER) -> linux:28 (KEY_ENTER) */
  [0x1d] = 0x1d, /* xtkbd:29 -> linux:29 (KEY_LEFTCTRL) -> linux:29 (KEY_LEFTCTRL) */
  [0x1e] = 0x1e, /* xtkbd:30 -> linux:30 (KEY_A) -> linux:30 (KEY_A) */
  [0x1f] = 0x1f, /* xtkbd:31 -> linux:31 (KEY_S) -> linux:31 (KEY_S) */
  [0x20] = 0x20, /* xtkbd:32 -> linux:32 (KEY_D) -> linux:32 (KEY_D) */
  [0x21] = 0x21, /* xtkbd:33 -> linux:33 (KEY_F) -> linux:33 (KEY_F) */
  [0x22] = 0x22, /* xtkbd:34 -> linux:34 (KEY_G) -> linux:34 (KEY_G) */
  [0x23] = 0x23, /* xtkbd:35 -> linux:35 (KEY_H) -> linux:35 (KEY_H) */
  [0x24] = 0x24, /* xtkbd:36 -> linux:36 (KEY_J) -> linux:36 (KEY_J) */
  [0x25] = 0x25, /* xtkbd:37 -> linux:37 (KEY_K) -> linux:37 (KEY_K) */
  [0x26] = 0x26, /* xtkbd:38 -> linux:38 (KEY_L) -> linux:38 (KEY_L) */
  [0x27] = 0x27, /* xtkbd:39 -> linux:39 (KEY_SEMICOLON) -> linux:39 (KEY_SEMICOLON) */
  [0x28] = 0x28, /* xtkbd:40 -> linux:40 (KEY_APOSTROPHE) -> linux:40 (KEY_APOSTROPHE) */
  [0x29] = 0x29, /* xtkbd:41 -> linux:41 (KEY_GRAVE) -> linux:41 (KEY_GRAVE) */
  [0x2a] = 0x2a, /* xtkbd:42 -> linux:42 (KEY_LEFTSHIFT) -> linux:42 (KEY_LEFTSHIFT) */
  [0x2b] = 0x2b, /* xtkbd:43 -> linux:43 (KEY_BACKSLASH) -> linux:43 (KEY_BACKSLASH) */
  [0x2c] = 0x2c, /* xtkbd:44 -> linux:44 (KEY_Z) -> linux:44 (KEY_Z) */
  [0x2d] = 0x2d, /* xtkbd:45 -> linux:45 (KEY_X) -> linux:45 (KEY_X) */
  [0x2e] = 0x2e, /* xtkbd:46 -> linux:46 (KEY_C) -> linux:46 (KEY_C) */
  [0x2f] = 0x2f, /* xtkbd:47 -> linux:47 (KEY_V) -> linux:47 (KEY_V) */
  [0x30] = 0x30, /* xtkbd:48 -> linux:48 (KEY_B) -> linux:48 (KEY_B) */
  [0x31] = 0x31, /* xtkbd:49 -> linux:49 (KEY_N) -> linux:49 (KEY_N) */
  [0x32] = 0x32, /* xtkbd:50 -> linux:50 (KEY_M) -> linux:50 (KEY_M) */
  [0x33] = 0x33, /* xtkbd:51 -> linux:51 (KEY_COMMA) -> linux:51 (KEY_COMMA) */
  [0x34] = 0x34, /* xtkbd:52 -> linux:52 (KEY_DOT) -> linux:52 (KEY_DOT) */
  [0x35] = 0x35, /* xtkbd:53 -> linux:53 (KEY_SLASH) -> linux:53 (KEY_SLASH) */
  [0x36] = 0x36, /* xtkbd:54 -> linux:54 (KEY_RIGHTSHIFT) -> linux:54 (KEY_RIGHTSHIFT) */
  [0x37] = 0x37, /* xtkbd:55 -> linux:55 (KEY_KPASTERISK) -> linux:55 (KEY_KPASTERISK) */
  [0x38] = 0x38, /* xtkbd:56 -> linux:56 (KEY_LEFTALT) -> linux:56 (KEY_LEFTALT) */
  [0x39] = 0x39, /* xtkbd:57 -> linux:57 (KEY_SPACE) -> linux:57 (KEY_SPACE) */
  [0x3a] = 0x3a, /* xtkbd:58 -> linux:58 (KEY_CAPSLOCK) -> linux:58 (KEY_CAPSLOCK) */
  [0x3b] = 0x3b, /* xtkbd:59 -> linux:59 (KEY_F1) -> linux:59 (KEY_F1) */
  [0x3c] = 0x3c, /* xtkbd:60 -> linux:60 (KEY_F2) -> linux:60 (KEY_F2) */
  [0x3d] = 0x3d, /* xtkbd:61 -> linux:61 (KEY_F3) -> linux:61 (KEY_F3) */
  [0x3e] = 0x3e, /* xtkbd:62 -> linux:62 (KEY_F4) -> linux:62 (KEY_F4) */
  [0x3f] = 0x3f, /* xtkbd:63 -> linux:63 (KEY_F5) -> linux:63 (KEY_F5) */
  [0x40] = 0x40, /* xtkbd:64 -> linux:64 (KEY_F6) -> linux:64 (KEY_F6) */
  [0x41] = 0x41, /* xtkbd:65 -> linux:65 (KEY_F7) -> linux:65 (KEY_F7) */
  [0x42] = 0x42, /* xtkbd:66 -> linux:66 (KEY_F8) -> linux:66 (KEY_F8) */
  [0x43] = 0x43, /* xtkbd:67 -> linux:67 (KEY_F9) -> linux:67 (KEY_F9) */
  [0x44] = 0x44, /* xtkbd:68 -> linux:68 (KEY_F10) -> linux:68 (KEY_F10) */
  [0x45] = 0x45, /* xtkbd:69 -> linux:69 (KEY_NUMLOCK) -> linux:69 (KEY_NUMLOCK) */
  [0x46] = 0x46, /* xtkbd:70 -> linux:70 (KEY_SCROLLLOCK) -> linux:70 (KEY_SCROLLLOCK) */
  [0x47] = 0x47, /* xtkbd:71 -> linux:71 (KEY_KP7) -> linux:71 (KEY_KP7) */
  [0x48] = 0x48, /* xtkbd:72 -> linux:72 (KEY_KP8) -> linux:72 (KEY_KP8) */
  [0x49] = 0x49, /* xtkbd:73 -> linux:73 (KEY_KP9) -> linux:73 (KEY_KP9) */
  [0x4a] = 0x4a, /* xtkbd:74 -> linux:74 (KEY_KPMINUS) -> linux:74 (KEY_KPMINUS) */
  [0x4b] = 0x4b, /* xtkbd:75 -> linux:75 (KEY_KP4) -> linux:75 (KEY_KP4) */
  [0x4c] = 0x4c, /* xtkbd:76 -> linux:76 (KEY_KP5) -> linux:76 (KEY_KP5) */
  [0x4d] = 0x4d, /* xtkbd:77 -> linux:77 (KEY_KP6) -> linux:77 (KEY_KP6) */
  [0x4e] = 0x4e, /* xtkbd:78 -> linux:78 (KEY_KPPLUS) -> linux:78 (KEY_KPPLUS) */
  [0x4f] = 0x4f, /* xtkbd:79 -> linux:79 (KEY_KP1) -> linux:79 (KEY_KP1) */
  [0x50] = 0x50, /* xtkbd:80 -> linux:80 (KEY_KP2) -> linux:80 (KEY_KP2) */
  [0x51] = 0x51, /* xtkbd:81 -> linux:81 (KEY_KP3) -> linux:81 (KEY_KP3) */
  [0x52] = 0x52, /* xtkbd:82 -> linux:82 (KEY_KP0) -> linux:82 (KEY_KP0) */
  [0x53] = 0x53, /* xtkbd:83 -> linux:83 (KEY_KPDOT) -> linux:83 (KEY_KPDOT) */
  [0x54] = 0x63, /* xtkbd:84 -> linux:99 (KEY_SYSRQ) -> linux:99 (KEY_SYSRQ) */
  [0x55] = 0xba, /* xtkbd:85 -> linux:186 (KEY_F16) -> linux:186 (KEY_F16) */
  [0x56] = 0x56, /* xtkbd:86 -> linux:86 (KEY_102ND) -> linux:86 (KEY_102ND) */
  [0x57] = 0x57, /* xtkbd:87 -> linux:87 (KEY_F11) -> linux:87 (KEY_F11) */
  [0x58] = 0x58, /* xtkbd:88 -> linux:88 (KEY_F12) -> linux:88 (KEY_F12) */
  [0x59] = 0x75, /* xtkbd:89 -> linux:117 (KEY_KPEQUAL) -> linux:117 (KEY_KPEQUAL) */
  [0x5a] = 0xbe, /* xtkbd:90 -> linux:190 (KEY_F20) -> linux:190 (KEY_F20) */
  [0x5b] = 0x65, /* xtkbd:91 -> linux:101 (KEY_LINEFEED) -> linux:101 (KEY_LINEFEED) */
  [0x5c] = 0x5f, /* xtkbd:92 -> linux:95 (KEY_KPJPCOMMA) -> linux:95 (KEY_KPJPCOMMA) */
  [0x5d] = 0xb7, /* xtkbd:93 -> linux:183 (KEY_F13) -> linux:183 (KEY_F13) */
  [0x5e] = 0xb8, /* xtkbd:94 -> linux:184 (KEY_F14) -> linux:184 (KEY_F14) */
  [0x5f] = 0xb9, /* xtkbd:95 -> linux:185 (KEY_F15) -> linux:185 (KEY_F15) */
  [0x63] = 0xa9, /* xtkbd:99 -> linux:169 (KEY_PHONE) -> linux:169 (KEY_PHONE) */
  [0x64] = 0x86, /* xtkbd:100 -> linux:134 (KEY_OPEN) -> linux:134 (KEY_OPEN) */
  [0x65] = 0x87, /* xtkbd:101 -> linux:135 (KEY_PASTE) -> linux:135 (KEY_PASTE) */
  [0x66] = 0x8d, /* xtkbd:102 -> linux:141 (KEY_SETUP) -> linux:141 (KEY_SETUP) */
  [0x67] = 0x90, /* xtkbd:103 -> linux:144 (KEY_FILE) -> linux:144 (KEY_FILE) */
  [0x68] = 0x91, /* xtkbd:104 -> linux:145 (KEY_SENDFILE) -> linux:145 (KEY_SENDFILE) */
  [0x69] = 0x92, /* xtkbd:105 -> linux:146 (KEY_DELETEFILE) -> linux:146 (KEY_DELETEFILE) */
  [0x6a] = 0x97, /* xtkbd:106 -> linux:151 (KEY_MSDOS) -> linux:151 (KEY_MSDOS) */
  [0x6b] = 0x99, /* xtkbd:107 -> linux:153 (KEY_DIRECTION) -> linux:153 (KEY_DIRECTION) */
  [0x6c] = 0xa1, /* xtkbd:108 -> linux:161 (KEY_EJECTCD) -> linux:161 (KEY_EJECTCD) */
  [0x6d] = 0xc1, /* xtkbd:109 -> linux:193 (KEY_F23) -> linux:193 (KEY_F23) */
  [0x6f] = 0xc2, /* xtkbd:111 -> linux:194 (KEY_F24) -> linux:194 (KEY_F24) */
  [0x70] = 0xaa, /* xtkbd:112 -> linux:170 (KEY_ISO) -> linux:170 (KEY_ISO) */
  [0x71] = 0xae, /* xtkbd:113 -> linux:174 (KEY_EXIT) -> linux:174 (KEY_EXIT) */
  [0x72] = 0xaf, /* xtkbd:114 -> linux:175 (KEY_MOVE) -> linux:175 (KEY_MOVE) */
  [0x73] = 0x59, /* xtkbd:115 -> linux:89 (KEY_RO) -> linux:89 (KEY_RO) */
  [0x74] = 0xbf, /* xtkbd:116 -> linux:191 (KEY_F21) -> linux:191 (KEY_F21) */
  [0x75] = 0xb1, /* xtkbd:117 -> linux:177 (KEY_SCROLLUP) -> linux:177 (KEY_SCROLLUP) */
  [0x76] = 0x55, /* xtkbd:118 -> linux:85 (KEY_ZENKAKUHANKAKU) -> linux:85 (KEY_ZENKAKUHANKAKU) */
  [0x77] = 0x5b, /* xtkbd:119 -> linux:91 (KEY_HIRAGANA) -> linux:91 (KEY_HIRAGANA) */
  [0x78] = 0x5a, /* xtkbd:120 -> linux:90 (KEY_KATAKANA) -> linux:90 (KEY_KATAKANA) */
  [0x79] = 0x5c, /* xtkbd:121 -> linux:92 (KEY_HENKAN) -> linux:92 (KEY_HENKAN) */
  [0x7b] = 0x5e, /* xtkbd:123 -> linux:94 (KEY_MUHENKAN) -> linux:94 (KEY_MUHENKAN) */
  [0x7d] = 0x7c, /* xtkbd:125 -> linux:124 (KEY_YEN) -> linux:124 (KEY_YEN) */
  [0x7e] = 0x79, /* xtkbd:126 -> linux:121 (KEY_KPCOMMA) -> linux:121 (KEY_KPCOMMA) */
  [0x101] = 0xab, /* xtkbd:257 -> linux:171 (KEY_CONFIG) -> linux:171 (KEY_CONFIG) */
  [0x102] = 0x96, /* xtkbd:258 -> linux:150 (KEY_WWW) -> linux:150 (KEY_WWW) */
  [0x103] = 0xbb, /* xtkbd:259 -> linux:187 (KEY_F17) -> linux:187 (KEY_F17) */
  [0x104] = 0xbd, /* xtkbd:260 -> linux:189 (KEY_F19) -> linux:189 (KEY_F19) */
  [0x105] = 0x81, /* xtkbd:261 -> linux:129 (KEY_AGAIN) -> linux:129 (KEY_AGAIN) */
  [0x106] = 0x82, /* xtkbd:262 -> linux:130 (KEY_PROPS) -> linux:130 (KEY_PROPS) */
  [0x107] = 0x83, /* xtkbd:263 -> linux:131 (KEY_UNDO) -> linux:131 (KEY_UNDO) */
  [0x108] = 0xb0, /* xtkbd:264 -> linux:176 (KEY_EDIT) -> linux:176 (KEY_EDIT) */
  [0x109] = 0xb5, /* xtkbd:265 -> linux:181 (KEY_NEW) -> linux:181 (KEY_NEW) */
  [0x10a] = 0xb6, /* xtkbd:266 -> linux:182 (KEY_REDO) -> linux:182 (KEY_REDO) */
  [0x10b] = 0x78, /* xtkbd:267 -> linux:120 (KEY_SCALE) -> linux:120 (KEY_SCALE) */
  [0x10c] = 0x84, /* xtkbd:268 -> linux:132 (KEY_FRONT) -> linux:132 (KEY_FRONT) */
  [0x10d] = 0x7b, /* xtkbd:269 -> linux:123 (KEY_HANJA) -> linux:123 (KEY_HANJA) */
  [0x10e] = 0xe9, /* xtkbd:270 -> linux:233 (KEY_FORWARDMAIL) -> linux:233 (KEY_FORWARDMAIL) */
  [0x10f] = 0xb2, /* xtkbd:271 -> linux:178 (KEY_SCROLLDOWN) -> linux:178 (KEY_SCROLLDOWN) */
  [0x110] = 0xa5, /* xtkbd:272 -> linux:165 (KEY_PREVIOUSSONG) -> linux:165 (KEY_PREVIOUSSONG) */
  [0x112] = 0x98, /* xtkbd:274 -> linux:152 (KEY_SCREENLOCK) -> linux:152 (KEY_SCREENLOCK) */
  [0x113] = 0x93, /* xtkbd:275 -> linux:147 (KEY_XFER) -> linux:147 (KEY_XFER) */
  [0x114] = 0xde, /* xtkbd:276 -> linux:222 (KEY_ALTERASE) -> linux:222 (KEY_ALTERASE) */
  [0x115] = 0xc3, /* xtkbd:277 -> linux:195 (unnamed) -> linux:195 (unnamed) */
  [0x116] = 0xc4, /* xtkbd:278 -> linux:196 (unnamed) -> linux:196 (unnamed) */
  [0x117] = 0x95, /* xtkbd:279 -> linux:149 (KEY_PROG2) -> linux:149 (KEY_PROG2) */
  [0x118] = 0xa8, /* xtkbd:280 -> linux:168 (KEY_REWIND) -> linux:168 (KEY_REWIND) */
  [0x119] = 0xa3, /* xtkbd:281 -> linux:163 (KEY_NEXTSONG) -> linux:163 (KEY_NEXTSONG) */
  [0x11a] = 0xc5, /* xtkbd:282 -> linux:197 (unnamed) -> linux:197 (unnamed) */
  [0x11b] = 0xc6, /* xtkbd:283 -> linux:198 (unnamed) -> linux:198 (unnamed) */
  [0x11c] = 0x60, /* xtkbd:284 -> linux:96 (KEY_KPENTER) -> linux:96 (KEY_KPENTER) */
  [0x11d] = 0x61, /* xtkbd:285 -> linux:97 (KEY_RIGHTCTRL) -> linux:97 (KEY_RIGHTCTRL) */
  [0x11e] = 0x8b, /* xtkbd:286 -> linux:139 (KEY_MENU) -> linux:139 (KEY_MENU) */
  [0x11f] = 0x94, /* xtkbd:287 -> linux:148 (KEY_PROG1) -> linux:148 (KEY_PROG1) */
  [0x120] = 0x71, /* xtkbd:288 -> linux:113 (KEY_MUTE) -> linux:113 (KEY_MUTE) */
  [0x121] = 0x8c, /* xtkbd:289 -> linux:140 (KEY_CALC) -> linux:140 (KEY_CALC) */
  [0x122] = 0xa4, /* xtkbd:290 -> linux:164 (KEY_PLAYPAUSE) -> linux:164 (KEY_PLAYPAUSE) */
  [0x123] = 0xa0, /* xtkbd:291 -> linux:160 (KEY_CLOSECD) -> linux:160 (KEY_CLOSECD) */
  [0x124] = 0xa6, /* xtkbd:292 -> linux:166 (KEY_STOPCD) -> linux:166 (KEY_STOPCD) */
  [0x125] = 0xcd, /* xtkbd:293 -> linux:205 (KEY_SUSPEND) -> linux:205 (KEY_SUSPEND) */
  [0x126] = 0x9a, /* xtkbd:294 -> linux:154 (KEY_CYCLEWINDOWS) -> linux:154 (KEY_CYCLEWINDOWS) */
  [0x127] = 0xc7, /* xtkbd:295 -> linux:199 (unnamed) -> linux:199 (unnamed) */
  [0x128] = 0xc8, /* xtkbd:296 -> linux:200 (KEY_PLAYCD) -> linux:200 (KEY_PLAYCD) */
  [0x129] = 0xc9, /* xtkbd:297 -> linux:201 (KEY_PAUSECD) -> linux:201 (KEY_PAUSECD) */
  [0x12b] = 0xca, /* xtkbd:299 -> linux:202 (KEY_PROG3) -> linux:202 (KEY_PROG3) */
  [0x12c] = 0xcb, /* xtkbd:300 -> linux:203 (KEY_PROG4) -> linux:203 (KEY_PROG4) */
  [0x12d] = 0xcc, /* xtkbd:301 -> linux:204 (KEY_DASHBOARD) -> linux:204 (KEY_DASHBOARD) */
  [0x12e] = 0x72, /* xtkbd:302 -> linux:114 (KEY_VOLUMEDOWN) -> linux:114 (KEY_VOLUMEDOWN) */
  [0x12f] = 0xce, /* xtkbd:303 -> linux:206 (KEY_CLOSE) -> linux:206 (KEY_CLOSE) */
  [0x130] = 0x73, /* xtkbd:304 -> linux:115 (KEY_VOLUMEUP) -> linux:115 (KEY_VOLUMEUP) */
  [0x131] = 0xa7, /* xtkbd:305 -> linux:167 (KEY_RECORD) -> linux:167 (KEY_RECORD) */
  [0x132] = 0xac, /* xtkbd:306 -> linux:172 (KEY_HOMEPAGE) -> linux:172 (KEY_HOMEPAGE) */
  [0x133] = 0xcf, /* xtkbd:307 -> linux:207 (KEY_PLAY) -> linux:207 (KEY_PLAY) */
  [0x134] = 0xd0, /* xtkbd:308 -> linux:208 (KEY_FASTFORWARD) -> linux:208 (KEY_FASTFORWARD) */
  [0x135] = 0x62, /* xtkbd:309 -> linux:98 (KEY_KPSLASH) -> linux:98 (KEY_KPSLASH) */
  [0x136] = 0xd1, /* xtkbd:310 -> linux:209 (KEY_BASSBOOST) -> linux:209 (KEY_BASSBOOST) */
  [0x138] = 0x64, /* xtkbd:312 -> linux:100 (KEY_RIGHTALT) -> linux:100 (KEY_RIGHTALT) */
  [0x139] = 0xd2, /* xtkbd:313 -> linux:210 (KEY_PRINT) -> linux:210 (KEY_PRINT) */
  [0x13a] = 0xd3, /* xtkbd:314 -> linux:211 (KEY_HP) -> linux:211 (KEY_HP) */
  [0x13b] = 0xd4, /* xtkbd:315 -> linux:212 (KEY_CAMERA) -> linux:212 (KEY_CAMERA) */
  [0x13c] = 0x89, /* xtkbd:316 -> linux:137 (KEY_CUT) -> linux:137 (KEY_CUT) */
  [0x13d] = 0xd5, /* xtkbd:317 -> linux:213 (KEY_SOUND) -> linux:213 (KEY_SOUND) */
  [0x13e] = 0xd6, /* xtkbd:318 -> linux:214 (KEY_QUESTION) -> linux:214 (KEY_QUESTION) */
  [0x13f] = 0xd7, /* xtkbd:319 -> linux:215 (KEY_EMAIL) -> linux:215 (KEY_EMAIL) */
  [0x140] = 0xd8, /* xtkbd:320 -> linux:216 (KEY_CHAT) -> linux:216 (KEY_CHAT) */
  [0x141] = 0x88, /* xtkbd:321 -> linux:136 (KEY_FIND) -> linux:136 (KEY_FIND) */
  [0x142] = 0xda, /* xtkbd:322 -> linux:218 (KEY_CONNECT) -> linux:218 (KEY_CONNECT) */
  [0x143] = 0xdb, /* xtkbd:323 -> linux:219 (KEY_FINANCE) -> linux:219 (KEY_FINANCE) */
  [0x144] = 0xdc, /* xtkbd:324 -> linux:220 (KEY_SPORT) -> linux:220 (KEY_SPORT) */
  [0x145] = 0xdd, /* xtkbd:325 -> linux:221 (KEY_SHOP) -> linux:221 (KEY_SHOP) */
  [0x146] = 0x77, /* xtkbd:326 -> linux:119 (KEY_PAUSE) -> linux:119 (KEY_PAUSE) */
  [0x147] = 0x66, /* xtkbd:327 -> linux:102 (KEY_HOME) -> linux:102 (KEY_HOME) */
  [0x148] = 0x67, /* xtkbd:328 -> linux:103 (KEY_UP) -> linux:103 (KEY_UP) */
  [0x149] = 0x68, /* xtkbd:329 -> linux:104 (KEY_PAGEUP) -> linux:104 (KEY_PAGEUP) */
  [0x14a] = 0xdf, /* xtkbd:330 -> linux:223 (KEY_CANCEL) -> linux:223 (KEY_CANCEL) */
  [0x14b] = 0x69, /* xtkbd:331 -> linux:105 (KEY_LEFT) -> linux:105 (KEY_LEFT) */
  [0x14c] = 0xe0, /* xtkbd:332 -> linux:224 (KEY_BRIGHTNESSDOWN) -> linux:224 (KEY_BRIGHTNESSDOWN) */
  [0x14d] = 0x6a, /* xtkbd:333 -> linux:106 (KEY_RIGHT) -> linux:106 (KEY_RIGHT) */
  [0x14e] = 0x76, /* xtkbd:334 -> linux:118 (KEY_KPPLUSMINUS) -> linux:118 (KEY_KPPLUSMINUS) */
  [0x14f] = 0x6b, /* xtkbd:335 -> linux:107 (KEY_END) -> linux:107 (KEY_END) */
  [0x150] = 0x6c, /* xtkbd:336 -> linux:108 (KEY_DOWN) -> linux:108 (KEY_DOWN) */
  [0x151] = 0x6d, /* xtkbd:337 -> linux:109 (KEY_PAGEDOWN) -> linux:109 (KEY_PAGEDOWN) */
  [0x152] = 0x6e, /* xtkbd:338 -> linux:110 (KEY_INSERT) -> linux:110 (KEY_INSERT) */
  [0x153] = 0x6f, /* xtkbd:339 -> linux:111 (KEY_DELETE) -> linux:111 (KEY_DELETE) */
  [0x154] = 0xe1, /* xtkbd:340 -> linux:225 (KEY_BRIGHTNESSUP) -> linux:225 (KEY_BRIGHTNESSUP) */
  [0x155] = 0xea, /* xtkbd:341 -> linux:234 (KEY_SAVE) -> linux:234 (KEY_SAVE) */
  [0x156] = 0xe3, /* xtkbd:342 -> linux:227 (KEY_SWITCHVIDEOMODE) -> linux:227 (KEY_SWITCHVIDEOMODE) */
  [0x157] = 0xe4, /* xtkbd:343 -> linux:228 (KEY_KBDILLUMTOGGLE) -> linux:228 (KEY_KBDILLUMTOGGLE) */
  [0x158] = 0xe5, /* xtkbd:344 -> linux:229 (KEY_KBDILLUMDOWN) -> linux:229 (KEY_KBDILLUMDOWN) */
  [0x159] = 0xe6, /* xtkbd:345 -> linux:230 (KEY_KBDILLUMUP) -> linux:230 (KEY_KBDILLUMUP) */
  [0x15a] = 0xe7, /* xtkbd:346 -> linux:231 (KEY_SEND) -> linux:231 (KEY_SEND) */
  [0x15b] = 0x7d, /* xtkbd:347 -> linux:125 (KEY_LEFTMETA) -> linux:125 (KEY_LEFTMETA) */
  [0x15c] = 0x7e, /* xtkbd:348 -> linux:126 (KEY_RIGHTMETA) -> linux:126 (KEY_RIGHTMETA) */
  [0x15d] = 0x7f, /* xtkbd:349 -> linux:127 (KEY_COMPOSE) -> linux:127 (KEY_COMPOSE) */
  [0x15e] = 0x74, /* xtkbd:350 -> linux:116 (KEY_POWER) -> linux:116 (KEY_POWER) */
  [0x15f] = 0x8e, /* xtkbd:351 -> linux:142 (KEY_SLEEP) -> linux:142 (KEY_SLEEP) */
  [0x163] = 0x8f, /* xtkbd:355 -> linux:143 (KEY_WAKEUP) -> linux:143 (KEY_WAKEUP) */
  [0x164] = 0xe8, /* xtkbd:356 -> linux:232 (KEY_REPLY) -> linux:232 (KEY_REPLY) */
  [0x165] = 0xd9, /* xtkbd:357 -> linux:217 (KEY_SEARCH) -> linux:217 (KEY_SEARCH) */
  [0x166] = 0x9c, /* xtkbd:358 -> linux:156 (KEY_BOOKMARKS) -> linux:156 (KEY_BOOKMARKS) */
  [0x167] = 0xad, /* xtkbd:359 -> linux:173 (KEY_REFRESH) -> linux:173 (KEY_REFRESH) */
  [0x168] = 0x80, /* xtkbd:360 -> linux:128 (KEY_STOP) -> linux:128 (KEY_STOP) */
  [0x169] = 0x9f, /* xtkbd:361 -> linux:159 (KEY_FORWARD) -> linux:159 (KEY_FORWARD) */
  [0x16a] = 0x9e, /* xtkbd:362 -> linux:158 (KEY_BACK) -> linux:158 (KEY_BACK) */
  [0x16b] = 0x9d, /* xtkbd:363 -> linux:157 (KEY_COMPUTER) -> linux:157 (KEY_COMPUTER) */
  [0x16c] = 0x9b, /* xtkbd:364 -> linux:155 (KEY_MAIL) -> linux:155 (KEY_MAIL) */
  [0x16d] = 0xe2, /* xtkbd:365 -> linux:226 (KEY_MEDIA) -> linux:226 (KEY_MEDIA) */
  [0x16f] = 0x70, /* xtkbd:367 -> linux:112 (KEY_MACRO) -> linux:112 (KEY_MACRO) */
  [0x170] = 0xeb, /* xtkbd:368 -> linux:235 (KEY_DOCUMENTS) -> linux:235 (KEY_DOCUMENTS) */
  [0x171] = 0xec, /* xtkbd:369 -> linux:236 (KEY_BATTERY) -> linux:236 (KEY_BATTERY) */
  [0x172] = 0xed, /* xtkbd:370 -> linux:237 (KEY_BLUETOOTH) -> linux:237 (KEY_BLUETOOTH) */
  [0x173] = 0xee, /* xtkbd:371 -> linux:238 (KEY_WLAN) -> linux:238 (KEY_WLAN) */
  [0x174] = 0xef, /* xtkbd:372 -> linux:239 (KEY_UWB) -> linux:239 (KEY_UWB) */
  [0x175] = 0x8a, /* xtkbd:373 -> linux:138 (KEY_HELP) -> linux:138 (KEY_HELP) */
  [0x176] = 0xb3, /* xtkbd:374 -> linux:179 (KEY_KPLEFTPAREN) -> linux:179 (KEY_KPLEFTPAREN) */
  [0x177] = 0xbc, /* xtkbd:375 -> linux:188 (KEY_F18) -> linux:188 (KEY_F18) */
  [0x178] = 0x85, /* xtkbd:376 -> linux:133 (KEY_COPY) -> linux:133 (KEY_COPY) */
  [0x179] = 0xc0, /* xtkbd:377 -> linux:192 (KEY_F22) -> linux:192 (KEY_F22) */
  [0x17b] = 0xb4, /* xtkbd:379 -> linux:180 (KEY_KPRIGHTPAREN) -> linux:180 (KEY_KPRIGHTPAREN) */
  [0x17d] = 0xa2, /* xtkbd:381 -> linux:162 (KEY_EJECTCLOSECD) -> linux:162 (KEY_EJECTCLOSECD) */
};



static mouse_t mouse;
static struct {
    uint16_t* vkeymap;
    uint8_t capslock;
    uint8_t numlock;
    uint8_t scrolllock;
    uint8_t e0;
} kb;

static evid_t kdid;
static evid_t mdid;


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
    
        (kb.scrolllock ? (1 << 0) : 0)    |
        (kb.numlock ? (1 << 1) : 0)     |
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

  irq_ack(1);


    uint16_t vkey = KEY_RESERVED;
    if(kb.e0)
        vkey = kb.vkeymap[0x100 | (vkscan & 0x7F)];
    else
        vkey = kb.vkeymap[vkscan & 0x7F];

  kb.e0 = 0;



    if(vkey == KEY_RESERVED)
        return;

    switch(vkey) {
        case KEY_CAPSLOCK:
            kb.capslock != kb.capslock;
            kb_setled();
            break;
        case KEY_NUMLOCK:
            kb.numlock != kb.numlock;
            kb_setled();
            break;
        case KEY_SCROLLLOCK:
            kb.scrolllock != kb.scrolllock;
            kb_setled();
    }



  keyboard_t k;
  k.vkey = vkey;
  k.down = !!!(vkscan & 0x80);

  __event_raise_EV_KEY(kdid, k.vkey, k.down);
    __fifo_send(PATH_KBDEV, &k, sizeof(keyboard_t));
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
                    mouse.dy = -(mouse.pack[2] - ((mouse.pack[0] & 0x20) ? 256 : 0)) * mouse.speed;

                    //mouse.dz = mouse.pack[3];

                    mouse.x += mouse.dx;
                    mouse.y -= mouse.dy;
                    
                    /* TODO: Add clipping */
                    
    
                    mouse.buttons[0] = (mouse.pack[0] & 0x01);
                    mouse.buttons[1] = (mouse.pack[0] & 0x02);
                    mouse.buttons[2] = (mouse.pack[0] & 0x04);
                    
                    
          
          
          for(int i = 0; i < 3; i++)
            __event_raise_EV_KEY(mdid, BTN_MOUSE + i, mouse.buttons[i]);

          __event_raise_EV_REL(mdid, mouse.dx, mouse.dy, mouse.dz);
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

    kb.vkeymap = &ps2_codemap[0];



    #define MOUSE_WRITE(x)        \
        PS2_WAIT;                \
        outb(PS2_CTRL, 0xD4);    \
        PS2_WAIT;                \
        outb(PS2_DATA, x)

    #define MOUSE_READ(x)        \
        PS2_WAIT_0;                \
        x = inb(PS2_DATA)



    inb(0x60);
    
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
    mouse.clip.right = 0x7FFF;
    mouse.clip.bottom = 0x7FFF;


    if(sys_mkfifo(PATH_KBDEV, 0644) != 0)
        kprintf(ERROR "%s: cannot create FIFO device!\n", PATH_KBDEV);
        
    if(sys_mkfifo(PATH_MOUSEDEV, 0644) != 0)
        kprintf(ERROR "%s: cannot create FIFO device!\n", PATH_MOUSEDEV);
        

  kdid = __event_device_add("ps2-keyboard", EC_KEY | EC_LED);
  mdid = __event_device_add("ps2-mouse", EC_KEY | EC_REL);

  __event_device_set_enabled(kdid, 1);
  __event_device_set_enabled(mdid, 1);

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
