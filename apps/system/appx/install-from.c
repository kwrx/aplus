#include "appx.h"

static char* sn(double n, double m) {
    if(n == 0)
        return "0";

    int i, j;
    for(i = 1, j = 0; n / i > m; j++)
        i *= m;

    if(j > 3)
        return "<overflow>";

    char buf[8];
    sprintf(buf, "%.3g%c", n / i, "\0kMG"[j]);

    return strdup(buf);
}

static void print_status(const char* package, const char* filename, int progress, int max) {
    char buf[21];
    memset(buf, 0, sizeof(buf));

    max = (int) (((double) progress / (double) max) * 100.0);

    int i, j;
    for(i = j = 0; i < max && j < 20; i += 5)
        buf[j++] = '#';

    const char* p = filename;
    i = strlen(filename);
    for(; i >= 0; i--) {
        if(filename[i] != '/')
            continue;
        p = &filename[i + 1];
        break;
    }

    fprintf(stdout, "\r%-20s %30s: [%-20s] %2d%%", package, p, buf, max);
    fflush(stdout);
}

void appx_install_from(const char* filename) {
    appx_lck_acquire();

   
    zip_t* zip = zip_open(filename, ZIP_RDONLY, NULL);
    if(unlikely(!zip)) {
        fprintf(stderr, "appx: %s\n", zip_strerror(zip));
        goto done;
    }


    long size = 0;
    long count = 0;

    if(verbose)
        fprintf(stdout, "Content:\n");
    
    int i;
    for(i = 0; i < zip_get_num_entries(zip, 0); i++) {
        struct zip_stat st;
        if(zip_stat_index(zip, i, 0, &st) != 0) {
            fprintf(stderr, "appx: %s\n", zip_strerror(zip));
            goto done;
        }

        if(st.name[strlen(st.name) - 1] == '/')
            continue;


        if(verbose)
            fprintf(stdout, " -> %s\n", st.name);

        count++;
        size += (long) st.size;
    }



    if(verbose)
        fprintf(stdout, "\nWill be installed %sB of data\n", sn(size, 1024));
    
    if(!yes) {
        if(verbose)
            fprintf(stdout, "Do you want to install? (y/n): ");

        char ch;
        scanf("%c", &ch);

        if(ch != 'y')
            goto done;
    }

    int progress = 0;
    static char buf[4096 * 1024];

    for(i = 0; i < zip_get_num_entries(zip, 0); i++) {
        struct zip_stat st;
        if(zip_stat_index(zip, i, 0, &st) != 0) {
            fprintf(stderr, "appx: %s\n", zip_strerror(zip));
            goto done;
        }

        if(st.name[strlen(st.name) - 1] == '/') {
            char* p = (char*) st.name;
            p[strlen(st.name) - 1] = '\0';

            #define mkdir(x, y) \
                close(open(x, O_CREAT | O_RDONLY, S_IFDIR | y)) /* FIXME */

            if(mkdir(p, 0644) != 0) {
                perror(st.name);
                continue;
            }
            continue;
        }

        FILE* fp = fopen(st.name, "w");
        if(!fp) {
            perror(st.name);
            goto done;
        }    
        
        zip_file_t* fz = zip_fopen(zip, st.name, 0);
        if(!fz) {
            fprintf(stderr, "%s(%s): %s\n", filename, st.name, zip_strerror(zip));
            goto done;
        }



        print_status(filename, st.name, progress, size);
        
        int e;
        while((e = zip_fread(fz, buf, sizeof(buf))) > 0) {
            if(fwrite(buf, 1, e, fp) != e) {
                perror(st.name);
                goto done;
            }

            progress += e;
            print_status(filename, st.name, progress, size);
        }

        zip_fclose(fz);
        fclose(fp);
    }
    
done:
    if(zip)
        zip_close(zip);

    appx_lck_release();
}