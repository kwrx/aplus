#include <iostream>
#include <aplus/base.h>
#include <aplus/fbdev.h>
#include <aplus/gnx.h>
#include <aplus/kmem.h>
#include <aplus/sysconfig.h>

#include <cairo/cairo.h>

using namespace std;
using namespace Gnx;


int LibGnx::Width = -1;
int LibGnx::Height = -1;
int LibGnx::BitsPerPixel = -1;
int LibGnx::BytesPerPixel = -1;
string LibGnx::DevicePath = "";
vector<struct __gnx_context*>* LibGnx::Contexts = NULL;

int LibGnx::Initialize(string devicePath) {
    int ld = open("/dev/log", O_WRONLY);
    if(ld >= 0)
        dup2(ld, STDOUT_FILENO);


    fprintf(stdout, "LibGnx::Initialize(): Initialized Gnx Display %s\n", devicePath.c_str());

    LibGnx::Width = sysconfig("screen.width", SYSCONFIG_FORMAT_INT, -1);
    LibGnx::Height = sysconfig("screen.height", SYSCONFIG_FORMAT_INT, -1);
    LibGnx::BitsPerPixel = sysconfig("screen.bpp", SYSCONFIG_FORMAT_INT, -1);

    if(LibGnx::Width == -1 || LibGnx::Height == -1 || LibGnx::BitsPerPixel == -1) {
        fprintf(stdout, "LibGnx::Initialize(): Invalid settings in /etc/config\n");
        return -1;
    }

    LibGnx::BytesPerPixel = LibGnx::BitsPerPixel >> 3;
    LibGnx::DevicePath = devicePath;
    LibGnx::Contexts = new vector<struct __gnx_context*>();
}

void LibGnx::Close(int status) {
    while(!LibGnx::Contexts->empty())
        LibGnx::RemoveContext(LibGnx::Contexts->back());

    fclose(stderr);
    exit(status);
}

void LibGnx::AddContext(void* data) {
    int fd = open(LibGnx::DevicePath.c_str(), O_RDWR);
    if(fd < 0) {
        fprintf(stdout, "LibGnx::AddContext(): GNX Server's not running!\n");
        return;
    }

    write(fd, &data, sizeof(void*));
    close(fd);

    LibGnx::Contexts->push_back((struct __gnx_context*) data);
}

void LibGnx::RemoveContext(void* data) {
    int i;
    for(i = 0; i < LibGnx::Contexts->size(); i++) {
        if((struct __gnx_context*) data != (struct __gnx_context*) LibGnx::Contexts->at(i))
            continue;

        LibGnx::Contexts->erase(LibGnx::Contexts->begin() + i);
        break;
    }

    int fd = open(LibGnx::DevicePath.c_str(), O_RDWR);
    if(fd < 0) {
        fprintf(stdout, "LibGnx::AddContext(): GNX Server's not running!\n");
        return;
    }

    struct __gnx_context* gc = (struct __gnx_context*) data;
    gc->w =
    gc->h = 0;

    write(fd, &data, sizeof(void*));
    close(fd);
}