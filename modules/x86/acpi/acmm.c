#include "acpi.h"
#include "accommon.h"

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
#ifdef ACPI_DEBUG
	kprintf("acpica: map memory at 0x%x (%d Bytes)\n", PhysicalAddress, Length);
#endif

	return PhysicalAddress;
}

void AcpiOsUnmapMemory(void* where, ACPI_SIZE Length) {
#ifdef ACPI_DEBUG
	kprintf("acpica: unmap memory at 0x%x (%d Bytes)\n", where, Length);
#endif

	return;
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress) {
	*PhysicalAddress = LogicalAddress;
	return 0;
}

void* AcpiOsAllocate(ACPI_SIZE Size) {
	return kmalloc(Size);
}

void AcpiOsFree(void* ptr) {
	kfree(ptr);
}

BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length) {
	return 1;
}

BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length) {
	return 1;
}
