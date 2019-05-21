/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _ARCH_X86_ACPI_H
#define _ARCH_X86_ACPI_H

#include <stdint.h>


#define X86_MADT_ENTRY_LAPIC            0
#define X86_MADT_ENTRY_IOAPIC           1
#define X86_MADT_ENTRY_INTERRUPT        2
#define X86_MADT_ENTRY_NMI              4
#define X86_MADT_ENTRY_LAPIC64          5


typedef struct {
    char magic[8];
    uint8_t cksum;
    char id[6];
    uint8_t revision;
    uint32_t address;

    /* XDST */
    uint32_t length;
    uint64_t xaddress;
    uint8_t xcksum;
    uint8_t padding[3];
} __packed acpi_rsdp_t;

typedef struct {
    char magic[4];
    uint32_t length;
    uint8_t revision;
    uint8_t cksum;
    char id[6];
    char tid[8];
    uint32_t orevision;
    uint32_t cid;
    uint32_t crevision;
    
    union {
        uint32_t tables[0];
        uint64_t xtables[0];
    };
} __packed acpi_sdt_t;


typedef struct {
    uint8_t type;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t size;
    uint64_t address;
} acpi_generic_address_t;

typedef struct {
    uint32_t fwctrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t pwrmode;
    uint16_t sci_interrupt;
    uint16_t smi_command;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_ctrl;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_arch_flags;
    uint8_t reserved2;
    uint32_t flags;

    acpi_generic_address_t reset_reg;

    uint8_t reset_value;
    uint8_t padding[3];

    uint64_t x_fwctrl;
    uint64_t x_dsdt;

    acpi_generic_address_t x_pm1a_event_block;
    acpi_generic_address_t x_pm1b_event_block;
    acpi_generic_address_t x_pm1a_control_block;
    acpi_generic_address_t x_pm1b_control_block;
    acpi_generic_address_t x_pm2_control_block;
    acpi_generic_address_t x_pm_timer_block;
    acpi_generic_address_t x_gpe0_block;
    acpi_generic_address_t x_gpe1_block;
} __packed acpi_fadt_t;


typedef struct {
    uint32_t lapic_address;
    uint32_t flags;
    uint8_t entries[0];
} __packed acpi_madt_t;

void acpi_init(void);
int acpi_find(acpi_sdt_t** sdt, const char name[4]);
#endif