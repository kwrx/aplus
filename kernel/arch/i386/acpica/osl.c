#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/mmio.h>
#include <aplus/intr.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>

#include <sys/mman.h>
#include "acpi.h"


#if defined(__i386__)
#   include <arch/i386/i386.h>
#endif

//#define __TRACE kprintf(LOG, "acpi-trace: (%d):%s ()\n", __LINE__, __func__);
#define __TRACE

ACPI_STATUS AcpiOsInitialize() { __TRACE
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate() { __TRACE
    return AE_OK;
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() { __TRACE
    ACPI_PHYSICAL_ADDRESS r = 0;
    AcpiFindRootPointer(&r);
    return r;
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue) { __TRACE
    *NewValue = 0;
    return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *old_table, ACPI_TABLE_HEADER **new_table) { __TRACE
    *new_table = NULL;
    return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *old_table, ACPI_PHYSICAL_ADDRESS *new_table, UINT32 *new_table_len) { __TRACE
    *new_table = 0;
    *new_table_len = 0;
    return AE_OK;
}



void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS phy_addr, ACPI_SIZE len) { __TRACE
    return sys_mmap((void*)phy_addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, 0, 0);
}


void AcpiOsUnmapMemory(void *virt_addr, ACPI_SIZE len) { __TRACE
    sys_munmap(virt_addr, len);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress) { __TRACE
    *PhysicalAddress = (ACPI_PHYSICAL_ADDRESS) LogicalAddress;
    return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE n) { __TRACE
    return kmalloc(n, GFP_KERNEL);
}


void AcpiOsFree(void *addr) { __TRACE
    kfree(addr);
}

BOOLEAN AcpiOsReadable(void *addr, ACPI_SIZE n) { __TRACE
    return 1;
}
BOOLEAN AcpiOsWritable(void *addr, ACPI_SIZE n) { __TRACE
    return 1;
}


ACPI_THREAD_ID AcpiOsGetThreadId() { __TRACE
    return sys_getpid();
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context) { __TRACE
    sys_clone((int (*)(void*)) Function, NULL, CLONE_FILES | CLONE_FS | CLONE_SIGHAND | CLONE_VM, Context);
    return AE_OK;
}

void AcpiOsSleep(UINT64 Milliseconds) { __TRACE
    timer_delay(Milliseconds);
}
void AcpiOsStall(UINT32 Microseconds) { __TRACE
    timer_delay(1);
}
void AcpiOsWaitEventsComplete(void) { __TRACE
    
}


ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle) { __TRACE
    *OutHandle = 0;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) { __TRACE
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout) { __TRACE
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) { __TRACE
    return AE_OK;
}

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle) { __TRACE
    *OutHandle = 0;
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) { __TRACE
    return;
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) { __TRACE
    return AE_OK;
} 

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags) { __TRACE
    
}


ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context) { __TRACE
    irq_enable(InterruptLevel, (void (*)(void*)) Handler);
    return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler) { __TRACE
    irq_disable(InterruptNumber);
    return AE_OK;
}



ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 *val, UINT32 width) { __TRACE
    switch(width) {
        case 1:
        case 8:
            *val = (UINT64) mmio_r8(addr);
            break;
        case 2:
        case 16:
            *val = (UINT64) mmio_r16(addr);
            break;
        case 4:
        case 32:
            *val = (UINT64) mmio_r32(addr);
            break;
        case 64:
            *val = (UINT64) mmio_r64(addr);
            break;
        default:
            kprintf(ERROR, "ACPI: BUG! invalid width %d in %s\n", width, __func__);
            break;
    }

    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS addr, UINT64 val, UINT32 width) { __TRACE
    switch(width) {
        case 1:
        case 8:
            mmio_w8(addr, val);
            break;
        case 2:
        case 16:
            mmio_w16(addr, val);
            break;
        case 4:
        case 32:
            mmio_w32(addr, val);
            break;
        case 64:
            mmio_w64(addr, val);
            break;
        default:
            kprintf(ERROR, "ACPI: BUG! invalid width %d in %s\n", width, __func__);
            break;
    }

    return AE_OK;
}



ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS addr, UINT32 *val, UINT32 width) { __TRACE
    switch(width) {
        case 1:
        case 8:
            *val = (UINT64) inb(addr);
            break;
        case 2:
        case 16:
            *val = (UINT64) inw(addr);
            break;
        case 4:
        case 32:
            *val = (UINT64) inl(addr);
            break;
        case 64:
        default:
            kprintf(ERROR, "ACPI: BUG! invalid width %d in %s\n", width, __func__);
            break;
    }

    return AE_OK;
}
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS addr, UINT32 val, UINT32 width) { __TRACE
    switch(width) {
        case 1:
        case 8:
            outb(addr, val);
            break;
        case 2:
        case 16:
            outw(addr, val);
            break;
        case 4:
        case 32:
            outl(addr, val);
            break;
        case 64:
        default:
            kprintf(ERROR, "ACPI: BUG! invalid width %d in %s\n", width, __func__);
            break;
    }

    return AE_OK;
}


ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *pciId, UINT32 reg, UINT64 *val, UINT32 width) { __TRACE
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *pciId, UINT32 reg, UINT64 val, UINT32 width) { __TRACE
    return AE_OK;
}


void AcpiOsVprintf(const char *fmt, va_list args) {
    static char buf[BUFSIZ];
    memset(buf, 0, BUFSIZ);

    vsprintf(buf, fmt, args);
    kprintf(INFO, buf);
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *fmt, ...) {
    va_list args;
	va_start(args, fmt);
	AcpiOsVprintf(fmt, args);
    va_end(args);
}

void AcpiOsRedirectOutput(void *dst) {
    
}

UINT64 AcpiOsGetTimer(void) { __TRACE
    return timer_getticks() * 10000;
}

ACPI_STATUS AcpiOsSignal(UINT32 func, void *info) { __TRACE
    return AE_OK;
}

ACPI_STATUS AcpiOsGetLine(char *buf, UINT32 len, UINT32 *read) {
    return AE_OK; 
}

ACPI_STATUS AcpiOsEnterSleep (UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) { __TRACE
    return AE_OK;
}


EXPORT(AcpiInitializeSubsystem);