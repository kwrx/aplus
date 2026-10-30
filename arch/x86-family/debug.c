/*
 * GPL3 License
 *
 * Author(s):
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 *
 * Copyright (c) 2013-2019 Antonino Natale
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

#include <stdint.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>



#define BIOS_COM_ADDRESS 0x400

#define COM1_DEFAULT_PORT 0x3F8
#define COM2_DEFAULT_PORT 0x2F8
#define COM3_DEFAULT_PORT 0x3E8
#define COM4_DEFAULT_PORT 0x2E8


static uint16_t com_address = 0;


#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)

    #include "debug/font_8x16.c.in"

    #define X86_VGA_WIDTH  (8)
    #define X86_VGA_HEIGHT (16)

    #define X86_VGA_MAX_COLS (512)
    #define X86_VGA_MAX_ROWS (256)

static size_t vga_offset    = 0;
static uintptr_t vga_mapped = 0;
static uint8_t vga_text[X86_VGA_MAX_COLS * X86_VGA_MAX_ROWS];


static int debug_vga_get_geometry(uintptr_t* address, size_t* cols, size_t* rows, size_t* size, size_t* bytes_per_pixel) {

    if (!core->framebuffer.address || core->framebuffer.width < X86_VGA_WIDTH || core->framebuffer.height < X86_VGA_HEIGHT)
        return 0;

    switch (core->framebuffer.depth) {
        case 8:
        case 16:
        case 24:
        case 32:
            break;
        default:
            return 0;
    }

    *bytes_per_pixel = core->framebuffer.depth / 8;

    if (core->framebuffer.width > SIZE_MAX / *bytes_per_pixel)
        return 0;

    if (core->framebuffer.pitch < core->framebuffer.width * *bytes_per_pixel)
        return 0;

    if (core->framebuffer.height > SIZE_MAX / core->framebuffer.pitch)
        return 0;

    *size = core->framebuffer.pitch * core->framebuffer.height;

    if (vga_mapped) {

        *address = vga_mapped;

    } else {

        if (core->framebuffer.address > UINTPTR_MAX - KERNEL_HEAP_AREA)
            return 0;

        *address = core->framebuffer.address + KERNEL_HEAP_AREA;
    }

    if (*address > UINTPTR_MAX - *size)
        return 0;

    *cols = MIN(core->framebuffer.width / X86_VGA_WIDTH, X86_VGA_MAX_COLS);
    *rows = MIN(core->framebuffer.height / X86_VGA_HEIGHT, X86_VGA_MAX_ROWS);

    return *cols && *rows;
}

static void debug_vga_draw_cell(uintptr_t address, size_t cols, size_t cell, size_t bytes_per_pixel) {

    const uint8_t* glyph = &builtin_fontdata[vga_text[cell] * X86_VGA_HEIGHT];

    uint8_t* line = (uint8_t*)address;
    line += (cell % cols) * X86_VGA_WIDTH * bytes_per_pixel;
    line += (cell / cols) * X86_VGA_HEIGHT * core->framebuffer.pitch;

    for (int y = 0; y < X86_VGA_HEIGHT; y++, line += core->framebuffer.pitch) {

        uint8_t* ptr = line;

        for (int x = 0; x < X86_VGA_WIDTH; x++, ptr += bytes_per_pixel) {

            bool on = glyph[y] & (1U << (X86_VGA_WIDTH - 1 - x));

            switch (bytes_per_pixel) {

                case 4:
                    *(volatile uint32_t*)ptr = on ? 0xFFFFFFFF : 0xFF000000;
                    break;

                case 3:
                    ptr[0] = on ? 0xFF : 0x00;
                    ptr[1] = on ? 0xFF : 0x00;
                    ptr[2] = on ? 0xFF : 0x00;
                    break;

                case 2:
                    *(volatile uint16_t*)ptr = on ? 0xFFFF : 0x0000;
                    break;

                case 1:
                    *(volatile uint8_t*)ptr = on ? 0xFF : 0x00;
                    break;
            }
        }
    }
}

static void debug_vga_scroll(uintptr_t address, size_t cols, size_t rows, size_t bytes_per_pixel) {

    while (vga_offset >= cols * rows) {

        for (size_t i = 0; i < cols * rows; i++) {

            uint8_t ch = i < cols * (rows - 1) ? vga_text[i + cols] : 0;

            if (vga_text[i] == ch)
                continue;

            vga_text[i] = ch;
            debug_vga_draw_cell(address, cols, i, bytes_per_pixel);
        }

        vga_offset -= cols;
    }
}

#endif


/*!
 * @brief Initialize Debugger on UARTx.
 *
 * Read COM Address from SMBios Area or default ports collection and configure Serial Ports.
 */
void arch_debug_init(void) {

#if defined(CONFIG_X86_HAVE_SMBIOS)
    uint16_t* p = (uint16_t*)(KERNEL_HEAP_AREA + BIOS_COM_ADDRESS);
#else
    uint16_t p[] = {COM1_DEFAULT_PORT, COM2_DEFAULT_PORT, COM3_DEFAULT_PORT, COM4_DEFAULT_PORT};
#endif


    for (int i = 0; i < 4; i++) {

        if (p[i] == 0)
            continue;

        com_address = p[i];

        outb(com_address + 1, 0x00);
        outb(com_address + 3, 0x80);
        outb(com_address + 0, 0x01);
        outb(com_address + 1, 0x00);
        outb(com_address + 3, 0x03);
        outb(com_address + 2, 0xC7);
        outb(com_address + 4, 0x0B);

        break;
    }


    if (com_address == 0)
        return;

    arch_debug_putc('\e');
    arch_debug_putc('[');
    arch_debug_putc('0');
    arch_debug_putc('m');



#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)
    vga_offset = 0;
#endif
}


/*!
 * @brief Write to Debugger.
 *
 * Wait and write a character on Serial Port.
 *
 * TODO: CONFIG_DEBUG_PRINT_TIMESTAMP is meant to stamp each line and does nothing yet.
 */
void arch_debug_putc(char ch) {


#define com_wait()                                                                 \
    {                                                                              \
        for (int i = 0; i < 100000 && ((inb(com_address + 5) & 0x20) == 0); i++) { \
            __builtin_ia32_pause();                                                \
        }                                                                          \
    }


    if (likely(com_address)) {

        com_wait();

        if (unlikely(ch == '\n')) {

            outb(com_address, '\r');
            com_wait();
            outb(com_address, '\n');
            com_wait();


#if defined(CONFIG_DEBUG_PRINT_TIMESTAMP)
#endif

        } else {

            outb(com_address, ch);
            com_wait();
        }
    }


#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)

    uintptr_t vga_address;
    size_t vga_cols;
    size_t vga_rows;
    size_t vga_size;
    size_t bytes_per_pixel;

    if (likely(debug_vga_get_geometry(&vga_address, &vga_cols, &vga_rows, &vga_size, &bytes_per_pixel))) {

        debug_vga_scroll(vga_address, vga_cols, vga_rows, bytes_per_pixel);

        switch (ch) {

            case '\r':
                vga_offset -= vga_offset % vga_cols;
                break;

            case '\n':
                vga_offset += vga_cols - (vga_offset % vga_cols);
                break;

            case '\v':
                vga_offset += vga_cols;
                break;

            case '\b':
                if (vga_offset)
                    vga_offset -= 1;
                break;

            case '\t':
                vga_offset += 8 - (vga_offset % 8);
                break;

            default:

                vga_text[vga_offset] = (uint8_t)ch;
                debug_vga_draw_cell(vga_address, vga_cols, vga_offset, bytes_per_pixel);

                vga_offset += 1;

                break;
        }

        debug_vga_scroll(vga_address, vga_cols, vga_rows, bytes_per_pixel);
    }

#endif
}


/**
 * @brief Map the boot framebuffer write-combining and redraw the text console through it.
 */
void arch_debug_map(void) {

#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)

    uintptr_t vga_address;
    size_t vga_cols;
    size_t vga_rows;
    size_t vga_size;
    size_t bytes_per_pixel;

    if (!debug_vga_get_geometry(&vga_address, &vga_cols, &vga_rows, &vga_size, &bytes_per_pixel))
        return;

    uintptr_t offset = core->framebuffer.address & (PML1_PAGESIZE - 1);
    uintptr_t span   = (offset + vga_size + PML1_PAGESIZE - 1) & ~(PML1_PAGESIZE - 1);

    if (span > KERNEL_MMIO_SIZE)
        return;

    uintptr_t virtaddr = KERNEL_MMIO_AREA + KERNEL_MMIO_SIZE - span;

    if (arch_vmm_map(&core->bsp.address_space, virtaddr, core->framebuffer.address - offset, span,
                     ARCH_VMM_MAP_RDWR | ARCH_VMM_MAP_NOEXEC | ARCH_VMM_MAP_FIXED | ARCH_VMM_MAP_VIDEO_MEMORY) == ARCH_VMM_MAP_FAILED)
        return;

    vga_mapped = virtaddr + offset;

    for (size_t i = 0; i < vga_cols * vga_rows; i++)
        debug_vga_draw_cell(vga_mapped, vga_cols, i, bytes_per_pixel);

#endif
}



/*!
 * @brief Stacktrace.
 *
 * Print stacktrace on Serial Port.
 */
/**
 * @brief Walks the frame pointer chain into @p frames.
 *
 * @param frames Receives the return addresses, zero-terminated.
 * @param count How many @p frames can hold.
 */
void arch_debug_stacktrace(uintptr_t* frames, size_t count) {


    if (!current_task)
        return;


    struct stack {
        struct stack* bp;
        uintptr_t ip;
    } __packed* frame;


#if defined(__x86_64__)
    __asm__ __volatile__("movq %%rbp, %%rax" : "=a"(frame));
#elif defined(__i386__)
    __asm__ __volatile__("movl %%ebp, %%rax" : "=a"(frame));
#else
    #error "Unsupported Architecture"
#endif


    uintptr_t stack_start = (uintptr_t)frame;
    uintptr_t stack_end   = stack_start > UINTPTR_MAX - KERNEL_STACK_SIZE ? UINTPTR_MAX : stack_start + KERNEL_STACK_SIZE;

    for (size_t i = 0; frame && i < count; i++) {

        frames[i] = 0;

        uintptr_t address = (uintptr_t)frame;

        if (unlikely(address < stack_start || address > stack_end || sizeof(*frame) > stack_end - address || (address & (sizeof(uintptr_t) - 1))))
            break;

        if (unlikely(!uio_check(address, R_OK | S_OK) || !uio_check(address + sizeof(*frame) - 1, R_OK | S_OK)))
            break;

        struct stack next;

        if (unlikely(!uio_check(address, S_OK))) {

#if defined(__x86_64__)
            next.ip = uio_r64(address + offsetof(struct stack, ip));
            next.bp = (struct stack*)uio_r64(address + offsetof(struct stack, bp));
#else
            next.ip = uio_r32(address + offsetof(struct stack, ip));
            next.bp = (struct stack*)uio_r32(address + offsetof(struct stack, bp));
#endif

        } else {

            next = *frame;
        }

        frames[i] = next.ip;

        if (!next.bp)
            break;

        if (unlikely((uintptr_t)next.bp <= address))
            break;

        frame = next.bp;
    }
}
