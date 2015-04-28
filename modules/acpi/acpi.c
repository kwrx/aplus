#ifdef __i386__

#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>


#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/times.h>

#include <arch/i386/i386.h>

#define SCI_EN		1


typedef struct {
	char magic[8];
	char checksum;
	char oem_id[6];
	char revision;
	void* rsdt_addr;

	/* 2.0 *
	uint32_t length;
	uint64_t xsdt_addr;
	uint8_t extchksum;
	uint8_t reserved[3];
	*/
} __packed rsdp_t;

typedef struct sdt {
	char magic[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;

	char oem_id[6];
	char oem_table[8];
	uint32_t oem_revision;

	uint32_t creator_id;
	uint32_t creator_revision;
} __packed sdt_t;


typedef struct fadt {
	sdt_t sdt;
	uint32_t firmware_ctl;
	uint32_t dsdt;

	uint8_t reserved;

	uint8_t ppmp;
	uint16_t sci_intr;
	uint32_t smi_cmd;
	uint8_t acpi_enable;
	uint8_t acpi_disable;
	uint8_t s4bios_req;
	uint8_t ps_ctl;
	uint32_t pm1_a_eb;
	uint32_t pm1_b_eb;
	uint32_t pm1_a_ctl;
	uint32_t pm1_b_ctl;
	uint32_t pm2_ctl;
	uint32_t pm_timer;
	uint32_t gpe0;
	uint32_t gpe1;
	uint8_t pm1_el;
	uint8_t pm1_cl;
	uint8_t pm2_cl;
	uint8_t pm_tl;
	uint8_t gpe0_l;
	uint8_t gpe1_l;
	uint8_t gpe1_b;
	uint8_t cstate_ctl;
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

	uint8_t reserved3[128];
} fadt_t;


struct {
	sdt_t* rsdt;
	sdt_t** tables;
	uint32_t entries;
} acpi;


static int acpi_checksum(uint8_t* header, uint32_t length) {
	uint8_t ck = 0;
	while(length--)
		ck += *header++;

	return (int) (ck == 0);
}


static sdt_t* acpi_find_rsdt(void) {
	for(
		uint32_t* addr = (uint32_t*) 0x000E0000;
		(int) addr < 0x00100000;
		addr += 4
	) {

		rsdp_t* rp = (rsdp_t*) addr;
		if(memcmp(rp->magic, "RSD PTR ", 8) == 0) {
			
			uint8_t ck = 0;
			for(int i = 0; i < sizeof(rsdp_t); i++)
				ck += *(uint8_t*) ((int) addr + i);
			
			if(ck == 0)
				return rp->rsdt_addr;
		}
	}

	return NULL;
}


static sdt_t* acpi_find_table(char* name) {
	for(int i = 0; i < acpi.entries; i++) {
		if(memcmp(acpi.tables[i]->magic, name, 4) == 0)
			return acpi.tables[i];
	}
}

static int acpi_enable(fadt_t* facp) {
	if(unlikely(!facp))
		return -1;

	if((inw(facp->pm1_a_ctl) & SCI_EN) == 0) {
		outb(facp->smi_cmd, facp->acpi_enable);

		while((inw(facp->pm1_a_ctl) & SCI_EN) != 1)
			cpu_wait();
	}

	if(facp->pm1_b_ctl != 0)
		while((inw(facp->pm1_b_ctl) & SCI_EN) != 1)
			cpu_wait();

	return 0;
}


int acpi_init(void) {
	acpi.rsdt = acpi_find_rsdt();
	if(unlikely(!acpi.rsdt)) {
		kprintf("acpi: cannot found RSDT\n");
		return -1;
	}

	acpi.entries = (acpi.rsdt->length - sizeof(sdt_t)) / 4;
	acpi.tables = (sdt_t**) ((uint32_t) acpi.rsdt + sizeof(sdt_t));

#ifdef ACPI_DEBUG
	kprintf("acpi: SDT entries are %d\n", acpi.entries);

	for(int i = 0; i < acpi.entries; i++)
		kprintf(" > %c%c%c%c\n", 	acpi.tables[i]->magic[0],
									acpi.tables[i]->magic[1],
									acpi.tables[i]->magic[2],
									acpi.tables[i]->magic[3]);
#endif


	int k = acpi_enable((fadt_t*) acpi_find_table("FACP"));
	if(k != 0)
		kprintf("acpi: could not enable ACPI\n");

	return k;
}

#endif
