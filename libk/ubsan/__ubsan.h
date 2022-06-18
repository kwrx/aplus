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

#ifndef __UBSAN_H__
#define __UBSAN_H__

#include <stdint.h>

struct source_location {
    const char* file;
    uint32_t line;
    uint32_t column;
};

struct type_descriptor {
    uint16_t kind;
    uint16_t info;
    const char name[0];
};

struct type_mismatch_data {
    struct source_location location;
    struct type_descriptor* type;
    uint8_t alignment;
    uint8_t type_check_kind;
};

struct invalid_builtin_data {
    struct source_location location;
    uint8_t kind;
};

struct invalid_value_data {
    struct source_location location;
    struct type_descriptor* type;
};

struct pointer_overflow_data {
    struct source_location location;
};

struct overflow_data {
    struct source_location location;
    struct type_descriptor* type;
};

struct out_of_bounds_data {
    struct source_location location;
    struct type_descriptor* array_type;
    struct type_descriptor* index_type;
};

struct shift_out_of_bounds_data {
    struct source_location location;
    struct type_descriptor* lhs_type;
    struct type_descriptor* rhs_type;
};

struct nonnull_arg_data {
    struct source_location location;
    struct source_location attribute;
    int arg_index;
};

struct nonnull_return_data {
    struct source_location attribute;
};

#if defined(__WITH_KINDS)
static const char* __kinds[] = {
    "load of",
    "store to",
    "reference binding to",
    "member access within",
    "member call on",
    "constructor call on",
    "downcast of",
    "downcast of",
    "upcast of",
    "cast to virtual base of",
};
#endif

#endif