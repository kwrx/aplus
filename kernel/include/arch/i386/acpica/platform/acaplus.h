#ifndef __ACAPLUS_H__
#define __ACAPLUS_H__


#include <aplus.h>
#include <aplus/base.h>
#include <aplus/module.h>



#define ACPI_INIT_FUNCTION
#define ACPI_EXPORT_SYMBOL(symbol)              EXPORT(symbol)


#define ACPI_CACHE_T                            ACPI_MEMORY_LIST
#define ACPI_USE_LOCAL_CACHE                    1

#define ACPI_SPINLOCK                           void*
#define ACPI_SEMAPHORE                          void*


#define ACPI_MSG_ERROR                          "ACPI Error: "
#define ACPI_MSG_EXCEPTION                      "ACPI Exception: "
#define ACPI_MSG_WARNING                        "ACPI Warning: "
#define ACPI_MSG_INFO                           "ACPI: "

#define ACPI_MSG_BIOS_ERROR                     "ACPI BIOS Error (bug): "
#define ACPI_MSG_BIOS_WARNING                   "ACPI BIOS Warning (bug): "




#define ACPI_MACHINE_WIDTH                      32
#define ACPI_32BIT_PHYSICAL_ADDRESS


#define ACPI_USE_DO_WHILE_0
#define ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_SYSTEM_HEADERS
#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_STANDARD_HEADERS

#endif /* __ACAPLUS_H__ */
