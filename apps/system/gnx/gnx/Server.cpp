#include <gnxserver.h>



Server::Server(uint16_t w, uint16_t h, uint16_t bpp, uintptr_t lfbptr)
: Screen.Width(w), Screen.Height(h), Screen.Bpp(bpp), Screen.Pitch(w * (bpp / 8)), Screen.Framebuffer(lfbptr) {
    this->Desktop = new WindowServer(this, NULL, w, h);
}


void Server::Run() {
    int fd = open("/tmp/gnx.lck", O_RDWR | O_CREAT | O_EXCL, S_IFREG | 0644);
    if(fd < 0) {
        if(errno == EEXIST) {
            cerr << "gnx: another instance is running!";
            exit(0);
        }

        perror("gnx");
        exit(-1);
    }

    pid_t pid = getpid();
    write(fd, &pid, sizeof(pid_t));
    close(fd);


    for(;;
        sched_yield()
    ) {
        static char buf[BUFSIZ];
        memset(buf, 0, sizeof(buf));
        
        if(msg_recv(&pid, buf, sizeof(buf)) <= 0)
            continue;

        
    }
}