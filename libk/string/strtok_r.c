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
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


char *strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr) {

	DEBUG_ASSERT(delim);
	DEBUG_ASSERT(saveptr);

	
	if (!str && !*saveptr)
		return NULL;

	if (!str)
		str = *saveptr;


	str += strspn(str, delim);

	if (!*str)
		return *saveptr = NULL;


	size_t amount = strcspn(str, delim);

	if (str[amount])
		*saveptr = str + amount + 1;
	else
		*saveptr = NULL;


	str[amount] = '\0';
	return str;

}


TEST(libk_strtok_test, {

	char str[] = "Hello World!";
	char* tok = str;

	char* token = strtok_r(str, " ", &tok);

	DEBUG_ASSERT(token != NULL);
	DEBUG_ASSERT(strcmp(token, "Hello") == 0);

	token = strtok_r(NULL, " ", &tok);

	DEBUG_ASSERT(token != NULL);
	DEBUG_ASSERT(strcmp(token, "World!") == 0);

	token = strtok_r(NULL, " ", &tok);

});