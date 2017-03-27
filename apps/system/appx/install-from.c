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


    
done:
    if(zip)
        zip_close(zip);

    appx_lck_release();
}