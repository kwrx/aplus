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
#include <string>
#include <unistd.h>
#include <fcntl.h>
using namespace std;


Window::Window(Peach* peach) {
    this->surface = NULL;
    this->X = peach->DefaultScreen->Width >> 2;
    this->Y = peach->DefaultScreen->Height >> 2;

    this->Title = L"Window";
    this->Flags = WindowFlags::Hide;
    this->MaximizeBox =
    this->MinimizeBox = true;
    this->context = peach;

    this->Resize(peach->DefaultScreen->Width >> 1, peach->DefaultScreen->Height >> 1);
}

Window::~Window() {
    if(this->surface)
        delete this->surface;
}

bool Window::Resize(uint16_t w, uint16_t h) {
    if(w < WINDOW_SIZE_W_MIN || h < WINDOW_SIZE_H_MIN)
        return false;

    this->Width = w;
    this->Height = h;
    
    if(!(this->Flags & WindowFlags::NoBorder)) {
        if(this->surface)
            delete this->surface;

        this->surface = new Renderer(this->context, w, h);
        renderTitleBar(this);
    }

    return true;
}