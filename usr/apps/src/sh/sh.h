//
//  sh.h
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

#ifndef _SH_H
#define _SH_H

#include <stdio.h>

#define SH_NAME			"aPlus Shell"

#ifdef DEBUG
#define SH_VERSION		"1.00-debug(X86-aplus)"
#else
#define SH_VERSION		"1.00-realese(X86-aplus)"
#endif

#define SH_LICENSE		"GPLv3+: GNU GPL version 3 o next\n\nThis is free software; you are free to change and redistribute it.\nThere is NO WARRANTY, to the extent permitted by law."
#define SH_COPYRIGHT	"2014 Antonio Natale"

static char sh_help[] = SH_NAME ", version " SH_VERSION "\n\n"
						"Uso:\tsh [options] ...\nOptions:\n\t--help\n\t--version\n\t-c command";
						
static char sh_version[] = SH_NAME ", version " SH_VERSION "\n\n"
							SH_LICENSE;
							
static inline int show_help() {
	printf("%s\n", sh_help);
	return 0;
}

static inline int show_version() {
	printf("%s\n", sh_version);
}

#endif
