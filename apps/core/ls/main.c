#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>

static void show_usage(int argc, char** argv) {
	printf (
		"ls - list files\n"
		"\n"
		"usage: %s [-lha] [path]\n"
		"\n"
		" -a     list all files (including . files)\n"
		" -l     use a long listing format\n"
		" -h     human-readable file sizes\n"
		" -?     show this help text\n"
		"\n", argv[0]
	);
}

int main(int argc, char** argv) {
	
	int show_hidden = 0;
	int human_readable = 0;
	int long_mode = 0;
	int print_dir = 0;
	
	
	if(argc > 1) {
		int i, c;
		while((c = getopt(argc, argv, "ahl?")) != -1) {
			switch(c) {
				case 'a':
					show_hidden = 1;
					break;
				case 'h':
					human_readable = 1;
					break;
				case 'l':
					long_mode = 1;
					break;
				case '?':
					show_usage(argc, argv);
					return 0;
			}
		}
		
		if(optind + 1 < argc)
			print_dir = 1;
	}
	
	
	void showdir(char* p) {	
		DIR* d = opendir(p);
		if(!d) {
			perror(p);
			return;
		}
		
		if(print_dir)
			printf("%s:\n", p);
		
		struct dirent* ent;
		while(ent = readdir(d)) {
			if(ent->d_name[0] == '.')
				if(!show_hidden)
					continue;
					
			if(!long_mode)
				printf("%s\n", ent->d_name);
			else {
				printf("%s\n", ent->d_name);
				/* TODO */		
			}
		}
		
		if(print_dir)
			printf("\n");
	}
	
	if(argc == 1)
		showdir(".");
	else
		while(optind++ < argc)
			showdir(argv[optind - 1]);
	
	return 0;
}