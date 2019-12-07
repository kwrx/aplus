#include <stdint.h>
#include <aplus/core/base.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/debug.h>



/*!
 * @brief bmain().
 *        Boot Entrypoint.
 * 
 * Initialize Hardware and boot services.
 */
void bmain(multiboot_uint32_t magic, struct multiboot_tag* btags) {
    
    arch_debug_init();


}