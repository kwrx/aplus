/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2022 Antonino Natale
 * 
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

#pragma once


#include <iostream>
#include <string>


namespace aplus::ui {

    struct UIConfig {

        struct {

            struct {
                
                size_t displays = 16;
                size_t frame_width = 8192;
                size_t frame_height = 8192;

            } limits;

            struct {

                size_t frame_width = 1280;
                size_t frame_height = 720;
                size_t frame_bits_per_pixel = 32;

            } defaults;

            std::string device = "/dev/fb";

        } display;

    } UIDefaultConfig = {};

}


#define LOG std::cerr << "[UI] (" << __FILE__ << "::" << __func__ << ":" << __LINE__ << "): "