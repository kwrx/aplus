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
#include <limits.h>
#include <sys/types.h>
#include <string.h>


size_t strcspn(const char *str, const char* reject) {

	size_t reject_length = 0;

	while (reject[reject_length])
		reject_length++;


	for (size_t result = 0; 1; result++) {

		char c = str[result];
		if (!c)
			return result;
	
	
		int matches = 0;
		for (size_t i = 0; i < reject_length; i++) {
			
			if (str[result] != reject[i])
				continue;

			matches = 1;
			break;
		}

		if (matches)
			return result;

	}

}