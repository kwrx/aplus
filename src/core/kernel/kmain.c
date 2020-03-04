/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
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
                                                                        
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>
#include <aplus/core/smp.h>
#include <aplus/core/hal.h>


static struct syscore __core;
struct syscore* core = &__core;


void cmain() {

}


void kmain() {


#if defined(CONFIG_HAVE_SMP)
    smp_init();
#endif


    kprintf ("core: %s %s-%s (%s)\n", CONFIG_SYSTEM_NAME,
                                      CONFIG_SYSTEM_VERSION,
                                      CONFIG_SYSTEM_CODENAME,
                                      CONFIG_COMPILER_HOST);
        
    kprintf("core: built with gcc %s (%s)\n", __VERSION__,
                                              __TIMESTAMP__);



}
