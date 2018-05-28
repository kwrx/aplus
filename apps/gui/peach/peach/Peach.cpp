/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
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


#include <peach/peach.h>
#include <iostream>
#include <list>
#include <unistd.h>
#include <fcntl.h>
#include <aplus/base.h>
#include <aplus/fb.h>
#include <aplus/sysconfig.h>
using namespace std;


Peach::Peach() {
    this->DefaultScreen = new Screen(this);
    this->DefaultScreen->SetMode (
        reinterpret_cast<uint32_t>(sysconfig("screen.width", 800)),
        reinterpret_cast<uint32_t>(sysconfig("screen.height", 600)),
        reinterpret_cast<uint32_t>(sysconfig("screen.bpp", 32))
    );

    this->Windows = new vector<Window*>();
    this->WindowTop = NULL;
    this->WindowFocused = NULL;
}

Peach::~Peach() {
    delete this->DefaultScreen;
}
