#include "acpi.h"
#include "accommon.h"

ACPI_STATUS AcpiOsInitialize() {
#ifdef ACPI_DEBUG
	kprintf("acpica: initializing.\n");
#endif
	return 0;
}

ACPI_STATUS AcpiOsTerminate() {
#ifdef ACPI_DEBUG
	kprintf("acpica: terminating.\n");
#endif
	return 0;
}


ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
	ACPI_SIZE Ret;
	AcpiFindRootPointer(&Ret);
	return Ret;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* PredefinedObject, ACPI_STRING* NewValue) {
	*NewValue = NULL;
	return 0;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable) {
	*NewTable = NULL;
	return 0;
}

