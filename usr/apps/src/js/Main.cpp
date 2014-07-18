//
//  Main.cpp
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

#include "TinyJS.h"
#include "TinyJS_Functions.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>



static void show_help() {
	printf("TinyJS interpreter.\n\tjs [filename] [options]\n");
	exit(0);
}


void js_print(CScriptVar *v, void *userdata) {
    printf("%s\n", v->getParameter("text")->getString().c_str());
}

void js_exit(CScriptVar* v, void* userdata) {
	exit(v->getParameter("status")->getInt());
}



int main(int argc, char **argv) {
	CTinyJS *js = new CTinyJS();
	registerFunctions(js);
	
	js->addNative("function print(text)", &js_print, 0);
	js->addNative("function exit(status)", &js_exit, 0);
	
	
	if(argc < 2)
		show_help();
	
	FILE* fp = fopen(argv[1], "rb");
	if(!fp)
		exit(-1);
		
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	void* core = malloc(size);
	memset(core, 0, size);
	
	fgets((char*) core, size, fp);
	fclose(fp);
	
	js->execute((const char*) core);

	free(core);
	delete js;

	return 0;
}

