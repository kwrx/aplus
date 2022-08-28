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
#include <vector>
#include <functional>

namespace aplus::ui::displays {

    class Display {

        public:

            Display(uint16_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h, void* framebuffer)
                : __id(id)
                , __x(x)
                , __y(y)
                , __w(w)
                , __h(h)
                , __framebuffer(framebuffer) {}

            const uint16_t id() const { return __id; }
            const uint16_t x()  const { return __x;  }
            const uint16_t y()  const { return __y;  }
            const uint16_t w()  const { return __w;  }
            const uint16_t h()  const { return __h;  }
            
            void* framebuffer() {
                return __framebuffer;
            }

            void present() const;
            void present(uint16_t off_x, uint16_t off_y, uint16_t off_w, uint16_t off_h) const;


            static void init();
            static void query(std::function<void(Display&)> selector);
            static Display& primary();

        private:

            uint16_t __id = {};
            uint16_t __x  = {};
            uint16_t __y  = {};
            uint16_t __w  = {};
            uint16_t __h  = {};
            void* __framebuffer = {};

            static std::vector<Display> __displays;

    };

}