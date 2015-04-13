#include "acpi.h"
#include "accommon.h"

#include <aplus.h>
#include <aplus/task.h>

ACPI_THREAD_ID AcpiOsGetThreadId() {
	return sys_getpid();
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) {
	return sys_clone(Function, NULL, CLONE_FS | CLONE_VM | CLONE_FILES | CLONE_SIGHAND, Context);
}

void AcpiOsSleep(UINT64 ms) {
	return;
}

void AcpiOsStall(UINT32 um) {
	return;
}
