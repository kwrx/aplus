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
#include <glob.h>

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
	

	void show_content(char* d, char* p) {
		if(p[0] == '.')
			if(!show_hidden)
				return;
				
		if(!long_mode)
			printf("%s\n", p);
		else {
			char buf[MAXNAMLEN];
			sprintf(buf, "%s/%s", d, p);
			
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
				printf(" %s", p);
				
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
	
	void showdir(char* p) {
		struct stat st;
		if(stat(p, &st) != 0) {
			fprintf(stderr, "%s: %s: %s\n", argv[0], p, strerror(errno));
			return;
		}
		
		if(!(S_ISDIR(st.st_mode))) {
			show_content(".", p);
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
		while(ent = readdir(d))
			show_content(p, ent->d_name);
		
		if(print_dir)
			printf("\n");
			
		closedir(d);
	}
	
	if(optind >= argc || argc == 1)
		showdir(".");
	else
		while(optind++ < argc) {
			glob_t gl;
			if(glob(argv[optind - 1], GLOB_ERR, NULL, &gl) != 0
				|| gl.gl_pathc == 0) {
				fprintf(stderr, "%s: no such file or directory\n", argv[optind - 1]);
				globfree(&gl);
				continue;
			}

			if(gl.gl_pathc > 1)
				print_dir = 1;

			int j;
			for(j = 0; j < gl.gl_pathc; j++)
				showdir(gl.gl_pathv[j]);
			
			globfree(&gl);
		}
			
	
	return 0;
}