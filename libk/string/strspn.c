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
                                                                        
#include <stdint.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>



size_t strspn(const char *str, const char* accept) {

	size_t accept_length = 0;

	while (accept[accept_length])
		accept_length++;


	for (size_t result = 0; 1; result++) {

		char c = str[result];
		if (!c)
			return result;


		int matches = 0;
		for (size_t i = 0; i < accept_length; i++) {
		
			if (str[result] != accept[i])
				continue;

			matches = 1;
			break;
		}

		if (!matches)
			return result;

	}

}