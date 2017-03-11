#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#ifndef MAXNAMLEN
#define MAXNAMLEN	256
#endif

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
		int c;
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
		struct stat st;
		if(stat(p, &st) != 0) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], p, strerror(errno));
			return;
		}
		
		if(!(S_ISDIR(st.st_mode))) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], p, strerror(ENOTDIR));
			return;
		}
		
		DIR* d = opendir(p);
		if(!d) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], p, strerror(errno));
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
				char buf[MAXNAMLEN];
				sprintf(buf, "%s/%s", p, ent->d_name);
				
				struct stat st;
				if(lstat(buf, &st) != -1) {
					#define IS(x, y)								\
						if(S_IS##x (st.st_mode)) { printf(y); }
						
					IS(LNK, "l")
					else IS(CHR, "c")
					else IS(BLK, "b")
					else IS(DIR, "d")
					else
						printf("-");
						
					#undef IS
					#define IS(x, y, z)								\
						printf((st.st_mode & x) ? y : z)
						
										
					IS(S_IRUSR, "r", "-");
					IS(S_IWUSR, "w", "-");
					IS(S_ISUID, "s", (st.st_mode & S_IXUSR ? "x" : "-"));
					IS(S_IRGRP, "r", "-");
					IS(S_IWGRP, "w", "-");
					IS(S_IXGRP, "x", "-");
					IS(S_IROTH, "r", "-");
					IS(S_IWOTH, "w", "-");
					IS(S_IXOTH, "x", "-");
					
					#undef IS
					
					printf(" %d ", st.st_nlink);
					
					/* TODO: print username for uid and gid */
					
					if(human_readable) {
						register int s = st.st_size;
						if(s >= (1 << 20))
							printf("%d.%1dM", s / (1 << 20), (s - (s / (1 << 20)) * (1 << 20)) / ((1 << 20) / 10));
						else if(s >= (1 << 10))
							printf("%d.%1dK", s / (1 << 10), (s - (s / (1 << 10)) * (1 << 10)) / ((1 << 10) / 10));
						else
							printf("%d", st.st_size);	
					} else
						printf("%d", st.st_size);
						
						
					char timebuf[80];
					struct tm* tm = localtime(&st.st_mtime);
					strftime(timebuf, 80, "%b %d %H:%M", tm);
					
					printf(" %s", timebuf);
					printf(" %s", ent->d_name);
					
					if(S_ISLNK(st.st_mode)) {
						char linkbuf[MAXNAMLEN];
						if(readlink(buf, linkbuf, MAXNAMLEN) > 0) {
							printf(" -> %s", linkbuf);
						}
					}
					
					printf("\n");
				}	
			}
		}
		
		if(print_dir)
			printf("\n");
			
		closedir(d);
	}
	
	if(optind >= argc || argc == 1)
		showdir(".");
	else
		while(optind++ < argc)
			showdir(argv[optind - 1]);
	
	return 0;
}