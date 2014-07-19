//
//  events.h
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#define SIGEVNT									(16)
#define EV_ERROR								(-1)

/* Keyboard events */
#define EV_KB_KEYDOWN							0
#define EV_KB_KEYUP								1


/* Mouse events */
#define EV_MOUSE_BUTTONDOWN						0
#define EV_MOUSE_BUTTONUP						1
#define EV_MOUSE_SCROLL							2


int event_add(int type);
int event_rem(int type);
int event_gettype();
int event_raise(int type);

